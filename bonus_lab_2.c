#include <string.h>
#include <stdio.h>
#include "stm32l476xx.h"
#include "SysClock.h"
#include "I2C.h"
#include "ssd1306.h"
#include "ssd1306_tests.h"
#include <math.h>


void RTC_Clock_Enable()
{
	if ((RCC->APB1ENR1 & RCC_APB1ENR1_PWREN) == 0) {
		RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;//enable power interface clock
		(void) RCC->APB1ENR1;//short delay
	}
	
	if ((PWR->CR1 & PWR_CR1_DBP) == 0) {
		PWR->CR1 |= PWR_CR1_DBP;//enable write access in backup domain
		while ((PWR->CR1 & PWR_CR1_DBP) == 0);//wait for access
	}
	//reset LSEON and LSEBYP bits before configuring LSE
	
	RCC->BDCR &= ~(RCC_BDCR_LSEON | RCC_BDCR_LSEBYP);
	
	//change the backup domain if reset
	RCC->BDCR |= RCC_BDCR_BDRST;
	RCC->BDCR &= ~RCC_BDCR_BDRST;
	
	//Wait for LSE clock to be ready
	
	while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0) {
		RCC->BDCR |= RCC_BDCR_LSEON; //Enable LSE oscilator
	}
	
	//select LSE as RTC clock source
	RCC->BDCR &= ~RCC_BDCR_RTCSEL;
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;
	
	//disable power interface clock
	RCC->APB1ENR1 &= ~RCC_APB1ENR1_PWREN;
	
	//enable the RTC clock
	RCC->BDCR |= RCC_BDCR_RTCEN;
}

void RTC_INIT()
{
	RTC_Clock_Enable();
	
	//disable write protection of RTC registers
	RTC->WPR = 0xCA;
	RTC->WPR = 0x53;
	
	
	//enter init mode to program TR and DR registers
	RTC->ISR |= RTC_ISR_INIT;
	
	//wait for INITF to change
	while ((RTC->ISR & RTC_ISR_INITF) == 0);
	
	//hour format 0=24hour, 1=am/pm
	RTC->CR &= ~RTC_CR_FMT;
	
	//generate a 1-Hz clock for the RTC time counter
	RTC->PRER |= ((1U<<7) - 1)<<16; //ASync_Prescaler=127
	RTC->PRER |= ((1U<<8) - 1); //Sync_Prescaler=255
	
	//set time to 11:32:00 am
	RTC->TR = 0U<<22 | 1U<<20 | 1U<<16 | 3U<<12 | 2U<<8;
	//set date to 2018/03/15
	RTC->DR = 1U<<20 | 8U<<16 | 0U<<12 | 3U<<8 | 1U<<4 | 5U;
	
	//exit initialization mode
	RTC->ISR &= ~RTC_ISR_INIT;
	
	//enable write protection for RTC registers
	RTC->WPR = 0xFF;
}
void Systick_initialize(uint32_t ticks){
	SysTick->CTRL=0;
	SysTick->LOAD=ticks-1;
	NVIC_SetPriority (SysTick_IRQn,(1<<__NVIC_PRIO_BITS)-1);
	SysTick->VAL=0;
	SysTick->CTRL|=SysTick_CTRL_CLKSOURCE_Msk;
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk;
}
volatile int32_t TimeDelay;
void SysTick_Handler(void){
	if(TimeDelay>>0)
		TimeDelay--;
}
void Delay(uint32_t nTime){
	TimeDelay=nTime;
	while(TimeDelay!=0);
}
int Timermath(int ms){
	return (ms*50<<6)-1;
}
void Delays(int ms){
	Systick_initialize(Timermath(ms));
	Delay(ms);
}
int main()
{
	System_Clock_Init(); // Switch System Clock = 80 MHz
	I2C_GPIO_init();
	I2C_Initialization(I2C1);
	ssd1306_Init();
	
	RTC_INIT();
	double x,y,a,b,c,d;
	while(1){
		char time[64] = "";
		char PM=(RTC->TR&RTC_TR_PM)>>22;
		char HT=(RTC->TR&RTC_TR_HT)>>20;
		char HU=(RTC->TR&RTC_TR_HU)>>16;
		char MNT=(RTC->TR&RTC_TR_MNT)>>12;
		char MNU=(RTC->TR&RTC_TR_MNU)>>8;
		char ST=(RTC->TR&RTC_TR_ST)>>4;
		char SU=RTC->TR&RTC_TR_SU;
		
		char date[64] = "";
		char YT=(RTC->DR&RTC_DR_YT)>>20;
		char YU=(RTC->DR&RTC_DR_YU)>>16;
		char MT=(RTC->DR&RTC_DR_MT)>>12;
		char MU=(RTC->DR&RTC_DR_MU)>>8;
		char DT=(RTC->DR&RTC_DR_DT)>>4;
		char DU=RTC->DR&RTC_DR_DU;
		ssd1306_Fill(White);
		ssd1306_DrawCircle(64,32,32,Black); //Clock Circle
		ssd1306_SetCursor(60,2);
		ssd1306_WriteString("12", Font_6x8, Black); //Clock Numbers
		ssd1306_SetCursor(88,29);
		ssd1306_WriteString("3", Font_6x8, Black);
		ssd1306_SetCursor(62,56);
		ssd1306_WriteString("6", Font_6x8, Black);
		ssd1306_SetCursor(35,29);
		ssd1306_WriteString("9", Font_6x8, Black);
		ssd1306_SetCursor(50,40);
		sprintf(date,"%d%d/%d%d",MT,MU,DT,DU); //Date on clock
		ssd1306_WriteString(date, Font_6x8, Black);
		char sec=(ST*10)+SU;
		char hour=(HT*10)+HU;
		char min=(10*MNT)+MNU;
		
		x=64+27*sin((3.14159265/180)*(6*sec));//Second Hand
		y=32-27*cos((3.14159265/180)*(6*sec));
		ssd1306_Line(64,32,x,y,Black);
		c=64+17*sin((3.14159265/180)*(30*hour));//Hour Hand
		d=32-17*cos((3.14159265/180)*(30*hour));
		ssd1306_Line(64,32,c,d,Black);
		a=64+22*sin((3.14159265/180)*(6*min));//Minute Hand
		b=32-22*cos((3.14159265/180)*(6*min));
		ssd1306_Line(64,32,a,b,Black);
		
		ssd1306_UpdateScreen();
		Delays(10);
}
	while(1);//deadloop
}