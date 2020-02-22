#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include "stm32f4xx.h"

void TimingDelay_Decrement(void);

void SysTick_Init(void);
               
void Delay_ms(__IO u32 nTime);
#define Delay_10ms(x) Delay_ms(10*x)

#endif /* __SYSTICK_H */
