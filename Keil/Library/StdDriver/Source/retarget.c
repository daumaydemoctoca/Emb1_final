#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h" 
#include "LCD.h" 
#include "string.h"
#include "timer.h"

#define TTL_PULSE_DURATION 10
#define SCAN_PERIOD 500000

#define ALARM_VERY_HIGH_FREQ_TIMER_CMP 50000
#define ALARM_HIGH_FREQ_TIMER_CMP 100000
#define ALARM_NORMAL_FREQ_TIMER_CMP 200000
#define ALARM_LOW_FREQ_TIMER_CMP 500000

void System_Config(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void SPI3_Config(void);
void Delay_us(uint32_t us);

void TMR0_IRQHandler(void)
{
    if(TIMER_GetIntFlag(TIMER0) == 1)
    {
        /* Clear Timer0 time-out interrupt flag */
        TIMER_ClearIntFlag(TIMER0);
				PC -> DOUT ^= 1 << 12;
				PB -> DOUT ^= 1 << 11;
    }
}

int main(void) {
		char duration_txt[]= "0";
		uint16_t distance = 0;

		System_Config(); 

    SPI3_Config();
		LCD_start();
		clear_LCD();
		
		// The 4 internal LEDs
		GPIO_SetMode(PC, BIT12, GPIO_MODE_OUTPUT);
		GPIO_SetMode(PB, BIT11, GPIO_MODE_OUTPUT);
		
		
		// The radar
		GPIO_SetMode(PC, BIT0, GPIO_MODE_OUTPUT); // trigger 
		GPIO_SetMode(PC, BIT1, GPIO_MODE_INPUT); // echo 
	
		// Configure timer 2 - used to count the duration the echo is high
		CLK->CLKSEL1 &= ~(0x07ul << 16);     
		CLK->CLKSEL1 |= (0x02ul << 16);  // Set the clock source is HCLK   
		CLK->APBCLK |= (0x01ul << 4);     // Enable the clock supply
		
		// setup timer 2
		TIMER_SET_PRESCALE_VALUE(TIMER2, 49); // 49       

		//define Timer 2 operation mode 
		TIMER2->TCSR &= ~(0x03ul << 27);     
		TIMER2->TCSR |= (0x01ul << 27);     // Periodic
		TIMER2->TCSR &= ~(0x01ul << 24); // Disable the timer counter mode (input/event)     

		//TDR to be updated continuously while timer counter is counting     
		TIMER2->TCSR |= (0x01ul << 16);   // Enable the Data Load       
				
		TIMER_SET_CMP_VALUE(TIMER2, 0xFFFFFF);
		//start counting     
		//Timer 2 initialization end---------------- 
		
		// Configure timer 0 - used to set LED and buzzer freq
		CLK->CLKSEL1 &= ~(0x07ul << 8);
		CLK->CLKSEL1 |= (0x02ul << 8);  // Set the clock source is HCLK
		CLK->APBCLK |= (0x01ul << 2);
		
		// setup timer 0
		TIMER_SET_PRESCALE_VALUE(TIMER0, 49); // 49  
		
		//define Timer 0 operation mode 
		TIMER0->TCSR &= ~(0x03ul << 27);     
		TIMER0->TCSR |= (0x01ul << 27);     // Periodic
		TIMER0->TCSR &= ~(0x01ul << 24); // Disable the timer counter mode (input/event) 
		
		//TDR to be updated continuously while timer counter is counting     
		TIMER0->TCSR |= (0x01ul << 16);   // Enable the Data Load       
		TIMER_EnableInt(TIMER0);
		NVIC_EnableIRQ(TMR0_IRQn);
		
		TIMER_Start(TIMER0);	
		// Timer 0 init end
	
		printS_5x7(45, 0, "EEET2481"); 
		printS_5x7(2, 8, "Distance Measurement Sys."); 
		printS_5x7(0, 32, "Current Distance (cm):"); 
		
    while (1) { 
				PC -> DOUT &= ~(1 << 0); // turn trigger off
				Delay_us(2);
				PC -> DOUT |= (1 << 0); // turn trigger on
				Delay_us(TTL_PULSE_DURATION);
				PC -> DOUT &= ~(1 << 0); // turn trigger off
			
			
				while(!PC1); // wait for echo to go high
				TIMER2->TCSR |= (0x01ul << 26);
				TIMER_Start(TIMER2);
			
				while(PC1); // wait for echo to go low
				TIMER_Stop(TIMER2);

				distance = TIMER2->TDR / 58;
				
				TIMER_SET_CMP_VALUE(TIMER0, 200000);
			
				if (distance >= 40) TIMER_SET_CMP_VALUE(TIMER0, ALARM_LOW_FREQ_TIMER_CMP);
				else if (distance >= 25) TIMER_SET_CMP_VALUE(TIMER0, ALARM_NORMAL_FREQ_TIMER_CMP);
				else if (distance >= 10) TIMER_SET_CMP_VALUE(TIMER0, ALARM_HIGH_FREQ_TIMER_CMP);
				else TIMER_SET_CMP_VALUE(TIMER0, ALARM_VERY_HIGH_FREQ_TIMER_CMP); 
				
				sprintf(duration_txt, "%04d", distance);
				printS_5x7(64 - (5 * strlen(duration_txt)) / 2, 48, duration_txt); 
				
				Delay_us(SCAN_PERIOD);
    } 
}

void Delay_us(uint32_t us) {
  uint32_t i;
	uint32_t j;
  for (i = 0; i < us; ++i) {
		for (j = 0; j < 8; ++j) {}
	}
}

void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers
    CLK -> PWRCON |= (0x01ul << 0);
    while (!(CLK -> CLKSTATUS & (1ul << 0)));

    //PLL configuration starts 50 Mhz
    CLK -> PLLCON &= ~(1ul << 19); //0: PLL input is HXT
    CLK -> PLLCON &= ~(1ul << 16); //PLL in normal mode
    CLK -> PLLCON &= (~(0x01FFul << 0));
    CLK -> PLLCON |= 48;
    CLK -> PLLCON &= ~(1ul << 18); //0: enable PLLOUT
    while (!(CLK -> CLKSTATUS & (0x01ul << 2)));
    //PLL configuration ends
    //clock source selection
    CLK -> CLKSEL0 &= (~(0x07ul << 0));
    CLK -> CLKSEL0 |= (0x02ul << 0); //clock frequency division
    CLK -> CLKDIV &= (~0x0Ful << 0);
    //enable clock of SPI3
    CLK -> APBCLK |= 1ul << 15;
    SYS_LockReg(); // Lock protected registers
}

void LCD_start(void) {
    LCD_command(0xE2); // Set system reset
    LCD_command(0xA1); // Set Frame rate 100 fps
    LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)
    LCD_command(0x81); // Set V BIAS potentiometer
    LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()
    LCD_command(0xC0);
    LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp) {
    SPI3 -> SSR |= 1ul << 0;
    SPI3 -> TX[0] = temp;
    SPI3 -> CNTRL |= 1ul << 0;
    while (SPI3 -> CNTRL & (1ul << 0));
    SPI3 -> SSR &= ~(1ul << 0);
}

void SPI3_Config(void) {
    SYS -> GPD_MFP |= 1ul << 11; //1: PD11 is configured for alternative function
    SYS -> GPD_MFP |= 1ul << 9; //1: PD9 is configured for alternative function
    SYS -> GPD_MFP |= 1ul << 8; //1: PD8 is configured for alternative function
    SPI3 -> CNTRL &= ~(1ul << 23); //0: disable variable clock feature

    SPI3 -> CNTRL &= ~(1ul << 22); //0: disable two bits transfer mode
    SPI3 -> CNTRL &= ~(1ul << 18); //0: select Master mode
    SPI3 -> CNTRL &= ~(1ul << 17); //0: disable SPI interrupt
    SPI3 -> CNTRL |= 1ul << 11; //1: SPI clock idle high
    SPI3 -> CNTRL &= ~(1ul << 10); //0: MSB is sent first
    SPI3 -> CNTRL &= ~(3ul << 8); //00: one transmit/receive word will be executed in one data transfer
    SPI3 -> CNTRL &= ~(31ul << 3); //Transmit/Receive bit length
    SPI3 -> CNTRL |= 9ul << 3; //9: 9 bits transmitted/received per data transfer
    SPI3 -> CNTRL |= (1ul << 2); //1: Transmit at negative edge of SPI CLK
    SPI3 -> DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}
