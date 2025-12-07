#include "main.h"
#include <stdio.h> // for Terminal Output
#include <stdlib.h> // for RandNumGen
#define INA219_ADDRESS1 (0x40<<1)
#define INA219_ADDRESS2 (0x41<<1)
// enable clocks and GPIO


// Prescale values to bring 100 MHz clock down to 1 MHz
// All count from 1-65535 (16-bit timers) in Up mode
// const unsigned long prescale_sampling = 99; // TIM9, 2 capture/compare channels
// const unsigned long prescale_sending = 999; // TIM10, 1 capture/compare channel, 100 KHz
// const unsigned long prescale_relay = 999; // TIM11, 1 capture/compare channel


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
const unsigned long send_period = 500; // 0.5 s on 100 KHz timer
const unsigned long relay_period = 1500; // 1.5 s

float ina_avg[2];
float ina1_sum = 0;
float ina2_sum = 0;
unsigned long sample_count = 0;
unsigned long relay_counter = 0;

task tasks[3];

extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim11;

INA219_t ina1, ina2;


int SampleSensors(int state){
	float current1 = INA219_ReadCurrent(&ina1);
	float current2 = INA219_ReadCurrent(&ina2);
	ina1_sum = ina1_sum + current1;
	ina2_sum = ina2_sum + current2;
	sample_count = sample_count + 1;
    if(sample_count >= 500){
    	ina_avg[0] = ina1_sum / 500.0f;
    	ina_avg[1] = ina2_sum / 500.0f;
    	ina1_sum = 0;
    	ina2_sum = 0;
    	sample_count = 0;
    }
    return state;
}

int SendVal(int state){
	char buffer[64];
	for(int i = 0; i < 2; i++){
		int len = sprintf(buffer, "%.3f\n", ina_avg[i]);
		HAL_UART_Transmit(&huart2, (uint8_t*)buffer, len, HAL_MAX_DELAY);
	}
	return state;
}

int Relay(int state){
	switch(state){
		case WAIT_STATE:
			if(relay_counter >= 3){
				state = RELAY_STATE;
			}
			break;
		case RELAY_STATE:
			state = WAIT_STATE;
			break;
	}

	switch(state){
		case WAIT_STATE:
            HAL_GPIO_WritePin(RELAY_OUT_GPIO_Port, RELAY_OUT_Pin, GPIO_PIN_RESET);
			break;
		case RELAY_STATE:
            HAL_GPIO_WritePin(RELAY_OUT_GPIO_Port, RELAY_OUT_Pin, GPIO_PIN_SET);
			relay_counter = 0;
			break;
	}

	relay_counter = relay_counter + 1;
	return state;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance == TIM11){
		for(int i = 0; i < numTasks; i++){
			tasks[i].elapsedTime = tasks[i].elapsedTime + 1;
		}
	}
}

int main(){
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_I2C1_Init();
	MX_TIM11_Init();

	INA219_Init(&ina1, &hi2c1, INA219_ADDRESS1);
	INA219_Init(&ina2, &hi2c1, INA219_ADDRESS2);

	tasks[0].state = SAMP_STATE;
	tasks[0].period = sample_period;
	tasks[0].elapsedTime = sample_period;
	tasks[0].Function = &SampleSensors;

	tasks[1].state = SEND_STATE;
	tasks[1].period = send_period;
	tasks[1].elapsedTime = send_period;
   	tasks[1].Function = &SendVal;

   	tasks[2].state = WAIT_STATE;
   	tasks[2].period = relay_period;
    tasks[2].elapsedTime = relay_period;
    tasks[2].Function = &Relay;

    HAL_TIM_Base_Start_IT(&htim11);

    while(1){
    	for(int i = 0; i < numTasks; i++){
    		if(tasks[i].elapsedTime >= tasks[i].period){
    			tasks[i].state = tasks[i].Function(tasks[i].state);
    			tasks[i].elapsedTime = 0;
    		}
    	}
    }
    return 1;
}
