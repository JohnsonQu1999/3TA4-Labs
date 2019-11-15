
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#define strBuffer 30				//defines max string length of lcd buffer

I2C_HandleTypeDef  pI2c_Handle;
RTC_HandleTypeDef RTCHandle;
RTC_DateTypeDef RTC_DateStructure;
RTC_TimeTypeDef RTC_TimeStructure;

__IO HAL_StatusTypeDef Hal_status;  //HAL_ERROR, HAL_TIMEOUT, HAL_OK, of HAL_BUSY 

//memory location to write to in the device
__IO uint16_t memLocation = 0x000A; //pick any location within range

char lcd_buffer[strBuffer];    // LCD display buffer 
char timestring[10]={0};    
char datestring[6]={0};

typedef enum {						//Machine's Generic States
	STATE_displayDateTime, STATE_whenPushedStoreTime, STATE_selHeld, STATE_displayLast2Times, STATE_settingsMode 	
} states;

typedef enum {						//states to change all the time and date parameters
	set_weekDay = 0, set_date, set_month, set_year, set_hour, set_min, set_second
} settingStates;

states state; 						//instance variable of the states enum
settingStates new_set;		//instance variable of the settingStates

uint8_t RTC_Flag = 1;
uint8_t selHeldFlag = 0;
uint8_t wd, dd, mo, yy, ss, mm, hh; // for weekday, day, month, year, second, minute, hour
uint8_t display2TimesCNT;

__IO uint32_t SEL_Pressed_StartTick;   //sysTick when the User button is pressed
__IO uint8_t leftpressed, rightpressed, uppressed, downpressed, selpressed;  // button pressed 
__IO uint8_t  sel_held;   // if the selection button is held for a while (>800ms)

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);

void RTC_Config(void);
void RTC_AlarmAConfig(void);
void storeTimeEEPROM(void);				//stores time into EEPROM 
void display2times(void);					//displays last two times
int decimalFromBCD(uint16_t bcd);	//converts BCD to decimal
const char * dayFromBCD(uint8_t bcd);
uint16_t bcdFromDecimal(int);
uint8_t bcdFromDay(uint8_t);

int main(void)
{
	leftpressed=0;
	rightpressed=0;
	uppressed=0;
	downpressed=0;
	selpressed=0;
	sel_held=0;
	
	state = STATE_displayDateTime;
	display2TimesCNT = 0;
	new_set = set_weekDay;

	HAL_Init();
	BSP_LED_Init(LED4); 
	BSP_LED_Init(LED5);	
	SystemClock_Config();   
	HAL_InitTick(0x0000); //set the systick interrupt priority to the highest, !!!This line need to be after systemClock_config()
	BSP_LCD_GLASS_Init();
	BSP_JOY_Init(JOY_MODE_EXTI);
	BSP_LCD_GLASS_DisplayString((uint8_t*)"LAB-3");	
	HAL_Delay(1000);


//configure real-time clock
	RTC_Config();
	RTC_AlarmAConfig();
	I2C_Init(&pI2c_Handle);

/*
// *********************Testing I2C EEPROM------------------
	//the following variables are for testging I2C_EEPROM
	uint8_t data1 =0x67,  data2=0x68;
	uint8_t readData=0x00;
	uint16_t EE_status;


	EE_status=I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation, data1);

  
  if(EE_status != HAL_OK)
  {
    I2C_Error(&pI2c_Handle);
  }
	
	
	BSP_LCD_GLASS_Clear();
	if (EE_status==HAL_OK) {
			BSP_LCD_GLASS_DisplayString((uint8_t*)"w 1 ok");
	}else
			BSP_LCD_GLASS_DisplayString((uint8_t*)"w 1 X");

	HAL_Delay(1000);
	
	EE_status=I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation+1 , data2);
	
  if(EE_status != HAL_OK)
  {
    I2C_Error(&pI2c_Handle);
  }
	
	BSP_LCD_GLASS_Clear();
	if (EE_status==HAL_OK) {
			BSP_LCD_GLASS_DisplayString((uint8_t*)"w 2 ok");
	}else
			BSP_LCD_GLASS_DisplayString((uint8_t*)"w 2 X");

	HAL_Delay(1000);
	
	readData=I2C_ByteRead(&pI2c_Handle,EEPROM_ADDRESS, memLocation);

	BSP_LCD_GLASS_Clear();
	if (data1 == readData) {
			BSP_LCD_GLASS_DisplayString((uint8_t*)"r 1 ok");;
	}else{
			BSP_LCD_GLASS_DisplayString((uint8_t*)"r 1 X");
	}	
	
	HAL_Delay(1000);
	
	readData=I2C_ByteRead(&pI2c_Handle,EEPROM_ADDRESS, memLocation+1);

	BSP_LCD_GLASS_Clear();
	if (data2 == readData) {
			BSP_LCD_GLASS_DisplayString((uint8_t*)"r 2 ok");;
	}else{
			BSP_LCD_GLASS_DisplayString((uint8_t *)"r 2 X");
	}	

	HAL_Delay(1000);
// ******************************testing I2C EEPROM*****************************	
	*/

  
	/* Infinite loop */
  while (1)
  {
			//the joystick is pulled down. so the default status of the joystick is 0, when pressed, get status of 1. 
			//while the interrupt is configured at the falling edge---the moment the pressing is released, the interrupt is triggered.
			//therefore, the variable "selpressed==1" can not be used to make choice here.
			if (BSP_JOY_GetState() == JOY_SEL) {
					SEL_Pressed_StartTick=HAL_GetTick(); 
					while(BSP_JOY_GetState() == JOY_SEL) {  //while the selection button is pressed)	
						if ((HAL_GetTick()-SEL_Pressed_StartTick)>800) {		
								selHeldFlag = 1;
								selpressed = 0;
						} 
					}
			}					
		
			switch (state) { 
				
				case STATE_displayDateTime:
						if (RTC_Flag == 1){ 
							BSP_LCD_GLASS_Clear();
							HAL_RTC_GetTime(&RTCHandle, &RTC_TimeStructure, RTC_FORMAT_BCD);
							snprintf(lcd_buffer, 7, "%02d%02d%02d", decimalFromBCD(RTC_TimeStructure.Hours), 
																								decimalFromBCD(RTC_TimeStructure.Minutes), 
																								decimalFromBCD(RTC_TimeStructure.Seconds));
							BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);
							HAL_RTC_GetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);
							RTC_Flag = 0;
						} 
						if (selpressed == 1){
							state = STATE_whenPushedStoreTime;
						}
						if (selHeldFlag == 1){
							state = STATE_selHeld;
						}
						if (leftpressed == 1){
							state = STATE_displayLast2Times;
						}
						if (rightpressed == 1){
							state = STATE_settingsMode;
						}
				break;
						
				case STATE_whenPushedStoreTime:
						storeTimeEEPROM();
						selpressed = 0;
						state = STATE_displayDateTime;
				break;
				
				case STATE_selHeld:
						BSP_LCD_GLASS_Clear();
						snprintf(lcd_buffer, 20, " %3s-%02d/%02d/%02d   ", 
																								dayFromBCD(RTC_DateStructure.WeekDay), 
																								decimalFromBCD(RTC_DateStructure.Date),
																								decimalFromBCD(RTC_DateStructure.Month), 
																								decimalFromBCD(RTC_DateStructure.Year));
						BSP_LCD_GLASS_ScrollSentence((uint8_t*)lcd_buffer, 1, 500);
						selHeldFlag = 0;
						state = STATE_displayDateTime;
				break;
				
				
				case STATE_displayLast2Times:
						if (display2TimesCNT%2 != 0){
							BSP_LCD_GLASS_Clear();
							display2times();
						}	
						if (display2TimesCNT%2 == 0){
							state = STATE_displayDateTime;
						}
						leftpressed = 0;
				break;
				
				case STATE_settingsMode:
						BSP_LED_On(LED4); BSP_LED_On(LED5); //both LEDs stay on during the setting modes functioning
						BSP_LCD_GLASS_Clear();
						
						rightpressed = 0;
						//method to read current values
						
						hh = decimalFromBCD(RTC_TimeStructure.Hours);
						mm = decimalFromBCD(RTC_TimeStructure.Minutes);
						ss = decimalFromBCD(RTC_TimeStructure.Seconds);
						
						wd = RTC_DateStructure.WeekDay;
						dd = decimalFromBCD(RTC_DateStructure.Date);
						mo = decimalFromBCD(RTC_DateStructure.Month);
						yy = decimalFromBCD(RTC_DateStructure.Year);
				
						while(rightpressed != 1){
								if(leftpressed == 1){
									if(new_set == set_weekDay){
										new_set = set_date;
									}
									else if(new_set == set_date){
										new_set = set_month;
									}
									else if(new_set == set_month){
										new_set = set_year;
									}
									else if(new_set == set_year){
										new_set = set_hour;
									}
									else if(new_set == set_hour){
										new_set = set_min;
									}
									else if(new_set == set_min){
										new_set = set_second;
									}
									else if(new_set == set_second){
										new_set = set_weekDay;
									}
									leftpressed = 0;
								}
								
								//takes care of incrementing values, depending on the parameter being edited
								if(selpressed == 1){
									if(new_set == set_weekDay){
										wd++;
										if(wd == 8){
											wd = 1;
										}
									}
									else if(new_set == set_date){
										dd++;
										if(dd == 32){
											dd = 1;
										}
									}
									else if(new_set == set_month){
										mo++;
										if(mo == 13){
											mo = 1;
										}
									}
									else if(new_set == set_year){
										yy++;
									}
									else if(new_set == set_hour){
										hh++;
										if(hh == 25){
											hh = 1;
										}
									}
									else if(new_set == set_min){
										mm++;
										if(mm == 61){
											mm = 1;
										}
									}
									else if(new_set == set_second){
										ss++;
										if(ss == 61){
											ss = 1;
										}
									}
								selpressed = 0;
								}
								
								//method to write to structures
								
								RTC_TimeStructure.Hours = bcdFromDecimal(hh);
								RTC_TimeStructure.Minutes = bcdFromDecimal(mm);
								RTC_TimeStructure.Seconds = bcdFromDecimal(ss);
								
								RTC_DateStructure.WeekDay = wd;
								RTC_DateStructure.Date = bcdFromDecimal(dd);
								RTC_DateStructure.Month = bcdFromDecimal(mo);
								RTC_DateStructure.Year = bcdFromDecimal(yy);
								
								//method to print to lcd depending on the mode (time struct or day struct)
								
								BSP_LCD_GLASS_Clear();
								
								if(new_set == set_weekDay){
									snprintf(lcd_buffer, 7, "WD %3s", dayFromBCD(RTC_DateStructure.WeekDay));
								}
								else if(new_set == set_date){
									snprintf(lcd_buffer, 7, "DD %02d", decimalFromBCD(RTC_DateStructure.Date));
								}
								else if(new_set == set_month){
									snprintf(lcd_buffer, 7, "MO %02d", decimalFromBCD(RTC_DateStructure.Month));
								}
								else if(new_set == set_year){
									snprintf(lcd_buffer, 7, "YY %02d", decimalFromBCD(RTC_DateStructure.Year));
								}
								else if(new_set == set_hour){
									snprintf(lcd_buffer, 7, "HH %02d", decimalFromBCD(RTC_TimeStructure.Hours));
								}
								else if(new_set == set_min){
									snprintf(lcd_buffer, 7, "MM %02d", decimalFromBCD(RTC_TimeStructure.Minutes));									
								}
								else if(new_set == set_second){
									snprintf(lcd_buffer, 7, "SS %02d", decimalFromBCD(RTC_TimeStructure.Seconds));
								}
								
								BSP_LCD_GLASS_DisplayString((uint8_t*)lcd_buffer);								
						}
						
						//updates the values in RTC
						HAL_RTC_SetTime(&RTCHandle, &RTC_TimeStructure, RTC_FORMAT_BCD);
						HAL_RTC_SetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);
						HAL_RTC_GetTime(&RTCHandle, &RTC_TimeStructure, RTC_FORMAT_BCD);
						HAL_RTC_GetDate(&RTCHandle, &RTC_DateStructure, RTC_FORMAT_BCD);
						
						rightpressed = 0;
						state = STATE_displayDateTime;
						BSP_LED_Off(LED4); BSP_LED_Off(LED5);		//turns off the LEDs in other states
				break;
				
				default:
					BSP_LCD_GLASS_DisplayString((uint8_t*)"ERROR");
				break;
			}				
	}
}

const char * dayFromBCD(uint8_t bcd){
	switch (bcd){
		case 1:
			return "Mon";
		case 2:
			return "Tue";
		case 3:
			return "Wed";
		case 4:
			return "Thu";
		case 5:
			return "Fri";
		case 6:
			return "Sat";
		case 7:
			return "Sun";
	}
}

int decimalFromBCD(uint16_t bcd){
		return (((bcd & 0xF0) >> 4) * 10) + (bcd & 0x0F);
}

uint16_t bcdFromDecimal(int decimal){
	unsigned int ones = 0;
	unsigned int temp = 0;
	unsigned int ten = 0;
	
	ones = decimal%10;
	temp = decimal/10;
	ten = temp<<4;
	
	return (ten | ones);
}

//uint8_t bcdFromDay(uint8_t day){
//	return day;
//}

void display2times(void){
		uint8_t Hours = 		I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation);
		uint8_t Minutes = 	I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 1);
		uint8_t Seconds = 	I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 2);
		uint8_t previousHours = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 3);
		uint8_t previousMinutes = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 4);
		uint8_t previousSeconds = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 5);
	
		snprintf(lcd_buffer, 30, " %02d%02d%02d - %02d%02d%02d -- ", 
													decimalFromBCD(Hours), decimalFromBCD(Minutes), 
													decimalFromBCD(Seconds), decimalFromBCD(previousHours), 
													decimalFromBCD(previousMinutes), decimalFromBCD(previousSeconds));
		BSP_LCD_GLASS_ScrollSentence((uint8_t*)lcd_buffer, 1, 400);
}

void storeTimeEEPROM(void){
		uint8_t previousValueHours = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation);
		uint8_t previousValueMinutes = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 1);
		uint8_t previousValueSeconds = I2C_ByteRead(&pI2c_Handle, EEPROM_ADDRESS, memLocation + 2);
	
		uint8_t hours   = RTC_TimeStructure.Hours;
		uint8_t minutes = RTC_TimeStructure.Minutes;
		uint8_t seconds = RTC_TimeStructure.Seconds;
		
		uint16_t EE_status;
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation, hours);
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation + 1, minutes);
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation + 2, seconds);
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation + 3, previousValueHours);
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation + 4, previousValueMinutes);
		EE_status = I2C_ByteWrite(&pI2c_Handle,EEPROM_ADDRESS, memLocation + 5, previousValueSeconds);
  
		if(EE_status != HAL_OK)
		{
			I2C_Error(&pI2c_Handle);
		}
}


void SystemClock_Config(void)
{ 
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};                                            

  // RTC requires to use HSE (or LSE or LSI, suspect these two are not available)
	//reading from RTC requires the APB clock is 7 times faster than HSE clock, 
	//so turn PLL on and use PLL as clock source to sysclk (so to APB)
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;     //RTC need either HSE, LSE or LSI           
  
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;  
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; // RCC_MSIRANGE_6 is for 4Mhz. _7 is for 8 Mhz, _9 is for 16..., _10 is for 24 Mhz, _11 for 48Hhz
  RCC_OscInitStruct.MSICalibrationValue= RCC_MSICALIBRATION_DEFAULT;
  
	//RCC_OscInitStruct.PLL.PLLState = RCC_PLL_OFF;//RCC_PLL_NONE;

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
//after RCC configuration, for timmer 2---7, which are one APB1, the TIMxCLK from RCC is 4MHz


void RTC_Config(void) {
	RTC_TimeTypeDef RTC_TimeStructure;
	RTC_DateTypeDef RTC_DateStructure;
	

	//****1:***** Enable the RTC domain access (enable wirte access to the RTC )
			//1.1: Enable the Power Controller (PWR) APB1 interface clock:
        __HAL_RCC_PWR_CLK_ENABLE();    
			//1.2:  Enable access to RTC domain 
				HAL_PWR_EnableBkUpAccess();    
			//1.3: Select the RTC clock source
				__HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSE);    
				//RCC_RTCCLKSOURCE_LSI is defined in hal_rcc.h
	       // according to P9 of AN3371 Application Note, LSI's accuracy is not suitable for RTC application!!!! 
				
			//1.4: Enable RTC Clock
			__HAL_RCC_RTC_ENABLE();   //enable RTC --see note for the Macro in _hal_rcc.h---using this Marco requires 
																//the above three lines.
			
	
			//1.5  Enable LSI
			__HAL_RCC_LSI_ENABLE();   //need to enable the LSI !!!
																//defined in _rcc.c
			while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY)==RESET) {}    //defind in rcc.c
	
			// for the above steps, please see the CubeHal UM1725, p616, section "Backup Domain Access" 	
				
				
				
	//****2.*****  Configure the RTC Prescaler (Asynchronous and Synchronous) and RTC hour 
        
		
		/************students: need to complete the following lines******************************/
		
				RTCHandle.Instance = RTC;
				RTCHandle.Init.HourFormat = RTC_HOURFORMAT_24;
				
				RTCHandle.Init.AsynchPrediv = 0x7F; 
				RTCHandle.Init.SynchPrediv = 0xF9; 
				
				
				RTCHandle.Init.OutPut = RTC_OUTPUT_DISABLE;
				RTCHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
				RTCHandle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
				
			
				if(HAL_RTC_Init(&RTCHandle) != HAL_OK)
				{
					BSP_LCD_GLASS_Clear(); 
					BSP_LCD_GLASS_DisplayString((uint8_t *)"RT I X"); 	
				}
	
	
	//****3.***** init the time and date
				
				
 		
				RTC_DateStructure.Year = 19;
				RTC_DateStructure.Month = RTC_MONTH_OCTOBER;
				RTC_DateStructure.Date = 24;
				RTC_DateStructure.WeekDay = RTC_WEEKDAY_MONDAY;
				
				if(HAL_RTC_SetDate(&RTCHandle,&RTC_DateStructure,RTC_FORMAT_BIN) != HAL_OK)   //BIN format is better 
															//before, must set in BCD format and read in BIN format!!
				{
					BSP_LCD_GLASS_Clear();
					BSP_LCD_GLASS_DisplayString((uint8_t *)"D I X");
				} 
  
  
				RTC_TimeStructure.Hours = 17;  
				RTC_TimeStructure.Minutes = 19; 
				RTC_TimeStructure.Seconds = 00;
				RTC_TimeStructure.TimeFormat = RTC_HOURFORMAT12_AM;   
				RTC_TimeStructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
				RTC_TimeStructure.StoreOperation = RTC_STOREOPERATION_SET;
				
				if(HAL_RTC_SetTime(&RTCHandle,&RTC_TimeStructure,RTC_FORMAT_BIN) != HAL_OK)   //BIN format is better
																																					//before, must set in BCD format and read in BIN format!!
				{
					BSP_LCD_GLASS_Clear();
					BSP_LCD_GLASS_DisplayString((uint8_t *)"T I X");
				}	


				
			__HAL_RTC_TAMPER1_DISABLE(&RTCHandle);
			__HAL_RTC_TAMPER2_DISABLE(&RTCHandle);	
				//Optionally, a tamper event can cause a timestamp to be recorded. ---P802 of RM0090
				//Timestamp on tamper event
				//With TAMPTS set to ‘1 , any tamper event causes a timestamp to occur. In this case, either
				//the TSF bit or the TSOVF bit are set in RTC_ISR, in the same manner as if a normal
				//timestamp event occurs. The affected tamper flag register (TAMP1F, TAMP2F) is set at the
				//same time that TSF or TSOVF is set. ---P802, about Tamper detection
				//-------that is why need to disable this two tamper interrupts. Before disable these two, when program start, there is always a timestamp interrupt.
				//----also, these two disable function can not be put in the TSConfig().---put there will make  the program freezed when start. the possible reason is
				//-----one the RTC is configured, changing the control register again need to lock and unlock RTC and disable write protection.---See Alarm disable/Enable 
				//---function.
				
			HAL_RTC_WaitForSynchro(&RTCHandle);	
			//To read the calendar through the shadow registers after Calendar initialization,
			//		calendar update or after wake-up from low power modes the software must first clear
			//the RSF flag. The software must then wait until it is set again before reading the
			//calendar, which means that the calendar registers have been correctly copied into the
			//RTC_TR and RTC_DR shadow registers.The HAL_RTC_WaitForSynchro() function
			//implements the above software sequence (RSF clear and RSF check).	
}


void RTC_AlarmAConfig(void)
{
	RTC_AlarmTypeDef RTC_Alarm_Structure;

	
	RTC_Alarm_Structure.Alarm = RTC_ALARM_A;
  RTC_Alarm_Structure.AlarmMask = RTC_ALARMMASK_ALL;
	
		
  
  if(HAL_RTC_SetAlarm_IT(&RTCHandle,&RTC_Alarm_Structure,RTC_FORMAT_BCD) != HAL_OK)
  {
			BSP_LCD_GLASS_Clear(); 
			BSP_LCD_GLASS_DisplayString((uint8_t *)"A S X");
  }

	__HAL_RTC_ALARM_CLEAR_FLAG(&RTCHandle, RTC_FLAG_ALRAF); //without this line, sometimes(SOMETIMES, when first time to use the alarm interrupt)
																			//the interrupt handler will not work!!! 		

		//need to set/enable the NVIC for RTC_Alarm_IRQn!!!!
	HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);   
	HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 3, 0);  //not important ,but it is better not use the same prio as the systick
	
}

//You may need to disable and enable the RTC Alarm at some moment in your application
HAL_StatusTypeDef  RTC_AlarmA_IT_Disable(RTC_HandleTypeDef *hrtc) 
{ 
 	// Process Locked  
	__HAL_LOCK(hrtc);
  
  hrtc->State = HAL_RTC_STATE_BUSY;
  
  // Disable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);
  
  // __HAL_RTC_ALARMA_DISABLE(hrtc);
    
   // In case of interrupt mode is used, the interrupt source must disabled 
   __HAL_RTC_ALARM_DISABLE_IT(hrtc, RTC_IT_ALRA);


 // Enable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);
  
  hrtc->State = HAL_RTC_STATE_READY; 
  
  // Process Unlocked 
  __HAL_UNLOCK(hrtc);  
}


HAL_StatusTypeDef  RTC_AlarmA_IT_Enable(RTC_HandleTypeDef *hrtc) 
{	
	// Process Locked  
	__HAL_LOCK(hrtc);	
  hrtc->State = HAL_RTC_STATE_BUSY;
  
  // Disable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);
  
  // __HAL_RTC_ALARMA_ENABLE(hrtc);
    
   // In case of interrupt mode is used, the interrupt source must disabled 
   __HAL_RTC_ALARM_ENABLE_IT(hrtc, RTC_IT_ALRA);


 // Enable the write protection for RTC registers 
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);
  
  hrtc->State = HAL_RTC_STATE_READY; 
  
  // Process Unlocked 
  __HAL_UNLOCK(hrtc);  

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
						selpressed=1;	
						break;	
			case GPIO_PIN_1:     //left button						
							leftpressed=1;
							display2TimesCNT++;
							break;
			case GPIO_PIN_2:    //right button						  to play again.
							rightpressed=1;			
							break;
			case GPIO_PIN_3:    //up button							
							BSP_LCD_GLASS_Clear();
							BSP_LCD_GLASS_DisplayString((uint8_t*)"up");
							break;
			
			case GPIO_PIN_5:    //down button						
							BSP_LCD_GLASS_Clear();
							BSP_LCD_GLASS_DisplayString((uint8_t*)"down");
							break;
			case GPIO_PIN_14:    //down button						
							BSP_LCD_GLASS_Clear();
							BSP_LCD_GLASS_DisplayString((uint8_t*)"PE14");
							break;			
			default://
						//default
						break;
	  } 
}



void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  RTC_Flag = 1;	
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
