#include <stdio.h> // for Terminal Output
#include <stdlib.h> // for RandNumGen
#define INA219_ADDRESS1 (0x40<<1)
#define INA219_ADDRESS2 (0x41<<1)

// Prescale values to bring 100 MHz clock down to 1 MHz
// All count from 1-65535 (16-bit timers) in Up mode
// const unsigned long prescale_sampling = 89; // TIM3
// const unsigned long period = 999; // TIM3


typedef struct task{
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*Function) (int);
} task;


enum Smp_states {SAMP_STATE};
enum Send_states {SEND_STATE};
enum Relay_states {WAIT_STATE,RELAY_STATE};

const unsigned long numTasks = 3;
const unsigned long sample_period = 1; // 1 KHz sampling based on 1 MHz timer
const unsigned long send_period = 500; // 0.5 s on 1 MHz timer
const unsigned long relay_period = 1500; // 1.5 s

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

float ina_avg[2]; // store averages over sample periods
float ina1_sum = 0; // sum values over sample period
float ina2_sum = 0;
unsigned long sample_count = 0; // counters
unsigned long relay_counter = 0;

task tasks[3]; // task array for scheduler

INA219_t ina1, ina2; // sensor objects for sensor driver code
uint8_t uart_buffer[64]; // uart character buffer
volatile int uart_busy = 0; // uart busy flag

int SampleSensors(int state){
    float current1 = INA219_ReadCurrent_mA(&ina1); // read current from sensor one (address 0x40)
    float current2 = INA219_ReadCurrent_mA(&ina2); // read current from sensor two (address 0x41)
    ina1_sum += current1;
    ina2_sum += current2;
    sample_count++;
    if(sample_count >= 500){ // average 500 values at a time
        ina_avg[0] = ina1_sum / 500.0f;
        ina_avg[1] = ina2_sum / 500.0f;
        ina1_sum = 0;
        ina2_sum = 0;
        sample_count = 0;
    }
    return state;
}
int SendVal(int state){
    if(!uart_busy){ // send if uart is not busy
        uart_busy = 1;
        int len = sprintf((char*)uart_buffer, "%.3f, %.3f\n", ina_avg[0], ina_avg[1]);
        HAL_UART_Transmit_IT(&huart2, uart_buffer, len);
    }
    return state;
}


int Relay(int state){
    switch(state){ // MOSFET state transitions
        case WAIT_STATE: state = RELAY_STATE; break;
        case RELAY_STATE: state = WAIT_STATE; break;
    }

    switch(state){ // MOSFET GPIO state actions
        case WAIT_STATE:
            HAL_GPIO_WritePin(RELAY_OUT_GPIO_Port, RELAY_OUT_Pin, GPIO_PIN_RESET); // write low to gpio pin
            break;
        case RELAY_STATE:
            HAL_GPIO_WritePin(RELAY_OUT_GPIO_Port, RELAY_OUT_Pin, GPIO_PIN_SET); // write high to gpio pin
            relay_counter = 0; // reset counter value
            break;
    }

    relay_counter++;
    return state;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){ // increments elapsed time for each task each timer tick
    if(htim->Instance == TIM3){
        for(int i = 0; i < numTasks; i++){
            tasks[i].elapsedTime++;
        }
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){ // reset uart busy flag if uart is available
    if(huart->Instance == USART2){
        uart_busy = 0;
    }
}

int main(void)
{
  HAL_Init(); // hardware configuration 
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();

  INA219_Init(&ina1, &hi2c1, INA219_ADDRESS1, 0.1f); // sensor object configuration
  INA219_Init(&ina2, &hi2c1, INA219_ADDRESS2, 0.1f);

  tasks[0].state = SAMP_STATE; // initialize task 0 (sensor sampling)
  tasks[0].period = sample_period;
  tasks[0].elapsedTime = sample_period;
  tasks[0].Function = &SampleSensors;

  tasks[1].state = SEND_STATE; // initialize task 1 (sending values through uart)
  tasks[1].period = send_period;
  tasks[1].elapsedTime = send_period;
  tasks[1].Function = &SendVal;

  tasks[2].state = WAIT_STATE; // initialize task 2 (MOSFET LED circuit)
  tasks[2].period = relay_period;
  tasks[2].elapsedTime = relay_period;
  tasks[2].Function = &Relay;

  HAL_TIM_Base_Start_IT(&htim3); // start timer 3
    
  while (1) // event loop
  {
      for(int i = 0; i < numTasks; i++){ // iterate through task array, update state by calling task function with current state
          if(tasks[i].elapsedTime >= tasks[i].period){
              tasks[i].state = tasks[i].Function(tasks[i].state);
              tasks[i].elapsedTime = 0;
          }
      }
  }
}


