#include "RIMS.h"

typedef struct task{
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*Function) (int);
} task;

const unsigned int numTasks = 2;
const unsigned long period = 100;
const unsigned long BL_period = 1500;
const unsigned long TL_period = 500;

task tasks[2];

enum BL_states {BL0, BL1};
int BlinkLED(int state){
    switch(state){
        case BL0:
            B0 = 0;
            state = BL1;
            break;
        case BL1:
            B0 = 1;
            state = BL0;
            break;
    }
    return state;
}

enum TL_states {TL0, TL1, TL2};
int ThreeLED(int state){
    switch(state){
        case TL0:
            B = (B & 0x01) | 0x80;
            state = TL1;
            break;
        case TL1:
            B = (B & 0x01) | 0x40;
            state = TL2;
            break;
        case TL2:
            B = (B & 0x01)| 0x20;
            state = TL0;
            break;
    }
    return state;
}

void TimerISR(){
    unsigned char i;
    for(i = 0; i < numTasks; i++){
        if(tasks[i].elapsedTime >= tasks[i].period){
            tasks[i].state = tasks[i].Function (tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += period;
    }
}

void main(){
    tasks[0].state = BL0;
    tasks[0].period = BL_period;
    tasks[0].elapsedTime = BL_period;
    tasks[0].Function = &BlinkLED;

    tasks[1].state = TL0;
    tasks[1].period = TL_period;
    tasks[1].elapsedTime = TL_period;
    tasks[1].Function = &ThreeLED;

    TimerSet(period);
    TimerOn();
    while(1);
}
