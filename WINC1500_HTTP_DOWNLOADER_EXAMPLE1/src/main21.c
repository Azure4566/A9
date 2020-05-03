/**************************************************************************/ /**
* @file      main.c
* @brief     Main application entry point
* @author    Eduardo Garcia
* @date      2020-02-15
* @copyright Copyright Bresslergroup\n
*            This file is proprietary to Bresslergroup.
*            All rights reserved. Reproduction or distribution, in whole
*            or in part, is forbidden except by express written permission
*            of Bresslergroup.
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <errno.h>
#include "asf.h"
#include "main.h"
#include "stdio_serial.h"
#include "SerialConsole.h"
#include "FreeRTOS.h"
#include "driver/include/m2m_wifi.h"
#include "CliThread/CliThread.h"
#include "WifiHandlerThread/WifiHandler.h"
#include "SeesawDriver/Seesaw.h"
#include "IMU\lsm6ds_reg.h"
#include "DistanceDriver\DistanceSensor.h"
#include "UiHandlerThread\UiHandlerThread.h"
#include "ControlThread\ControlThread.h"


/******************************************************************************
* Defines and Types
******************************************************************************/
#define APP_TASK_ID 0 /**< @brief ID for the application task */
#define CLI_TASK_ID 1 /**< @brief ID for the command line interface task */
//#define BOOT_TEST	1 //Uncomment me to compile boot test.

/******************************************************************************
* Local Function Declaration
******************************************************************************/
void vApplicationIdleHook(void);
//!< Initial task used to initialize HW before other tasks are initialized
static void StartTasks(void);
void vApplicationDaemonTaskStartupHook(void);
static void TestB(void);
static void TestA(void);
/******************************************************************************
* Variables
******************************************************************************/
static TaskHandle_t cliTaskHandle    = NULL; //!< CLI task handle
static TaskHandle_t daemonTaskHandle    = NULL; //!< Daemon task handle
static TaskHandle_t wifiTaskHandle    = NULL; //!< Wifi task handle
static TaskHandle_t uiTaskHandle    = NULL; //!< UI task handle
static TaskHandle_t controlTaskHandle    = NULL; //!< Control task handle

char bufferPrint[64]; //Buffer for daemon task

/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
	/* Initialize the board. */
	system_init();

	/* Initialize the UART console. */
	InitializeSerialConsole();

	//Initialize trace capabilities
	 vTraceEnable(TRC_START);
    // Start FreeRTOS scheduler
    vTaskStartScheduler();

	return 0; //Will not get here
}

/**************************************************************************/ /**
* function          vApplicationDaemonTaskStartupHook
* @brief            Initialization code for all subsystems that require FreeRToS
* @details			This function is called from the FreeRToS timer task. Any code
*					here will be called before other tasks are initilized.
* @param[in]        None
* @return           None
*****************************************************************************/
void vApplicationDaemonTaskStartupHook(void)
{
#ifdef BOOT_TEST
	//TestA(); //Comment me for Test B
	//TestB(); //Comment me for Test A
#endif
SerialConsoleWriteString("\r\n\r\n-----ESE516 Main Program-----\r\n");

//Initialize HW that needs FreeRTOS Initialization
SerialConsoleWriteString("\r\n\r\nInitialize HW...\r\n");
	if (I2cInitializeDriver() != STATUS_OK)
	{
		SerialConsoleWriteString("Error initializing I2C Driver!\r\n");
	}
	else
	{
		SerialConsoleWriteString("Initialized I2C Driver!\r\n");
	}

	if(0 != InitializeSeesaw())
	{
		SerialConsoleWriteString("Error initializing Seesaw!\r\n");
	}	
	else
	{
		SerialConsoleWriteString("Initialized Seesaw!\r\n");
	}

	uint8_t whoamI = 0;
	(lsm6ds3_device_id_get(GetImuStruct(), &whoamI));
	
	if (whoamI != LSM6DS3_ID){
		SerialConsoleWriteString("Cannot find IMU!\r\n");
	}
	else
	{
		SerialConsoleWriteString("IMU found!\r\n");
	}

	if(InitImu() == 0)
	{
		SerialConsoleWriteString("IMU initialized!\r\n");
	}
	else
	{
		SerialConsoleWriteString("Could not initialize IMU\r\n");
	}

	InitializeDistanceSensor();
	

	StartTasks();

	vTaskSuspend(daemonTaskHandle);
}

/**************************************************************************//**
* function          StartTasks
* @brief            Initialize application tasks
* @details
* @param[in]        None
* @return           None
*****************************************************************************/
static void StartTasks(void)
{

snprintf(bufferPrint, 64, "Heap before starting tasks: %d\r\n", xPortGetFreeHeapSize());
SerialConsoleWriteString(bufferPrint);

//Initialize Tasks here

if (xTaskCreate(vCommandConsoleTask, "CLI_TASK", CLI_TASK_SIZE, NULL, CLI_PRIORITY, &cliTaskHandle) != pdPASS) {
	SerialConsoleWriteString("ERR: CLI task could not be initialized!\r\n");
}

snprintf(bufferPrint, 64, "Heap after starting CLI: %d\r\n", xPortGetFreeHeapSize());
SerialConsoleWriteString(bufferPrint);


if (xTaskCreate(vWifiTask, "WIFI_TASK", WIFI_TASK_SIZE, NULL, WIFI_PRIORITY, &wifiTaskHandle) != pdPASS) {
	SerialConsoleWriteString("ERR: WIFI task could not be initialized!\r\n");
}
snprintf(bufferPrint, 64, "Heap after starting WIFI: %d\r\n", xPortGetFreeHeapSize());
SerialConsoleWriteString(bufferPrint);


if(xTaskCreate(vUiHandlerTask, "UI Task", UI_TASK_SIZE, NULL, UI_TASK_PRIORITY, &uiTaskHandle) != pdPASS) {
	SerialConsoleWriteString("ERR: UI task could not be initialized!\r\n");
}

snprintf(bufferPrint, 64, "Heap after starting UI Task: %d\r\n", xPortGetFreeHeapSize());
SerialConsoleWriteString(bufferPrint);

if(xTaskCreate(vControlHandlerTask, "Control Task", CONTROL_TASK_SIZE, NULL, CONTROL_TASK_PRIORITY, &controlTaskHandle) != pdPASS) {
	SerialConsoleWriteString("ERR: Control task could not be initialized!\r\n");
}
snprintf(bufferPrint, 64, "Heap after starting Control Task: %d\r\n", xPortGetFreeHeapSize());
SerialConsoleWriteString(bufferPrint);
}













static void configure_console(void)
{

	stdio_base = (void *)GetUsartModule();
	ptr_put = (int (*)(void volatile*,char))&usart_serial_putchar;
	ptr_get = (void (*)(void volatile*,char*))&usart_serial_getchar;


	# if defined(__GNUC__)
	// Specify that stdout and stdin should not be buffered.
	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
	// Note: Already the case in IAR's Normal DLIB default configuration
	// and AVR GCC library:
	// - printf() emits one character at a time.
	// - getchar() requests only 1 byte to exit.
	#  endif
	//stdio_serial_init(GetUsartModule(), EDBG_CDC_MODULE, &usart_conf);
	//usart_enable(&cdc_uart_module);
}



#ifdef BOOT_TEST

static void TestA(void)
{
	init_storage();
	SerialConsoleWriteString("Test Program A - LED Toggles every 500ms\r\n");

	FIL file_object; //FILE OBJECT used on main for the SD Card Test
	char test_file_name[] = "0:FlagB.txt";
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT res = f_open(&file_object,
	(char const *)test_file_name,
	FA_CREATE_ALWAYS | FA_WRITE);

	if (res != FR_OK)
	{
		LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
	}
	else
	{
		SerialConsoleWriteString("FlagB.txt added! Hold button pressed to reset device!\r\n");
	}
	f_close(&file_object); //Close file

	while(1)
	{
		port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
		delay_s(1);
		port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		delay_s(1);
		if(port_pin_get_input_level(BUTTON_0_PIN) == false)
		{
			delay_ms(1000);
			if(port_pin_get_input_level(BUTTON_0_PIN) == false)
			{
				system_reset();
			}
		}
	}
}


static void TestB(void)
{
	init_storage();
	SerialConsoleWriteString("Test Program B - LED Toggles every 100 ms second\r\n");

	FIL file_object; //FILE OBJECT used on main for the SD Card Test
	char test_file_name[] = "0:FlagA.txt";
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	FRESULT res = f_open(&file_object,
	(char const *)test_file_name,
	FA_CREATE_ALWAYS | FA_WRITE);

	if (res != FR_OK)
	{
		LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
	}
	else
	{
		SerialConsoleWriteString("FlagA.txt added! Hold button pressed to reset device!\r\n");
	}
	f_close(&file_object); //Close file

	while(1)
	{
		port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
		delay_ms(200);
		port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		delay_ms(200);
		if(port_pin_get_input_level(BUTTON_0_PIN) == false)
		{
			delay_ms(1000);
			if(port_pin_get_input_level(BUTTON_0_PIN) == false)
			{
				system_reset();
			}
			
		}
	}
}

#endif


void vApplicationMallocFailedHook(void)
{
SerialConsoleWriteString("Error on memory allocation on FREERTOS!\r\n");
while(1);
}

void vApplicationStackOverflowHook(void)
{
SerialConsoleWriteString("Error on stack overflow on FREERTOS!\r\n");
while(1);
}

#include "MCHP_ATWx.h"
void vApplicationTickHook (void)
{
SysTick_Handler_MQTT();
}




