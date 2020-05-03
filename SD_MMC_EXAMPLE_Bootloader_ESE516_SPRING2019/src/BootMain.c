/**************************************************************************//**
* @file      BootMain.c
* @brief     Main file for the ESE516 bootloader. Handles updating the main application
* @details   Main file for the ESE516 bootloader. Handles updating the main application
* @author    Eduardo Garcia
* @date      2020-02-15
* @version   2.0
* @copyright Copyright University of Pennsylvania
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "conf_example.h"
#include <string.h>
#include "sd_mmc_spi.h"

#include "SD Card/SdCard.h"
#include "Systick/Systick.h"
#include "SerialConsole/SerialConsole.h"
#include "ASF/sam0/drivers/dsu/crc32/crc32.h"





/******************************************************************************
* Defines
******************************************************************************/
#define APP_START_ADDRESS  ((uint32_t)0x12000) ///<Start of main application. Must be address of start of main application
#define APP_START_RESET_VEC_ADDRESS (APP_START_ADDRESS+(uint32_t)0x04) ///< Main application reset vector address
#define MEM_EXAMPLE 1 //COMMENT ME TO REMOVE THE MEMORY WRITE EXAMPLE BELOW

/******************************************************************************
* Structures and Enumerations
******************************************************************************/

struct usart_module cdc_uart_module; ///< Structure for UART module connected to EDBG (used for unit test output)

/******************************************************************************
* Local Function Declaration
******************************************************************************/
static int check_for_bootflag(void);
static void copy_binary_file(int BOOTLOADER_FLAG);
static void jumpToApplication(void);
static bool StartFilesystemAndTest(void);
static void configure_nvm(void);


/******************************************************************************
* Global Variables
******************************************************************************/
//INITIALIZE VARIABLES
char FLAG_A[] = "0:FlagA.txt";	///<Test TEXT File A
char FLAG_B[] = "0:FlagB.txt";	///<Test TEXT File B
char BIN_FILE_A[] = "0:TestA.bin"; ///<Test BINARY File A
char BIN_FILE_B[] = "0:TestB.bin"; ///<Test BINARY File B
char test_file_name[] = "0:sd_mmc_test.txt";	///<Test TEXT File name
char test_bin_file[] = "0:sd_binary.bin";	///<Test BINARY File name
Ctrl_status status; ///<Holds the status of a system initialization
FRESULT res; //Holds the result of the FATFS functions done on the SD CARD TEST
FATFS fs; //Holds the File System of the SD CARD
FILINFO fno; //Holds the information of the file
FIL file_object; //FILE OBJECT used on main for the SD Card Test


/******************************************************************************
* Global Functions
******************************************************************************/

/**************************************************************************//**
* @fn		int main(void)
* @brief	Main function for ESE516 Bootloader Application

* @return	Unused (ANSI-C compatibility).
* @note		Bootloader code initiates here.
*****************************************************************************/

int main(void)
{

	/*1.) INIT SYSTEM PERIPHERALS INITIALIZATION*/
	system_init();
	delay_init();
	InitializeSerialConsole();
	system_interrupt_enable_global();
	/* Initialize SD MMC stack */
	sd_mmc_init();

	//Initialize the NVM driver
	configure_nvm();

	irq_initialize_vectors();
	cpu_irq_enable();

	//Configure CRC32
	dsu_crc32_init();

	SerialConsoleWriteString("ESE516 - ENTER BOOTLOADER");	//Order to add string to TX Buffer

	/*END SYSTEM PERIPHERALS INITIALIZATION*/


	/*2.) STARTS SIMPLE SD CARD MOUNTING AND TEST!*/

	//EXAMPLE CODE ON MOUNTING THE SD CARD AND WRITING TO A FILE
	//See function inside to see how to open a file
	SerialConsoleWriteString("\x0C\n\r-- SD/MMC Card Example on FatFs --\n\r");

	if(StartFilesystemAndTest() == false)
	{
		SerialConsoleWriteString("SD CARD failed! Check your connections. System will restart in 5 seconds...");
		delay_cycles_ms(5000);
		system_reset();
	}
	else
	{
		SerialConsoleWriteString("SD CARD mount success! Filesystem also mounted. \r\n");
	}

	/*END SIMPLE SD CARD MOUNTING AND TEST!*/


	/*3.) STARTS BOOTLOADER HERE!*/
	
	#ifdef MEM_EXAMPLE
			//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
			SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER");	//Order to add string to TX Buffer
			delay_cycles_ms(100); //Delay to allow print
		
			//Deinitialize HW - deinitialize started HW here!
			DeinitializeSerialConsole(); //Deinitializes UART
			sd_mmc_deinit(); //Deinitialize SD CARD

			//Jump to application
			jumpToApplication();
	#endif

	//PERFORM BOOTLOADER HERE!
	
	int BOOTLOADER_FLAG = 0;
	SerialConsoleWriteString("Checking boot flag... \r\n");
	BOOTLOADER_FLAG = check_for_bootflag();
	copy_binary_file(BOOTLOADER_FLAG);

	/*END BOOTLOADER HERE!*/

	//Should not reach here! The device should have jumped to the main FW.
	
}







/******************************************************************************
* Static Functions
******************************************************************************/

/**************************************************************************//**
* function      static int check_for_bootflag(void)
* @brief        Check the update flag in the SD card
* @details      Check if the .txt file exists in the SD card to determine whether updating the program or not
* @return       Returns 1 if TextA.txt exists, 2 if TextB.txt exists, 0 if no update flag
******************************************************************************/
static int check_for_bootflag(void)
{
		
	res = f_open(&file_object, (char const *)FLAG_A, FA_READ);

	if (res != FR_OK)
	{
		res = f_open(&file_object, (char const *)FLAG_B, FA_READ);
		if (res != FR_OK)
		{
			SerialConsoleWriteString("ERROR - NO BOOTFLAG!\r\n");
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			//Return flag value
			return 0;
		} else 
		{
			SerialConsoleWriteString("LOADING BOOTFLAG B\r\n");
			delay_cycles_ms(100); //Delay to allow print
			f_close(&file_object);
			f_unlink((char const *)FLAG_B);
			SerialConsoleWriteString("FlagB.txt Deleted");
			//Return flag value
			return 2;
		}
	} else 
	{
		SerialConsoleWriteString("LOADING BOOTFLAG A\r\n");
		delay_cycles_ms(100); //Delay to allow print
		//Close and delete the flag
		f_close(&file_object);
		f_unlink((char const *)FLAG_A);
		SerialConsoleWriteString("FlagA.txt Deleted");
		//Return flag value
		return 1;
	}
}

/**************************************************************************//**
* function      static void copy_binary_file(int BOOTLOADER_FLAG)
* @brief        Copy the binary file in the SD card to the NVM
* @details      Copy the indicated binary file on a row-by-row basis
* @param        BOOTLOADER_FLAG: the flag indicates which binary file to load
* @return  
******************************************************************************/

static void copy_binary_file(int BOOTLOADER_FLAG)
{
	struct nvm_parameters parameters;
	char helpStr[64]; //Used to help print values
	nvm_get_parameters (&parameters); //Get NVM parameters
	snprintf(helpStr, 63,"NVM Info: Number of Pages %d. Size of a page: %d bytes. \r\n", parameters.nvm_number_of_pages, parameters.page_size);
	SerialConsoleWriteString(helpStr);
	
	if (BOOTLOADER_FLAG == 1)
	{	
		//Open the binary file in the SD card
		BIN_FILE_A[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object, (char const *)BIN_FILE_A, FA_READ);
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open TestA.bin!\r\n");
		}
		//Debug output
		SerialConsoleWriteString("TestA.bin Loaded\r\n");
		
		//Check the file size
		res = f_stat((char const *)BIN_FILE_A, &fno);
		int numSize = fno.fsize;
		int numTotalRows = (numSize-1)/256 + 1;
		//Debug output
		//SerialConsoleWriteString("TestA.bin Size is: ");
		//SerialConsoleWriteString("\r\n");
		//SerialConsoleWriteString("TestA.bin Size in rows is: ");
		//SerialConsoleWriteString("\r\n");
		
		//Write data to NVM in current row. Writes are per page, so we need four writes to write a complete row
		uint32_t CURRENT_ROW_START_ADDRESS = APP_START_ADDRESS;
		//ERASE-READ-WRITE the file in rows, in total row number of the file
		for (int ROW = 0; ROW < numTotalRows ; ROW++)
		{
			//SerialConsoleWriteString("a");
			//Erase the current row
			enum status_code nvmError = nvm_erase_row(APP_START_ADDRESS + ROW * 256);
			if(nvmError != STATUS_OK)
			{
				SerialConsoleWriteString("Erase error");
			}
			
			//Advance the file pointer to current row			
			f_lseek(&file_object, ROW * 256);
			
			uint8_t readBuffer[256]; //Buffer the size of one row
			uint32_t numBytesRead = 0;
			
			int numBytesLeft = 256;
			numBytesRead = 0;
			int numberBytesTotal = 0;
			//READ BINARY FILE INTO BUFFER by row
			while(numBytesLeft != 0) 
			{
				res = f_read(&file_object, &readBuffer[numberBytesTotal], numBytesLeft, &numBytesRead); //Question to students: What is numBytesRead? What are we doing here?
				numBytesLeft -= numBytesRead; 
				numberBytesTotal += numBytesRead;
				
				if (ROW == (numTotalRows - 1) && numBytesRead == (numSize - ROW *256))
				{
					break;
				}
				
			}
			//Debug output
			//SerialConsoleWriteString("Read a row \r\n");

	

			//Update current row start address pointer
			CURRENT_ROW_START_ADDRESS = APP_START_ADDRESS + ROW * 256;
			//Write buffer in one row
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS, &readBuffer[0], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 64, &readBuffer[64], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 128, &readBuffer[128], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 192, &readBuffer[192], 64);
			//Debug output
			if (res != FR_OK)
			{
				SerialConsoleWriteString("Test write to NVM failed!\r\n");
			}
			
			//CRC of SD Card
			uint32_t resultCrcSd = 0xFFFFFFFF;
			*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;
			enum status_code crcres = dsu_crc32_cal	(readBuffer	,256, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!
			*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
			//CRC of memory (NVM)
			uint32_t resultCrcNvm = 0xFFFFFFFF;
			crcres |= dsu_crc32_cal	(CURRENT_ROW_START_ADDRESS,256, &resultCrcNvm);

			//Check CRC
			if (crcres != STATUS_OK)
			{
				SerialConsoleWriteString("Could not calculate CRC!!\r\n");
			}
			else
			{
				snprintf(helpStr, 63,"CRC SD CARD: %lu  CRC NVM: %lu \r\n", resultCrcNvm, resultCrcSd);
				//SerialConsoleWriteString(helpStr);
				if (resultCrcNvm != resultCrcSd)
				{
					snprintf(helpStr, 63,"Error Detected While Copying Row %d \r\n", ROW);
					SerialConsoleWriteString(helpStr);
				}
				else
				{
					//SerialConsoleWriteString("Copy Verified! \r\n");
				}
			}
		}
		
		//Close the file
		res = f_lseek(&file_object, 0);
		res = f_close(&file_object);
		//Debug output
		if (res == FR_OK)
		{
			SerialConsoleWriteString("TestA.bin Closed");
		} else 
		{
			SerialConsoleWriteString("ERROR: TestA.bin Cannot be Closed");
		}
		
		//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
		SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER");	//Order to add string to TX Buffer
		delay_cycles_ms(100); //Delay to allow print
		
		//Deinitialize HW - deinitialize started HW here!
		DeinitializeSerialConsole(); //Deinitializes UART
		sd_mmc_deinit(); //Deinitialize SD CARD

		//Jump to application
		jumpToApplication();
		
	} else if (BOOTLOADER_FLAG == 2)
	{	
		//Open the binary file in the SD card
		BIN_FILE_B[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object, (char const *)BIN_FILE_B, FA_READ);
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open TestB.bin!\r\n");
		}
		//Debug output
		SerialConsoleWriteString("TestB.bin Loaded\r\n");
		
		//Check the file size
		res = f_stat((char const *)BIN_FILE_B, &fno);
		int numSize = fno.fsize;
		int numTotalRows = (numSize-1)/256 + 1;
		//Debug output
		SerialConsoleWriteString("TestB.bin Size is: ");
		delay_cycles_ms(100); //Delay to allow print
		SerialConsoleWriteString((char)numSize);
		SerialConsoleWriteString("\r\n");
		SerialConsoleWriteString("TestB.bin Size in rows is: ");
		delay_cycles_ms(100); //Delay to allow print
		SerialConsoleWriteString((char)numTotalRows);
		SerialConsoleWriteString("\r\n");
		
		//Write data to NVM in current row. Writes are per page, so we need four writes to write a complete row
		uint32_t CURRENT_ROW_START_ADDRESS = APP_START_ADDRESS;
		//Read the file in rows, in total row number of the file
		for (int ROW = 0; ROW < numTotalRows ; ROW++)
		{
			//Erase the current row
			enum status_code nvmError = nvm_erase_row(APP_START_ADDRESS + ROW * 256);
			if(nvmError != STATUS_OK)
			{
				SerialConsoleWriteString("Erase error");
			}
			
			//Advance the file pointer to current row			
			f_lseek(&file_object, ROW * 256);
			
			uint8_t readBuffer[256]; //Buffer the size of one row
			uint32_t numBytesRead = 0;
			
			int numBytesLeft = 256;
			numBytesRead = 0;
			int numberBytesTotal = 0;
			//READ BINARY FILE INTO BUFFER by row
			while(numBytesLeft  != 0) 
			{
				res = f_read(&file_object, &readBuffer[numberBytesTotal], numBytesLeft, &numBytesRead); //Question to students: What is numBytesRead? What are we doing here?
				numBytesLeft -= numBytesRead; 
				numberBytesTotal += numBytesRead;
				
				if (ROW == (numTotalRows - 1) && numBytesRead == (numSize - ROW *256))
				{
					break;
				}
			}
			//Debug output
			//SerialConsoleWriteString("Read a row \r\n");

			//Update current row start address pointer
			CURRENT_ROW_START_ADDRESS = APP_START_ADDRESS + ROW * 256;
			//Write buffer in one row
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS, &readBuffer[0], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 64, &readBuffer[64], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 128, &readBuffer[128], 64);
			res = nvm_write_buffer (CURRENT_ROW_START_ADDRESS + 192, &readBuffer[192], 64);
			//Debug output
			if (res != FR_OK)
			{
				SerialConsoleWriteString("Test write to NVM failed!\r\n");
			}

			//CRC of SD Card
			uint32_t resultCrcSd = 0xFFFFFFFF;
			*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;
			enum status_code crcres = dsu_crc32_cal	(readBuffer	,256, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!
			*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
			
			//CRC of memory (NVM)
			uint32_t resultCrcNvm = 0xFFFFFFFF;
			crcres |= dsu_crc32_cal	(CURRENT_ROW_START_ADDRESS,256, &resultCrcNvm);

			//Check CRC
			if (crcres != STATUS_OK)
			{
				SerialConsoleWriteString("Could not calculate CRC!!\r\n");
			}
			else
			{
				snprintf(helpStr, 63,"CRC SD CARD: %lu  CRC NVM: %lu \r\n", resultCrcNvm, resultCrcSd);
				//SerialConsoleWriteString(helpStr);
				if (resultCrcNvm != resultCrcSd)
				{
					snprintf(helpStr, 63,"Error Detected While Copying Row %d \r\n", ROW);
					SerialConsoleWriteString(helpStr);
				}
				else
				{
					//SerialConsoleWriteString("Copy Verified! \r\n");
				}
			}
		}
		//Close the file
		res = f_lseek(&file_object, 0);
		res = f_close(&file_object);
		//Debug output
		if (res == FR_OK)
		{
			SerialConsoleWriteString("TestB.bin Closed");
		} else 
		{
			SerialConsoleWriteString("ERROR: TestB.bin Cannot be Closed");
		}
		
		//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
		SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER \r\n");	//Order to add string to TX Buffer
		delay_cycles_ms(100); //Delay to allow print
		
		//Deinitialize HW - deinitialize started HW here!
		DeinitializeSerialConsole(); //Deinitializes UART
		sd_mmc_deinit(); //Deinitialize SD CARD

		//Jump to application
		jumpToApplication();
		
	} else
	{
		//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
		SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER \r\n");	//Order to add string to TX Buffer
		delay_cycles_ms(100); //Delay to allow print
		
		//Deinitialize HW - deinitialize started HW here!
		DeinitializeSerialConsole(); //Deinitializes UART
		sd_mmc_deinit(); //Deinitialize SD CARD

		//Jump to application
		jumpToApplication();
	}
}


/**************************************************************************//**
* function      static void StartFilesystemAndTest()
* @brief        Starts the filesystem and tests it. Sets the filesystem to the global variable fs
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       Returns true is SD card and file system test passed. False otherwise.
******************************************************************************/
static bool StartFilesystemAndTest(void)
{
	bool sdCardPass = true;
	uint8_t binbuff[256];

	//Before we begin - fill buffer for binary write test
	//Fill binbuff with values 0x00 - 0xFF
	for(int i = 0; i < 256; i++)
	{
		binbuff[i] = i;
	}

	//MOUNT SD CARD
	Ctrl_status sdStatus= SdCard_Initiate();
	if(sdStatus == CTRL_GOOD) //If the SD card is good we continue mounting the system!
	{
		SerialConsoleWriteString("SD Card initiated correctly!\n\r");

		//Attempt to mount a FAT file system on the SD Card using FATFS
		SerialConsoleWriteString("Mount disk (f_mount)...\r\n");
		memset(&fs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs); //Order FATFS Mount
		if (FR_INVALID_DRIVE == res)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}
		SerialConsoleWriteString("[OK]\r\n");

		//Create and open a file
		SerialConsoleWriteString("Create a file (f_open)...\r\n");

		test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object,
		(char const *)test_file_name,
		FA_CREATE_ALWAYS | FA_WRITE);
		
		if (res != FR_OK)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");

		//Write to a file
		SerialConsoleWriteString("Write to test file (f_puts)...\r\n");

		if (0 == f_puts("Test SD/MMC stack\n", &file_object))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");


		//Write binary file
		//Read SD Card File
		test_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object, (char const *)test_bin_file, FA_WRITE | FA_CREATE_ALWAYS);
		
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open binary file!\r\n");
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		//Write to a binaryfile
		SerialConsoleWriteString("Write to test file (f_write)...\r\n");
		uint32_t varWrite = 0;
		if (0 != f_write(&file_object, binbuff,256, &varWrite))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");
		
		main_end_of_test:
		SerialConsoleWriteString("End of Test.\n\r");

	}
	else
	{
		SerialConsoleWriteString("SD Card failed initiation! Check connections!\n\r");
		sdCardPass = false;
	}

	return sdCardPass;
}



/**************************************************************************//**
* function      static void jumpToApplication(void)
* @brief        Jumps to main application
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       
******************************************************************************/
static void jumpToApplication(void)
{
// Function pointer to application section
void (*applicationCodeEntry)(void);

// Rebase stack pointer
__set_MSP(*(uint32_t *) APP_START_ADDRESS);

// Rebase vector table
SCB->VTOR = ((uint32_t) APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

// Set pointer to application section
applicationCodeEntry =
(void (*)(void))(unsigned *)(*(unsigned *)(APP_START_RESET_VEC_ADDRESS));

// Jump to application. By calling applicationCodeEntry() as a function we move the PC to the point in memory pointed by applicationCodeEntry, 
//which should be the start of the main FW.
applicationCodeEntry();
}



/**************************************************************************//**
* function      static void configure_nvm(void)
* @brief        Configures the NVM driver
* @details      
* @return       
******************************************************************************/
static void configure_nvm(void)
{
    struct nvm_config config_nvm;
    nvm_get_config_defaults(&config_nvm);
    config_nvm.manual_page_write = false;
    nvm_set_config(&config_nvm);
}



