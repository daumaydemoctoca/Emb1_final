
#include <stdio.h>
#include <stdlib.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h" 
#include "LCD.h" 
#include "string.h"
#include "Draw2D.h"
void Delay_ms(uint32_t ms);
void System_Config(void);
void SPI3_Config(void);

void LCD_start(void);
uint8_t KeyPadScanning(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
void KeyPadEnable(void);
void EINT1_IRQHandler(void);
void Interrupt_Config(void);
int input_number(int input[6], int i);

uint8_t key_pressed, last_key_pressed = 0;

typedef enum {welcome_screen, input_screen, change_screen, wrong_screen, correct_screen, success_screen} STATES;

STATES lock_state = welcome_screen;

int main(void)
{
	//lock_state = welcome_screen;
	uint8_t i= 0;
	
	int key[6] = {1,1,1,1,1,1};
	int input[6];
	System_Config();
	Interrupt_Config();
	KeyPadEnable();
	SPI3_Config();  
	LCD_start();
	clear_LCD();

	while (1) {
		// label for switch
		LOOP:switch(lock_state){
			case welcome_screen:
				clear_LCD();
				printS_5x7(1, 0, "EEET2481-Door Lock System");
				printS_5x7(40, 24, "_ _ _ _ _ _");
				printS_5x7(32, 42, "Please select: ");
				printS_5x7(1, 48, "1: Unlock   2: Change key");
			
				while(key_pressed == 0) key_pressed = KeyPadScanning();
				// debounce
				Delay_ms(200);	
				clear_LCD();
				// change state
				if (key_pressed == 1) {
					lock_state =  input_screen;
				}
				else if (key_pressed == 2) {
					lock_state = change_screen;
				}
				
				key_pressed = 0;
				break;
		
			case input_screen:
				printS_5x7(1, 0, "EEET2481-Door Lock System");
				printS_5x7(12, 12, "Please input your key!");
				printS_5x7(40, 24, "_ _ _ _ _ _");
				
				//read 6 digit
				for (i = 0; i < 6 ; ++i){
						last_key_pressed = 0;
						key_pressed = 0;
						while(1)  {
							if (lock_state != input_screen) {
								goto LOOP;
							}
							if (input_number(input, i) == 0){break;}
						}
				}
				// compare input value to password
				for(i = 0; i < 6 ; i++){
					if(input[i] != key[i]) {
						lock_state = wrong_screen;
						break;
					}
					lock_state = correct_screen;	
				}
				clear_LCD();
				break;
		
			case change_screen:
				
				printS_5x7(0, 0, "EET2481- Door Lock System");
				printS_5x7(14, 12, "Please input new key");
				printS_5x7(40, 24, "_ _ _ _ _ _");
				
				//read 6 digit
				for (i = 0; i < 6 ; ++i){
						last_key_pressed = 0;
						key_pressed = 0;
						while(1)  {
							if (lock_state != change_screen) goto LOOP;
							if (input_number(input, i) == 0){break;}
						}
				}
				// pass input value to password
				for (i = 0; i < 6 ; ++i){
					key[i] = input[i];
				}
				lock_state = success_screen;	
				clear_LCD();
				break;
		
			case correct_screen:
				printS_5x7(0, 0, "EET2481- Door Lock System");
				printS_5x7(1, 16, "Welcome Home");
				// Scan key pressed
				while(key_pressed==0){ 
					key_pressed = KeyPadScanning();
					if (lock_state != correct_screen) {
						goto LOOP;
					}
				} 
				Delay_ms(200);
				key_pressed=0;
				clear_LCD();
				lock_state = welcome_screen;	
				break;
	 
			case wrong_screen:
				
				printS_5x7(0, 0, "EET2481- Door Lock System");
				printS_5x7(24, 16, "The key is wrong");
				printS_5x7(1, 32, "System restarts in 1 second");
				printS_5x7(40, 48, "Thank you");
				Delay_ms(1000);
				clear_LCD();
				lock_state = welcome_screen;	
				key_pressed=0;
				break;
		
			case success_screen:
				printS_5x7(0, 0, "EET2481- Door Lock System");
				printS_5x7(1, 16, "Your key has been changed");
				printS_5x7(40, 24, "* * * * * *");
				printS_5x7(40, 48, "THANK YOU!");
				Delay_ms(1000);
				key_pressed=0;
				clear_LCD();
				lock_state = welcome_screen;	
				break;
			default: break;
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------------
// Functions definition
//------------------------------------------------------------------------------------------------------------------------------------

void reset_input(char input_txt[6]) {
	uint8_t i;
	for (i = 0; i < 6; ++i) {
		input_txt[i] = '_';
	}
}

void LCD_start(void)
{
    LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps  
    LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)  
    LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()        	
    LCD_command(0xC0);  
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
  SPI3->SSR |= 1ul << 0;  
  SPI3->TX[0] = temp;
  SPI3->CNTRL |= 1ul << 0;
  while(SPI3->CNTRL & (1ul << 0));
  SPI3->SSR &= ~(1ul << 0);
}

void LCD_data(unsigned char temp)
{
  SPI3->SSR |= 1ul << 0;  
  SPI3->TX[0] = 0x0100+temp;
  SPI3->CNTRL |= 1ul << 0;
  while(SPI3->CNTRL & (1ul << 0));
  SPI3->SSR &= ~(1ul << 0);
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
  LCD_command(0xB0 | PageAddr);
  LCD_command(0x10 | (ColumnAddr>>4)&0xF); 
  LCD_command(0x00 | (ColumnAddr & 0xF));
}

void KeyPadEnable(void){
    GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI); 
    GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
    GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
    }

uint8_t KeyPadScanning(void){
    PA0=1; PA1=1; PA2=0; PA3=1; PA4=1; PA5=1;
    if (PA3==0) return 1;
    if (PA4==0) return 4;
    if (PA5==0) return 7;
    PA0=1; PA1=0; PA2=1; PA3=1; PA4=1; PA5=1;
    if (PA3==0) return 2;
    if (PA4==0) return 5;
    if (PA5==0) return 8;
    PA0=0; PA1=1; PA2=1; PA3=1; PA4=1; PA5=1;
    if (PA3==0) return 3;
    if (PA4==0) return 6;
    if (PA5==0) return 9;	
    return 0;
    }

void System_Config (void){
    SYS_UnlockReg(); // Unlock protected registers
    CLK -> PWRCON |= (0x01ul << 0);
    while (!(CLK -> CLKSTATUS & (1ul << 0)));

    //PLL configuration starts
    CLK -> PLLCON &= ~(1ul << 19); //0: PLL input is HXT
    CLK -> PLLCON &= ~(1ul << 16); //PLL in normal mode
    CLK -> PLLCON &= (~(0x01FFul << 0));
    CLK -> PLLCON |= 46;
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
    
void SPI3_Config (void){
    SYS->GPD_MFP |= 1ul << 11; //1: PD11 is configured for alternative function
    SYS->GPD_MFP |= 1ul << 9; //1: PD9 is configured for alternative function
    SYS->GPD_MFP |= 1ul << 8; //1: PD8 is configured for alternative function
  
    SPI3->CNTRL &= ~(1ul << 23); //0: disable variable clock feature
    SPI3->CNTRL &= ~(1ul << 22); //0: disable two bits transfer mode
    SPI3->CNTRL &= ~(1ul << 18); //0: select Master mode
    SPI3->CNTRL &= ~(1ul << 17); //0: disable SPI interrupt    
    SPI3->CNTRL |= 1ul << 11; //1: SPI clock idle high 
    SPI3->CNTRL &= ~(1ul << 10); //0: MSB is sent first   
    SPI3->CNTRL &= ~(3ul << 8); //00: one transmit/receive word will be executed in one data transfer
   
    SPI3->CNTRL &= ~(31ul << 3); //Transmit/Receive bit length
    SPI3->CNTRL |= 9ul << 3;     //9: 9 bits transmitted/received per data transfer
    
    SPI3->CNTRL |= (1ul << 2);  //1: Transmit at negative edge of SPI CLK       
    SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
 }
 
 void Delay_ms(uint32_t ms) {
  uint32_t i;
	uint32_t j;
  for (i = 0; i < ms; i++) {
		for (j=0; j <9600; j++){}
	}
}

void Interrupt_Config(void) {
	GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT); // The switch
	GPIO_SetMode(PC, BIT12, GPIO_MODE_OUTPUT); 
	//enable debouncing function
	GPIO_ENABLE_DEBOUNCE(PB, BIT15);  //enable the debounce function
	GPIO_SET_DEBOUNCE_TIME(GPIO_DBCLKSRC_LIRC, GPIO_DBCLKSEL_64); //configure debounce 
	//de-bounce counter clock source is 10kHz, n = 6, -> sample cycles = 2^6 =64
	//=> (64)*(1/(10*1000)) s = 64*0.0001 s = 6.4 ms
	//GPIO config initialization end--------------	
	
	//GPIO interupt initialization start--------------
	//GPIO Interrupt configuration. GPIO-B15 is the interrupt source
	PB->IMD &= (~(1ul << 15));
	PB->IEN |= (1ul << 15);
	//NVIC interrupt configuration for GPIO-B15 interrupt source
	NVIC->ISER[0] |= 1ul<<3;
	NVIC->IP[0] &= (~(3ul<<30));
	//GPIO interupt initialization end--------------
}

void EINT1_IRQHandler(void){
	lock_state = welcome_screen;
	PB->ISRC |= (1ul << 15);
}
 
int input_number(int input[6], int i){	
		char input_txt = '_';
		key_pressed = KeyPadScanning();
		if (key_pressed == 0 && last_key_pressed != key_pressed) { // check if the button is released
			sprintf(input_txt, "%d", last_key_pressed);	// Convert integer to char
			printS_5x7(40 + i*10, 24, input_txt);	// Print numerical value to LCD
			Delay_ms(200);
			printS_5x7(40 + i*10, 24, "*");	// Replace with *
			input[i] = last_key_pressed;
			return 0;
		}
		last_key_pressed = key_pressed; // Save last status of button
		return 1;
}

 