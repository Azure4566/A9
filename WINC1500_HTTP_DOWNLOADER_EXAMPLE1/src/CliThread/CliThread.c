/**************************************************************************//**
* @file      CliThread.c
* @brief     File for the CLI Thread handler. Uses FREERTOS + CLI
* @author    Eduardo Garcia
* @date      2020-02-15

******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include "CliThread.h"
#include "IMU/lsm6ds_reg.h"
#include "SeesawDriver/Seesaw.h"
#include "WifiHandlerThread/WifiHandler.h"
#include "DistanceDriver/DistanceSensor.h"

/******************************************************************************
* Defines
******************************************************************************/


/******************************************************************************
* Variables
******************************************************************************/
static const int8_t * const pcWelcomeMessage =
"FreeRTOS CLI.\r\nType Help to view a list of registered commands.\r\n";


static const CLI_Command_Definition_t xImuGetCommand =
{
	"imu",
	"imu: Returns a value from the IMU\r\n",
	CLI_GetImuData,
	0
};

static const CLI_Command_Definition_t xOTAUCommand =
{
	"fw",
	"fw: Download a file and perform an FW update\r\n",
	CLI_OTAU,
	0
};

static const CLI_Command_Definition_t xResetCommand =
{
	"reset",
	"reset: Resets the device\r\n",
	CLI_ResetDevice,
	0
};

static const CLI_Command_Definition_t xNeotrellisTurnLEDCommand =
{
	"led",
	"led [keynum][R][G][B]: Sets the given LED to the given R,G,B values.\r\n",
	CLI_NeotrellisSetLed,
	4
};

static const CLI_Command_Definition_t xNeotrellisProcessButtonCommand =
{
	"getbutton",
	"getbutton: Processes and prints the FIFO button buffer from the seesaw.\r\n",
	CLI_NeotrellProcessButtonBuffer,
	0
};

static const CLI_Command_Definition_t xDistanceSensorGetDistance =
{
	"getdistance",
	"getdistance: Returns the distance from the US-100 Sensor.\r\n",
	CLI_DistanceSensorGetDistance,
	0
};


static const CLI_Command_Definition_t xSendDummyGameData =
{
 "game",
 "game: Sends dummy game data\r\n",
 CLI_SendDummyGameData,
 0
};

//Clear screen command
const CLI_Command_Definition_t xClearScreen =
{
	CLI_COMMAND_CLEAR_SCREEN,
	CLI_HELP_CLEAR_SCREEN,
	CLI_CALLBACK_CLEAR_SCREEN,
	CLI_PARAMS_CLEAR_SCREEN
};


/******************************************************************************
* Forward Declarations
******************************************************************************/

/******************************************************************************
* Callback Functions
******************************************************************************/


/******************************************************************************
* CLI Thread
******************************************************************************/

void vCommandConsoleTask( void *pvParameters )
{
//REGISTER COMMANDS HERE
FreeRTOS_CLIRegisterCommand( &xOTAUCommand);
FreeRTOS_CLIRegisterCommand( &xImuGetCommand );
FreeRTOS_CLIRegisterCommand( &xClearScreen );
FreeRTOS_CLIRegisterCommand( &xResetCommand );
FreeRTOS_CLIRegisterCommand( &xNeotrellisTurnLEDCommand );
FreeRTOS_CLIRegisterCommand( &xNeotrellisProcessButtonCommand );
FreeRTOS_CLIRegisterCommand( &xDistanceSensorGetDistance);
FreeRTOS_CLIRegisterCommand( &xSendDummyGameData);

uint8_t cRxedChar[2], cInputIndex = 0;
BaseType_t xMoreDataToFollow;
/* The input and output buffers are declared static to keep them off the stack. */
static int8_t pcOutputString[ MAX_OUTPUT_LENGTH_CLI  ], pcInputString[ MAX_INPUT_LENGTH_CLI ];
static char pcLastCommand[ MAX_INPUT_LENGTH_CLI ];
static bool isEscapeCode = false;
static char pcEscapeCodes [4];
static uint8_t pcEscapeCodePos = 0;

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */

    /* Send a welcome message to the user knows they are connected. */
    SerialConsoleWriteString( pcWelcomeMessage);

    for( ;; )
    {
        /* This implementation reads a single character at a time.  Wait in the
        Blocked state until a character is received. */
        int recv = SerialConsoleReadCharacter(&cRxedChar);
		if(recv == -1) //If no characters in the buffer, thread goes to sleep for a while
		{
			vTaskDelay( CLI_TASK_DELAY);
		}else if( cRxedChar[0] == '\n' || cRxedChar[0] == '\r'  )
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            SerialConsoleWriteString("\r\n");
			//Copy for last command
			isEscapeCode = false; pcEscapeCodePos = 0;
			strncpy(pcLastCommand, pcInputString, MAX_INPUT_LENGTH_CLI-1);
			pcLastCommand[MAX_INPUT_LENGTH_CLI-1] = 0;	//Ensure null termination

            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            explanation of why this is. */
            do
            {
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand
                              (
                                  pcInputString,   /* The command string.*/
                                  pcOutputString,  /* The output buffer. */
                                  MAX_OUTPUT_LENGTH_CLI/* The size of the output buffer. */
                              );

                /* Write the output generated by the command interpreter to the
                console. */
				//Ensure it is null terminated
				pcOutputString[MAX_OUTPUT_LENGTH_CLI - 1] = 0;
                SerialConsoleWriteString(pcOutputString);

            } while( xMoreDataToFollow != pdFALSE );

            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
        }
        else
        {
		            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */
		
			if (true == isEscapeCode) {

				if(pcEscapeCodePos < CLI_PC_ESCAPE_CODE_SIZE) {
					pcEscapeCodes[pcEscapeCodePos++] = cRxedChar[0];
				}
				else {
					isEscapeCode = false; pcEscapeCodePos = 0;
				}
			
				if (pcEscapeCodePos >= CLI_PC_MIN_ESCAPE_CODE_SIZE) {
				
					// UP ARROW SHOW LAST COMMAND
					if(strcasecmp(pcEscapeCodes, "oa"))	{
                            /// Delete current line and add prompt (">")
                            sprintf(pcInputString, "%c[2K\r>", 27);
				            SerialConsoleWriteString(pcInputString);
                            /// Clear input buffer
                            cInputIndex = 0;
                            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
                        /// Send last command
						strncpy(pcInputString, pcLastCommand, MAX_INPUT_LENGTH_CLI - 1); 	
                        cInputIndex = (strlen(pcInputString) < MAX_INPUT_LENGTH_CLI - 1) ? strlen(pcLastCommand) : MAX_INPUT_LENGTH_CLI - 1;
						SerialConsoleWriteString(pcInputString);
					}
				
					isEscapeCode = false; pcEscapeCodePos = 0;
				}			
			}
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            else if( cRxedChar[0] == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( cRxedChar[0] == ASCII_BACKSPACE || cRxedChar[0] == ASCII_DELETE )
            {
				char erase[4] = {0x08, 0x20, 0x08, 0x00};
				SerialConsoleWriteString(erase);
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = 0;
                }
            }
			// ESC
			else if( cRxedChar[0] == ASCII_ESC) {
				isEscapeCode = true; //Next characters will be code arguments
				pcEscapeCodePos = 0;
			}
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a n is entered the complete
                string will be passed to the command interpreter. */
                if( cInputIndex < MAX_INPUT_LENGTH_CLI )
                {
                    pcInputString[ cInputIndex ] = cRxedChar[0];
                    cInputIndex++;
                }

					//Order Echo
					cRxedChar[1] = 0;
					SerialConsoleWriteString(&cRxedChar[0]);
            }
        }
    }
}




/******************************************************************************
* CLI Functions - Define here
******************************************************************************/
//Example CLI Command. Reads from the IMU and returns data.
BaseType_t CLI_GetImuData( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
static axis3bit16_t data_raw_acceleration;
static axis3bit16_t data_raw_angular_rate;
static float acceleration_mg[3];
uint8_t reg;

lsm6ds3_xl_flag_data_ready_get(GetImuStruct(), &reg);
if(reg){
	memset(data_raw_acceleration.u8bit, 0x00, 3 * sizeof(int16_t));
	lsm6ds3_acceleration_raw_get(GetImuStruct(), data_raw_acceleration.u8bit);
	acceleration_mg[0] =
	lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[0]);
	acceleration_mg[1] =
	lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[1]);
	acceleration_mg[2] =
	lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[2]);

snprintf(pcWriteBuffer,xWriteBufferLen, "Acceleration [mg]:X %d\tY %d\t%Z %d\r\n",
(int)acceleration_mg[0], (int)acceleration_mg[1], (int)acceleration_mg[2]);
}else
{
	snprintf(pcWriteBuffer,xWriteBufferLen, "No data ready! \r\n");
}

struct ImuDataPacket imuPacketTemp;

imuPacketTemp.xmg = (int16_t)acceleration_mg[0];
imuPacketTemp.ymg = (int16_t)acceleration_mg[1];
imuPacketTemp.zmg = (int16_t)acceleration_mg[2];
int error = WifiAddImuDataToQueue(&imuPacketTemp);
if(error == pdTRUE)
{
	strcat(pcWriteBuffer, "IMU Data MQTT Post\r\n");
}


return pdFALSE;
}


//THIS COMMAND USES vt100 TERMINAL COMMANDS TO CLEAR THE SCREEN ON A TERMINAL PROGRAM LIKE TERA TERM
//SEE http://www.csie.ntu.edu.tw/~r92094/c++/VT100.html for more info
//CLI SPECIFIC COMMANDS
static char bufCli[CLI_MSG_LEN];
BaseType_t xCliClearTerminalScreen( char *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	char clearScreen = ASCII_ESC;
	snprintf(bufCli, CLI_MSG_LEN - 1, "%c[2J", clearScreen);
	snprintf(pcWriteBuffer, xWriteBufferLen, bufCli);
	return pdFALSE;
}



//Example CLI Command. Reads from the IMU and returns data.
BaseType_t CLI_OTAU( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	WifiHandlerSetState(WIFI_DOWNLOAD_INIT);

return pdFALSE;
}

//Example CLI Command. Resets system.
BaseType_t CLI_ResetDevice( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	system_reset();
	return pdFALSE;
}

/**************************************************************************//**
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	CLI command to turn on a given LED to a given R,G,B, value
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	//Check code SeesawSetLed and SeesawSetLed
	//How do you get parameters? Checl link in comments!
	//Check that the input is sanitized: Key between 0-15, RGB between 0-255. Print if there is an error!
	//return pdFalse to tell the FreeRTOS CLI your call is done and does not need to call again.
	//This function expects 4 arguments inside pcCommandString: key, R, G, B.
	int8_t *pcParameter1, *pcParameter2, *pcParameter3, *pcParameter4;
	uint8_t p1, p2, p3, p4;
	BaseType_t xParameter1StringLength, xParameter2StringLength,
	xParameter3StringLength, xParameter4StringLength;
	
	//Obtain parameters
	pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParameter1StringLength);
	pcParameter2 = FreeRTOS_CLIGetParameter(pcCommandString, 2, &xParameter2StringLength);
	pcParameter3 = FreeRTOS_CLIGetParameter(pcCommandString, 3, &xParameter3StringLength);
	pcParameter4 = FreeRTOS_CLIGetParameter(pcCommandString, 4, &xParameter4StringLength);
	
	//Terminate parameters
	pcParameter1[ xParameter1StringLength ] = 0x00;
	pcParameter2[ xParameter2StringLength ] = 0x00;
	pcParameter3[ xParameter3StringLength ] = 0x00;
	pcParameter4[ xParameter4StringLength ] = 0x00;
	
	//Convert to int
	p1 = atoi(pcParameter1);
	p2 = atoi(pcParameter2);
	p3 = atoi(pcParameter3);
	p4 = atoi(pcParameter4);
	
	if (p1 < 0 || p1 > 15) {
		snprintf( pcWriteBuffer, xWriteBufferLen, "Error - Keynum out of range!\r\n" );
		return pdFALSE;
		} else {
		*pcWriteBuffer = NULL;
	}
	
	if (p2 < 0 || p2 > 255) {
		snprintf( pcWriteBuffer, xWriteBufferLen, "Error - Red LED brightness out of range!\r\n" );
		return pdFALSE;
		} else {
		*pcWriteBuffer = NULL;
	}
	
	if (p3 < 0 || p3 > 255) {
		snprintf( pcWriteBuffer, xWriteBufferLen, "Error - Green LED brightness out of range!\r\n");
		return pdFALSE;
		} else {
		*pcWriteBuffer = NULL;
	}
	
	if (p4 < 0 || p4 > 255) {
		snprintf( pcWriteBuffer, xWriteBufferLen, "Error - Blue LED brightness out of range!\r\n");
		return pdFALSE;
		} else {
		*pcWriteBuffer = NULL;
	}
	
	int error = SeesawSetLed(p1, p2, p3, p4);
	SeesawOrderLedUpdate();
	
	return pdFALSE;
}



/**************************************************************************//**
BaseType_t CLI_NeotrellProcessButtonBuffer( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	CLI command to process the Neotrellis FIFO button buffer. The Seesaw driver will store all button events until we read them.
This function will read all the events in the buffer and print the action of each one. For example this is a print:
		Key 10 pressed
		Key 11 pressed
		Key 11 released
		Key 10 released
		The function will print "Buffer Empty" if there is nothing on the button buffer.
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
BaseType_t CLI_NeotrellProcessButtonBuffer( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{

	//See functions SeesawGetKeypadCount and SeesawReadKeypad
	//Returns the number of key events currently on the Seesaw Keypad
	
	//Returns the number of requested events in the Seesaw FIFO buffer into the buffer variable
	
	//	NEO_TRELLIS_SEESAW_KEY(number) ;
	//snprintf(pcWriteBuffer,xWriteBufferLen, "count: %d\num_req_eve:%d\t\n",count,num_req_eve);
	//Print to pcWriteBuffer in order.
	//If the string is too long to print, print what you can.
	//The function you write will be useful in the future.
	uint8_t buffer[64];
	uint8_t count = SeesawGetKeypadCount();
	if(count >= 1)
	{
	int32_t res = SeesawReadKeypad(buffer,1);
	if(res==0)
		{
			uint8_t pos,press;
			press = buffer[0] & 0x3;
			pos =  buffer[0]>>2;
			int num = NEO_TRELLIS_SEESAW_KEY(pos);
			if(press == 0x2){
				snprintf( pcWriteBuffer, xWriteBufferLen, "Button #%d is released\r\n",NEO_TRELLIS_SEESAW_KEY(num));
			}
			else if(press == 0x3){
				snprintf( pcWriteBuffer, xWriteBufferLen, "Button #%d is pressed\r\n",NEO_TRELLIS_SEESAW_KEY(num));
			}
		}
		return pdTRUE;
	}
	else
	{
	pcWriteBuffer = 0;
		return pdFALSE;
	}
	

}



/**************************************************************************//**
BaseType_t CLI_DistanceSensorGetDistance( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	Returns distance in mm
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
BaseType_t CLI_DistanceSensorGetDistance( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{

	uint16_t distance = 0;
	int error = DistanceSensorGetDistance(&distance, 100);
	if (0 != error )
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "Sensor Error %d!\r\n", error);
	}
	else
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "Distance: %d mm\r\n", distance);
	}

	error = WifiAddDistanceDataToQueue(&distance);
	if(error == pdTRUE)
	{
		strcat(pcWriteBuffer, "Distance Data MQTT Post\r\n");
	}
	return pdFALSE;
}



/**************************************************************************//**
BaseType_t CLI_SendDummyGameData( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	Returns dummy game data
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
BaseType_t CLI_SendDummyGameData( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
struct GameDataPacket gamevar;

gamevar.game[0] = 0;
gamevar.game[1] = 1;
gamevar.game[2] = 2;
gamevar.game[3] = 3;
gamevar.game[4] = 4;
gamevar.game[5] = 5;
gamevar.game[6] = 6;
gamevar.game[7] = 7;
gamevar.game[8] = 8;
gamevar.game[9] = 9;
gamevar.game[10] = 0xFF;

	int error = WifiAddGameDataToQueue(&gamevar);
	if(error == pdTRUE)
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "Dummy Game Data MQTT Post\r\n");
	}
	return pdFALSE;
}
