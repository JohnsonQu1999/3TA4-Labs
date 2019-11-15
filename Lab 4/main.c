#include "main.h"

__IO HAL_StatusTypeDef Hal_status;  //HAL_ERROR, HAL_TIMEOUT, HAL_OK, of HAL_BUSY 

ADC_HandleTypeDef    			Adc_Handle; 															//Handle for ADC conversion
ADC_ChannelConfTypeDef 		sConfig;																	//Handle for ADC channel
TIM_HandleTypeDef    			Tim1_Handle,Tim2_Handle;									//Handle for PWM and Polling timers
TIM_OC_InitTypeDef 				Tim2_OCInitStructure,fConfig;							//fConfig for PWM timer
uint16_t 									Tim1_PrescalerValue,Tim2_PrescalerValue;
__IO uint32_t 						Tim2_CCR; 																// the pulse of the TIM2
volatile double 					dutyCycle = 0;														//Stores the duty cycle

/* FLAGS */
uint16_t 									Tim2Flag = 0, 
													selectButtonFlag = 0, 
													downButtonFlag = 0, 
													upButtonFlag = 0, 
													Tim1Flag = 0;												
double 										measuredTemp, 														//Stores current temperature
													setPointTemperature,											//Stores set point temperature
													diff = 0,																	//Stores the difference between
													lastDutyCycle = 0;												//Stores the last duty cycle value
uint32_t 									aResultDMA;																//Gets value from register
char 											lcd_buffer[6];    												//LCD display buffer

typedef enum {						//state enum
	initial_state, set_temp
} states;
states state;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
void TIM1_Config(uint16_t period, double D_Cycle);
void TIM2_Config(uint16_t period);
void TIM2_OC_Config(void);
static void Error_Handler(void);
void ADC_Init();
void ADC_Channel_Config();
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim);
void printTemperature(double);
double getTemperature();
void printSetPointTemperature(double measuredTemp);

int main(void)
{
	/* Initialization */
	state = initial_state;
	
	HAL_Init();
	SystemClock_Config();   
	HAL_InitTick(0x0000); 				// set systick's priority to the highest.
	BSP_LED_Init(LED4);
	BSP_LED_Init(LED5);
	BSP_LCD_GLASS_Init();
	BSP_JOY_Init(JOY_MODE_EXTI);  

	/* PWM Timer initialization */
	Tim1_PrescalerValue = 15900;
	TIM1_Config(1999, 0);					//Period = 1999, duty cycle = 0

	/* Polling Timer initialization */
	TIM2_Config(5000);	
	Tim2_CCR = 500;
	TIM2_OC_Config();
	
	/* ADC initialization */
 	ADC_Init();
	ADC_Channel_Config();

	/* Delay to prevent false values */
	HAL_Delay(1000);
	measuredTemp = getTemperature();
	setPointTemperature = measuredTemp + 2;
	
  while (1)
  {
		switch (state) {
			case initial_state:														//Main state (displays current temperature)
				BSP_LED_Off(LED4); BSP_LED_Off(LED5);				//Turn off status LEDs
				if (Tim2Flag == 1){													//Update current temp & display on each polling timer interrupt
					Tim2Flag = 0;
					BSP_LCD_GLASS_Clear();
					measuredTemp = getTemperature();
					printTemperature(measuredTemp);
				}
				if (selectButtonFlag == 1){									//Transition to edit settings state
					selectButtonFlag = 0;
					state = set_temp;
				}
				
				diff = measuredTemp-setPointTemperature;		//Gets the difference between the current temperature and the setpoint
					
				lastDutyCycle = dutyCycle;									//Stores the last duty cycle, to prevent excessive editing of timer parameters
				
				if(diff<0){																	//Assigns duty cycles discretely
					dutyCycle = 0;
				}
				else if(diff<1){
					dutyCycle = 0.05;
				}
				else if(diff<2){
					dutyCycle = 0.15;
				}
				else if(diff<3){
					dutyCycle = 0.25;
				}
				else if(diff<4){
					dutyCycle = 0.4;
				}
				else if(diff<5){
					dutyCycle = 0.6;
				}
				else{
					dutyCycle = 1;
				}
								
				if(dutyCycle != lastDutyCycle){							//Edit PULSE if the duty cycle has changed
					__HAL_TIM_SET_COMPARE(&Tim1_Handle,TIM_CHANNEL_2,dutyCycle*1999);
				}
				break;
				
			case set_temp:																//Edit set point state
				if(selectButtonFlag){												//Transition to main state
					selectButtonFlag = 0;
					state = initial_state;
				}
					BSP_LED_On(LED4); BSP_LED_On(LED5);				//Turn on status LEDs
					printSetPointTemperature(setPointTemperature);	
				
				if (upButtonFlag){													//Increase setpoint by 0.5 if up button is pressed
					upButtonFlag = 0;
					setPointTemperature+=0.5;
				}
				if (downButtonFlag){												//Decrease setpoint by 0.5 if down button is pressed
					downButtonFlag = 0;
					setPointTemperature-=0.5;
				}
				break;
			
	} //end of while 1
}

}

void printTemperature(double measuredTemp){					//Prints the parameter measuredTemp to the LCD
	snprintf(lcd_buffer, 7, "%05.2f C", (double)measuredTemp);
	lcd_buffer[2] = 'd';
	BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
}

void printSetPointTemperature(double measuredTemp){ //Prints the parameter measuredTemp to the LCD
	snprintf(lcd_buffer, 10, "S %05.2f C", measuredTemp);
	lcd_buffer[4] = 'd';
	BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
}

double getTemperature(){ //Returns the scaled value in the register
	return (0.02442*aResultDMA);
}

void ADC_Channel_Config(){
	sConfig.Channel      = ADC_CHANNEL_6;               /* Sampled channel number */
  sConfig.Rank         = ADC_REGULAR_RANK_1;          /* Rank of sampled channel number ADCx_CHANNEL */
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;  /* Sampling time (number of clock cycles unit) */
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;            /* Single-ended input channel */
  sConfig.OffsetNumber = ADC_OFFSET_NONE;             /* No offset subtraction */ 
  sConfig.Offset = 0; 
	if (HAL_ADC_ConfigChannel(&Adc_Handle, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
	if (HAL_ADC_Start_DMA(&Adc_Handle, (uint32_t*)&aResultDMA, 1) != HAL_OK)
  {
    Error_Handler();
  }
}

void ADC_Init(){
	Adc_Handle.Instance = ADC1;
	if (HAL_ADC_DeInit(&Adc_Handle) != HAL_OK)
  {
    /* ADC de-initialization Error */
    Error_Handler();
  }
	Adc_Handle.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;          /* Asynchronous clock mode, input ADC clock not divided */
  Adc_Handle.Init.Resolution            = ADC_RESOLUTION_12B;             /* 12-bit resolution for converted data */
  Adc_Handle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;           /* Right-alignment for converted data */
  Adc_Handle.Init.ScanConvMode          = DISABLE;                       /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
  Adc_Handle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;           /* EOC flag picked-up to indicate conversion end */
  Adc_Handle.Init.LowPowerAutoWait      = DISABLE;                       /* Auto-delayed conversion feature disabled */
  Adc_Handle.Init.ContinuousConvMode    = ENABLE;                        /* Continuous mode enabled (automatic conversion restart after each conversion) */
  Adc_Handle.Init.NbrOfConversion       = 1;                             /* Parameter discarded because sequencer is disabled */
  Adc_Handle.Init.DiscontinuousConvMode = DISABLE;                       /* Parameter discarded because sequencer is disabled */
  Adc_Handle.Init.NbrOfDiscConversion   = 1;                             /* Parameter discarded because sequencer is disabled */
  Adc_Handle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;            /* Software start to trig the 1st conversion manually, without external event */
  Adc_Handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE; /* Parameter discarded because software trigger chosen */
  Adc_Handle.Init.DMAContinuousRequests = ENABLE;                        /* DMA circular mode selected */
  Adc_Handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;      /* DR register is overwritten with the last conversion result in case of overrun */
  Adc_Handle.Init.OversamplingMode      = DISABLE;   
  /* Initialize ADC peripheral according to the passed parameters */
  if (HAL_ADC_Init(&Adc_Handle) != HAL_OK)
  {
    Error_Handler();
  }	
	 if (HAL_ADCEx_Calibration_Start(&Adc_Handle, ADC_SINGLE_ENDED) !=  HAL_OK)
  {
    Error_Handler();
  }
}
void SystemClock_Config(void)
{ 
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};                                            

 
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;            
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;  
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; // RCC_MSIRANGE_6 is for 4Mhz. _7 is for 8 Mhz, _9 is for 16..., _10 is for 24 Mhz, _11 for 48Hhz
  RCC_OscInitStruct.MSICalibrationValue= RCC_MSICALIBRATION_DEFAULT;

	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;   //PLL source: either MSI, or HSI or HSE, but can not make HSE work.
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40; 
  RCC_OscInitStruct.PLL.PLLR = 2;  //2,4,6 or 8
  RCC_OscInitStruct.PLL.PLLP = 7;   // or 17.
  RCC_OscInitStruct.PLL.PLLQ = 4;   //2, 4,6, 0r 8  
	//the PLL will be MSI (4Mhz)*N /M/R = 

	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    // Initialization Error 
    while(1);
  }

  // configure the HCLK, PCLK1 and PCLK2 clocks dividers 
  // Set 0 Wait State flash latency for 4Mhz 
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; //the freq of pllclk is MSI (4Mhz)*N /M/R = 80Mhz 
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  
	
	if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)   //???
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

  // Disable Power Control clock   //why disable it?
  __HAL_RCC_PWR_CLK_DISABLE();      
}




/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin) {
			case GPIO_PIN_0: 		               //SELECT button					
						selectButtonFlag = 1;
						break;	
			case GPIO_PIN_1:     //left button						
							
							break;
			case GPIO_PIN_2:    //right button						  to play again.
						
							break;
			case GPIO_PIN_3:    //up button							
							upButtonFlag = 1;
							break;
			
			case GPIO_PIN_5:    //down button						
							downButtonFlag = 1;
							break;
			
			default://
						//default
						break;
	  } 
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32XXX_hal_tim.c for different callback function names. 
{																																//for timer4 
		if ((*htim).Instance == TIM2) {	
			Tim2Flag = 1;																	//this call-back function sets the flag for this timer
		}
		if ((*htim).Instance == TIM1) {	
			Tim1Flag = 1;																	//this call-back function sets the flag for this timer
		}

		__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h
}
 
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef * htim){  //this is for TIM4_pwm
	
	__HAL_TIM_SET_COUNTER(htim, 0x0000);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
{

}


//PWM Timer
void TIM1_Config(uint16_t period, double D_Cycle)
{
  
  /* Compute the prescaler value to have TIM3 counter clock equal to 10 KHz */
  Tim1_PrescalerValue = (uint16_t) (SystemCoreClock/ 40000) - 1;
  
  /* Set TIM3 instance */
  Tim1_Handle.Instance = TIM1; 
	Tim1_Handle.Init.Period = period;
  Tim1_Handle.Init.Prescaler = Tim1_PrescalerValue;
  Tim1_Handle.Init.ClockDivision = 0;
  Tim1_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	Tim1_Handle.Init.RepetitionCounter = 0;

	// Initialize the timer
	if(HAL_TIM_PWM_Init(&Tim1_Handle) != HAL_OK){
		Error_Handler();
	}
	
	//Set output compare values
	fConfig.OCMode       = TIM_OCMODE_PWM1;
  fConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
  fConfig.OCFastMode   = TIM_OCFAST_DISABLE;
  fConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  fConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  fConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
	
	//Calculate pulse
	fConfig.Pulse = (uint32_t)(D_Cycle*period);
	
	//Set parameters
	if(HAL_TIM_PWM_ConfigChannel(&Tim1_Handle, &fConfig, TIM_CHANNEL_2) != HAL_OK){
		Error_Handler();
	}
	
	//Start timer
	if(HAL_TIM_PWM_Start(&Tim1_Handle, TIM_CHANNEL_2) != HAL_OK){
		Error_Handler();
	}
}

//Reading Sensor Timer
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
}



void  TIM2_OC_Config(void)
{
		Tim2_OCInitStructure.OCMode=  TIM_OCMODE_TIMING;
		Tim2_OCInitStructure.Pulse=Tim2_CCR;
		Tim2_OCInitStructure.OCPolarity=TIM_OCPOLARITY_HIGH;
		
		HAL_TIM_OC_Init(&Tim2_Handle); // if the TIM4 has not been set, then this line will call the callback function _MspInit() 
		HAL_TIM_OC_ConfigChannel(&Tim2_Handle, &Tim2_OCInitStructure, TIM_CHANNEL_1); //must add this line to make OC work.!!!
	 	HAL_TIM_OC_Start_IT(&Tim2_Handle, TIM_CHANNEL_1); //this function enable IT and enable the timer. so do not need
}

static void Error_Handler(void)
{
  /* Turn LED4 on */
  BSP_LED_On(LED4);
  while(1)
  {
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
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
