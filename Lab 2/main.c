#include "main.h"

/* Private typedef -----------------------------------------------------------*/
TIM_HandleTypeDef    Tim2_Handle, Tim3_Handle;			// 2 timers are used 
TIM_OC_InitTypeDef Tim2_OCInitStructure;						// Tim2 works on OCR
RNG_HandleTypeDef Rng_Handle;

typedef enum {																			// defines all the states of FSM
	ResetState, randomDelay, responseCounter, displayTime
} differentStates;

differentStates state = ResetState;									// initially set the state variable to be in ResetState (LED Flashing Continuously)

/* Private variables ---------------------------------------------------------*/
char lcd_buffer[6];    															// LCD display buffer

__IO HAL_StatusTypeDef Hal_status;  								//HAL_ERROR, HAL_TIMEOUT, HAL_OK, of HAL_BUSY 

uint16_t EE_status=0;
uint16_t VirtAddVarTab[NB_OF_VAR] = {0x5555, 0x6666, 0x7777}; // the emulated EEPROM can save 3 varibles, at these three addresses.
uint16_t EEREAD;  																						//to practice reading the BESTRESULT save in the EE, for EE read/write, require uint16_t type

uint16_t Tim2_PrescalerValue, Tim3_PrescalerValue;	//timer prescalers to slow them down

__IO uint32_t Tim2_CCR;															//CCR value of OCR TIMER

//USING FLAGS TO FLOW THROUGH DIFFERENT STATES, Flag for each timer or push-button interrupt is serviced by setting the respective flags to high
uint8_t selectButtonFlag = 0;												//middle button of the joystick		
uint8_t Tim2Flag = 0;																//used to keep track of timer 2's interrupts 
uint8_t Tim3Flag = 0;																//used to keep track of timer 3's interrupts 
static uint8_t displayBestFlag = 0;									//up-button of the joystick
static uint8_t goBackToResetFlag = 0;								//down-button of the joystick
static uint16_t Tim3_Counter = 0;										//Counts the elapsed time
static uint16_t responseTime = 0;										//stores the best result in this variable
static uint16_t bestTime = 65535;										//variable used to reset timing counter


/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
void TIM2_Config(uint16_t period);
void TIM2_OC_Config(void);
void  TIM3_Config(uint16_t offTime);
uint16_t getRandomDelayTime(void);									// generates 16-bit random number for timing delays


int main(void)
{	
	HAL_Init();
  SystemClock_Config(); 
	HAL_InitTick(0x0000); 														//set the systick interrupt priority to the highest

	BSP_LED_Init(LED5);																//initializes the flashing LED
	BSP_LED_Off(LED5);
	
	BSP_LCD_GLASS_Init();
	BSP_JOY_Init(JOY_MODE_EXTI);
	
	state = ResetState;			
	TIM2_Config(5000);																//setting the period to 0.5s --> 1 interrupt per 0.5 s --> 1 complete flashing cycle every second --> flashing @ 1Hz
	Tim2_CCR = 500;	
	TIM2_OC_Config();
	
//******************* use emulated EEPROM ====================================
	//First, Unlock the Flash Program Erase controller 
	HAL_FLASH_Unlock();
		
// EEPROM Init 
	EE_status=EE_Init();
	if(EE_status != HAL_OK)
  {
		Error_Handler();
  }
// then can write to or read from the emulated EEPROM
	
//*********************use RNG ================================  
Rng_Handle.Instance=RNG;  //Everytime declare a Handle, need to assign its Instance a base address. like the timer handles.... 													
	
	Hal_status=HAL_RNG_Init(&Rng_Handle);   //go to msp.c to see further low level initiation.
	
	if( Hal_status != HAL_OK)
  {
    Error_Handler();
  }
//then can use RNG
	
	//EE_WriteVariable(VirtAddVarTab[0], bestTime);   //Uncomment this line & reflash to reset the best time;

  /* Infinite loop */
  while (1){																//the game loops through this to allow players to beat their best time
		switch (state){													//switch cases are used to transition between all the states of FSM
			
			case ResetState:											//LED Flashing state - Initial State
						if(selectButtonFlag == 1){													//detects user's indication to start the game; condition to turn LED OFF for some time
							selectButtonFlag = 0;															//select Button is pressed, interrupt service routine sets the flag value to be 1
							HAL_TIM_Base_Stop(&Tim2_Handle);									//undo the interrupt causing setting for subsequent inputs
							Tim2Flag = 0;																			
							state = randomDelay;															//move to next state
							uint32_t randomDelayTime = getRandomDelayTime();	//computes a random number
							Tim2_CCR = randomDelayTime;												//assign a comparator value for timer 2
							TIM2_Config(randomDelayTime);											//intialize and configure the timer to cause interrupt after a random amount of time
							TIM2_OC_Config();					
							BSP_LED_Off(LED5);																//turn off the LED
							Tim2Flag = 0;																		
						} 
						else if (Tim2Flag == 1) {														//this is the very first thing that the u-controller does; this ensures that the LED is flashing at the correct
							Tim2Flag = 0;																			//speed after returning from the final state (displayTime). Note that we change the CCR and period of TIM2 to a random time
							BSP_LED_Toggle(LED5);															//toggle the LED
							Tim2_CCR = 500;																		//toggles LED at a certain pace using interrupts
							TIM2_Config(5000);
							TIM2_OC_Config();
						}
						
						if(displayBestFlag == 1){														//displays the best-time achieved by the player when user presses up-button
							displayBestFlag = 0;															
							EE_ReadVariable(VirtAddVarTab[0], &bestTime);			//reads from EEPROM memory location
							snprintf(lcd_buffer, 7, "%dms", bestTime);				//stores series of character in a variable, as an array
							BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);//displays it on the LED
						}
						//Implement left = write 65535 into eeprom
						break;
						
			case randomDelay:																					//enters this state to wait for some time with LED OFF
						if(Tim2Flag == 1){																	//this timer creates an interrupt after 1.5-2.5s																		
							BSP_LED_On(LED5);																	
							Tim2Flag = 0;
							TIM3_Config(1000);
							Tim3_Counter = 0;
							responseTime = 0;
							state = responseCounter;													//moves to the next state, which shows and records response time 
						}
						if(displayBestFlag == 1){														//shows the best-time on the LCD
								displayBestFlag = 0;							
								EE_ReadVariable(VirtAddVarTab[0], &bestTime);
								snprintf(lcd_buffer, 7, "%dms", bestTime);
								BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
						}
						if(goBackToResetFlag == 1) {												//upon pressing down-button of the joystick,
							goBackToResetFlag = 0;														//this block brings you back to the reset state
							state = ResetState;
							BSP_LCD_GLASS_Clear();
						}
						break;
						
			case responseCounter:
				restart_label:																					//label used to implement cheating detection
						BSP_LCD_GLASS_Clear();
						snprintf(lcd_buffer, 7, "%dmS", Tim3_Counter);			//shows the counter incrementing in milli-seconds 
						BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
						if(Tim3Flag == 1 && selectButtonFlag == 1){					//after registering user's input (select-button), 
							Tim3Flag = 0;																			//this if-block detects when the user reacts
							selectButtonFlag = 0;
							responseTime = Tim3_Counter;
							if (responseTime <= 0) {													//this if-block detects the cheating condition where responseTime = 0ms
								Tim3_Counter = 0;
								goto restart_label;															//this brings the player back to the start of the game
							}
							state = displayTime;															//move to the next state
						}
						if(displayBestFlag == 1){														//displays best time on the LCD
							displayBestFlag = 0;							
							EE_ReadVariable(VirtAddVarTab[0], &bestTime);
							snprintf(lcd_buffer, 7, "%dms", bestTime);
							BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
						}
						if(goBackToResetFlag == 1) {												//pressing down-button brings you back to the reset state
							goBackToResetFlag = 0;
							state = ResetState;
							BSP_LCD_GLASS_Clear();
						}
						break;
						
			case displayTime:																					//shows your timing for the last game
						snprintf(lcd_buffer, 7, "%dms", responseTime);
						BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);	
						EE_ReadVariable(VirtAddVarTab[0], &bestTime);				//reads the best-time from EEPROM
						if(responseTime < bestTime){												//if best-time is less than the stored best-time, enter the block
							bestTime = responseTime;													//make the smaller time as the best time
							EE_WriteVariable(VirtAddVarTab[0], bestTime);			//writes the new result in the EEPROM 
						}	
						if(displayBestFlag == 1){														//displays the record time
							displayBestFlag = 0;							
							EE_ReadVariable(VirtAddVarTab[0], &bestTime);
							snprintf(lcd_buffer, 7, "%dms", bestTime);
							BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
						}
						if(selectButtonFlag == 1){													//select-button brings you back to the reset-state
							selectButtonFlag = 0;
							BSP_LCD_GLASS_Clear();
							state = ResetState;
						}
						if(goBackToResetFlag == 1) {												//down-button brings you back to the reset-state as well
							goBackToResetFlag = 0;
							state = ResetState;
							BSP_LCD_GLASS_Clear();
						}
						break;
		}

	}
}

uint16_t getRandomDelayTime(void){					//generates a random number 
	uint32_t randomNumber;
  HAL_RNG_GenerateRandomNumber(&Rng_Handle, &randomNumber);	
	randomNumber = randomNumber%1000;         //allows the time to stay within 1000ms
	randomNumber = randomNumber + 1500;				//lets the time to be a minimum of 1500ms
		
	return ((uint16_t)randomNumber);					//passes it over for timer 
}

void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
                                     

  // MSI is enabled after System reset at 4Mhz, PLL not used 
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; // RCC_MSIRANGE_6 is for 4Mhz. _7 is for 8 Mhz, _9 is for 16..., _10 is for 24 Mhz, _11 for 48Hhz
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  
//	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40; 
  RCC_OscInitStruct.PLL.PLLR = 2;  //2,4,6 or 8
  RCC_OscInitStruct.PLL.PLLP = 7;   // or 17.
  RCC_OscInitStruct.PLL.PLLQ = 4;   //2, 4,6, 0r 8  ===the clock for RNG will be 4Mhz *N /M/Q =40Mhz. which is nearly 48
	
	
	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    // Initialization Error 
    while(1);
  }

  // Select MSI as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers 
  // Set 0 Wait State flash latency for 4Mhz 
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    // Initialization Error 
    while(1);
  }

  // The voltage scaling allows optimizing the power consumption when the device is
  //   clocked below the maximum system frequency, to update the voltage scaling value
  //   regarding system frequency refer to product datasheet.  

  // Enable Power Control clock 
  __HAL_RCC_PWR_CLK_ENABLE();

  if(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2) != HAL_OK)
  {
    // Initialization Error 
    while(1);
  }

  // Disable Power Control clock 
  __HAL_RCC_PWR_CLK_DISABLE();
}



void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)										//whenever a joystick button is pushed, it creates an interrupt
{
  switch (GPIO_Pin) {
			case GPIO_PIN_0: 		              //SELECT button					
						BSP_LCD_GLASS_Clear();
						selectButtonFlag = 1;
					
						break;	
			case GPIO_PIN_1:     							//left button						
						
							break;
			case GPIO_PIN_2:    							//right button to play again.
					
							break;
			case GPIO_PIN_3:    //up button							
						displayBestFlag = 1;
							break;
			
			case GPIO_PIN_5:    //down button						
						goBackToResetFlag = 1;
							break;
			default://
						//default
						break;
	  } 
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32fxx_hal_tim.c for different callback function names. 
{																																
	if ((*htim).Instance == TIM2) {	
				//BSP_LED_Toggle(LED5);
			Tim2Flag = 1;																	//this call-back function sets the flag for this timer
		}

		__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h

}

void TIM2_Config(uint16_t period)
{
  
  /* Compute the prescaler value to have TIM3 counter clock equal to 10 KHz */
  Tim2_PrescalerValue = (uint16_t) (SystemCoreClock/ 1500) - 1;
  
  /* Set TIM3 instance */
  Tim2_Handle.Instance = TIM2; 
	Tim2_Handle.Init.Period = period;
  Tim2_Handle.Init.Prescaler = Tim2_PrescalerValue;
  Tim2_Handle.Init.ClockDivision = 0;
  Tim2_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
  //if(HAL_TIM_Base_Init(&Tim4_Handle) != HAL_OK)
  //{
    /* Initialization Error */
  //  Error_Handler();
  //} 
}



void  TIM2_OC_Config(void)
{
		Tim2_OCInitStructure.OCMode=  TIM_OCMODE_TIMING;
		Tim2_OCInitStructure.Pulse=Tim2_CCR;
		Tim2_OCInitStructure.OCPolarity=TIM_OCPOLARITY_HIGH;
		
		HAL_TIM_OC_Init(&Tim2_Handle); // if the TIM4 has not been set, then this line will call the callback function _MspInit() 
													//in stm32f4xx_hal_msp.c to set up peripheral clock and NVIC.
	
		HAL_TIM_OC_ConfigChannel(&Tim2_Handle, &Tim2_OCInitStructure, TIM_CHANNEL_1); //must add this line to make OC work.!!!
	
	   /* **********see the top part of the hal_tim.c**********
		++ HAL_TIM_OC_Init and HAL_TIM_OC_ConfigChannel: to use the Timer to generate an 
              Output Compare signal. 
			similar to PWD mode and Onepulse mode!!!
	
	*******************/
	
	 	HAL_TIM_OC_Start_IT(&Tim2_Handle, TIM_CHANNEL_1); //this function enable IT and enable the timer. so do not need
				//HAL_TIM_OC_Start() any more
		//HAL_TIM_OC_Start_IT(&Tim2_Handle, TIM_CHANNEL_2);
				
		
}

void  TIM3_Config(uint16_t count)
{ 
  
  /* Compute the prescaler value to have TIM3 counter clock equal to 10 KHz */
  Tim3_PrescalerValue = (uint16_t) (SystemCoreClock/1000000) - 1;
  
  /* Set TIM3 instance */
  Tim3_Handle.Instance = TIM3; //TIM3 is defined in stm32f429xx.h
  Tim3_Handle.Init.Period = count - 1;
  Tim3_Handle.Init.Prescaler = Tim3_PrescalerValue;
  Tim3_Handle.Init.ClockDivision = 0;
  Tim3_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
  if(HAL_TIM_Base_Init(&Tim3_Handle) != HAL_OK) // this line need to call the callback function _MspInit() in stm32f4xx_hal_msp.c to set up peripheral clock and NVIC..
  {
    /* Initialization Error */
    Error_Handler();
  }
  
  /*##-2- Start the TIM Base generation in interrupt mode ####################*/
  /* Start Channel1 */
  if(HAL_TIM_Base_Start_IT(&Tim3_Handle) != HAL_OK)   //the TIM_XXX_Start_IT function enable IT, and also enable Timer
																											//so do not need HAL_TIM_BASE_Start() any more.
  {
    /* Starting Error */
    Error_Handler();
  }
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)   //see  stm32lxx_hal_tim.c for different callback function names. 
																															//for timer 3 , Timer 3 use update event initerrupt
{
	if ((*htim).Instance == TIM3) {											
		Tim3Flag = 1;															//flag is set to 1
		Tim3_Counter = Tim3_Counter + 1;					//counts the value up every milli-second and used to display the timing on the LCD
	}
}


static void Error_Handler(void)
{
 
 
}



#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
