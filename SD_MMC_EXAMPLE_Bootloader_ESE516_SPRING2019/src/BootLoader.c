	//define #MEM EXAMPLE
#ifdef MEM_EXAMPLE
	//Example
	//Check out the NVM API at http://asf.atmel.com/docs/latest/samd21/html/group__asfdoc__sam0__nvm__group.html#asfdoc_sam0_nvm_examples . It provides important information too!

	//We will ask the NVM driver for information on the MCU (SAMD21)
	struct nvm_parameters parameters;
	char helpStr[64]; //Used to help print values
	nvm_get_parameters (&parameters); //Get NVM parameters
	snprintf(helpStr, 63,"NVM Info: Number of Pages %d. Size of a page: %d bytes. \r\n", parameters.nvm_number_of_pages, parameters.page_size);
	SerialConsoleWriteString(helpStr);
	//The SAMD21 NVM (and most if not all NVMs) can only erase and write by certain chunks of data size.
	//The SAMD21 can WRITE on pages. However erase are done by ROW. One row is conformed by four (4) pages.
	//An erase is mandatory before writing to a page.
	

	//We will do the following in this example:
	//Erase the first row of the application data
	//Read a page of data from the SD Card. The SD CARD initialization writes a file with this data as its test for initialization
	//Write it to the first row.
	//CheckCRC32 of both files.
	
	uint8_t readBuffer[256]; //Buffer the size of one row
	uint32_t numBytesRead = 0;

	//Erase first page of Main FW
	//This page starts at APP_START_ADDRESS
	enum status_code nvmError = nvm_erase_row(APP_START_ADDRESS);
	if(nvmError != STATUS_OK)
	{
		SerialConsoleWriteString("Erase error");
	}
	
	//Make sure it got erased - we read the page. Erasure in NVM is an 0xFF
	for(int iter = 0; iter < 256; iter++)
	{
		char *a = (char *)(APP_START_ADDRESS + iter); //Pointer pointing to address APP_START_ADDRESS
		if(*a != 0xFF)
		{
			SerialConsoleWriteString("Error - test page is not erased!");
			break;	
		}
	}	

	//Read SD Card File
	test_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res = f_open(&file_object, (char const *)test_bin_file, FA_READ);
		
	if (res != FR_OK)
	{
		SerialConsoleWriteString("Could not open test file!\r\n");
	}

	int numBytesLeft = 256;
	numBytesRead = 0;
	int numberBytesTotal = 0;
	while(numBytesLeft  != 0) 
	{
		
		res = f_read(&file_object, &readBuffer[numberBytesTotal], numBytesLeft, &numBytesRead); //Question to students: What is numBytesRead? What are we doing here?
		numBytesLeft -= numBytesLeft; 
		numberBytesTotal += numBytesRead;
	}
	


	//Write data to first row. Writes are per page, so we need four writes to write a complete row
	res = nvm_write_buffer (APP_START_ADDRESS, &readBuffer[0], 64);
	res = nvm_write_buffer (APP_START_ADDRESS + 64, &readBuffer[64], 64);
	res = nvm_write_buffer (APP_START_ADDRESS + 128, &readBuffer[128], 64);
	res = nvm_write_buffer (APP_START_ADDRESS + 192, &readBuffer[192], 64);

		if (res != FR_OK)
		{
			SerialConsoleWriteString("Test write to NVM failed!\r\n");
		}
		else
		{
			SerialConsoleWriteString("Test write to NVM succeeded!\r\n");
		}

	//CRC32 Calculation example: Please read http://asf.atmel.com/docs/latest/samd21/html/group__asfdoc__sam0__drivers__crc32__group.html

	//CRC of SD Card
	uint32_t resultCrcSd = 0;
	*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;
	enum status_code crcres = dsu_crc32_cal	(readBuffer	,256, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!
	*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
	//CRC of memory (NVM)
	uint32_t resultCrcNvm = 0;
	
	crcres |= dsu_crc32_cal	(readBuffer	,256, &resultCrcNvm);

	if (crcres != STATUS_OK)
	{
		SerialConsoleWriteString("Could not calculate CRC!!\r\n");
	}
	else
	{
		snprintf(helpStr, 63,"CRC SD CARD: %d  CRC NVM: %d \r\n", resultCrcNvm, resultCrcSd);
		SerialConsoleWriteString(helpStr);
	}


#endif