/*
 * Copyright (C) 2024 BURL Concepts, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "burldaq.h"

//Definitions
#define FALSE 0
#define TRUE 1

#define DAQ_SUCCESS             0
#define ERROR_DAQ_NOT_FOUND     101
#define ERROR_DAQ_GET_TIME      102
#define ERROR_FILE_OPEN         103
#define ERROR_NO_FILE_NAME      104
#define ERROR_DAQ_CAPTURE		105

//Configuration
//Default SPI Frequencey
#define DEFAULT_FREQ        10   //40 MHz is the default (current one max 10 Mhz)

#define DEFAULT_CAPTURE_PERIOD  1
#define NUMBER_OF_SAMPLES_PER_USB_SCAN  256   // Number of samples per USB Read
#define BATTERY_SCALE_DOWN_FACTOR 2
#define DEFAULT_CAPTURE_SIZE_IN_BYTES   2048



//global variables
  int spi_fd; //spi interface for DAQ
  int flag;
  int i, j, sample;
  int temp, input;
  int ch;
  int nScanFailures = 0;
  __u8 spiFrequency = DEFAULT_FREQ;
  __u32 period;
  __u16 version;
  __u16 sdataOut[128];   // holds 12 bit unsigned analog output data
  __u16 sdataIn[256];    // holds 13 bit unsigned analog input data
  char serial[9];
  char daqVersion[32];
  char result_raw_filename[256];
  char result_processed_filename[256];
  char TimeString[64];
  __u16 value;
  bool debugMode = false;
  bool isDeviceConnected = false;
  int isScanReady = FALSE;
  double frequency = DEFAULT_FREQ;

//Private Functions
//Function to create directory on raspberry pi file system
static void _mkdir(const char *dir) {
           char tmp[256];
           char *p = NULL;
           size_t len;

           snprintf(tmp, sizeof(tmp),"%s",dir);
           len = strlen(tmp);
           if(tmp[len - 1] == '/')
                   tmp[len - 1] = 0;
           for(p = tmp + 1; *p; p++)
                   if(*p == '/') {
                           *p = 0;
                           mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                           *p = '/';
                   }
           mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
   }


//Check file path and create directories if doesn't exist
int checkResultFilePath()
   {
       int retValue = DAQ_SUCCESS;
        //extract the path from the file
       char *lastSlash = NULL;
       char *parent = NULL;

       //Check whether we have a file name
       if(result_raw_filename[0] == '\0') return ERROR_NO_FILE_NAME;


       lastSlash = strrchr(result_raw_filename, '/');
       parent = strndup(result_raw_filename, (strlen(result_raw_filename) - strlen(lastSlash)));

       DIR* dir = opendir(parent);
       if (!dir)
       {
       /* Directory  doesnt exists, create now */
       if(debugMode) printf("Creating path : %s\n", parent) ;
       _mkdir(parent);
       }
       closedir(dir);

       return retValue;
}


//Public functions
//Config DAQ
int ConfigureDAQ()
{
  int retValue = DAQ_SUCCESS;
  if(debugMode) printf("ConfigureDAQ\n");
  //TODO - if any additional configurations needed
  return retValue;
}

//Init DAQ
int DAQ_Init()
{
	if(debugMode) printf("In DAQ Native Code : DAQ_Init() called..\n");
	int retValue = DAQ_SUCCESS;
	int i;

	// Open the SPI device (New DAQ is connected to SPI 1.1 on Raspberry PI)
	spi_fd = open("/dev/spidev1.1", O_RDWR);
	if (spi_fd < 0) {
		perror("Error opening SPI device");
		return ERROR_DAQ_NOT_FOUND;
	}
	else
	{
		printf("SPI connection is successful");
		isDeviceConnected = true;
		close(spi_fd); //we verified that the spi connection opened
		//close it now until the next time we need it in StartDAQCapture
	}

	return retValue;
}

//Function to interact with DAQ over SPI and receives the data
//This function should be called when a capture is triggered, if the trigger is needed
//In this example, for reading voltage or configuration data, the trigger is not mandatory
int StartDAQCapture()
{
    int retValue = DAQ_SUCCESS;
    int nBytes = -1;

    //Make sure the file is opened
    FILE *fp = NULL;
    if(debugMode) printf("create a file \n");
    fp=fopen(result_raw_filename,"wb");
    if(debugMode) printf("created a file \n");
    if(fp == NULL) return ERROR_FILE_OPEN;

    //Data buffer
    uint8_t tx_data[DEFAULT_CAPTURE_SIZE_IN_BYTES] = {0x00};  // placeholder data to be sent
    uint8_t rx_data[sizeof(tx_data)];  // Buffer for received data

    //open the spidev everytime so that we can power it down externally when we want to
    int spi_fd = open("/dev/spidev1.1", O_RDWR);
    if (spi_fd < 0) {
        perror("Could not open SPI device");
        return ERROR_DAQ_CAPTURE;
    }

	// Set SPI parameters
	int mode = SPI_MODE_0;  // Set SPI mode 0
	ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

	int bits = 8;  // Set data word size to 8 bits
	ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);

	int speed_hz = spiFrequency * 1000000;  // Set SPI clock speed to configured frequency
	if(debugMode) printf("SPI Frequency :%d", speed_hz);
	ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);

	// Perform SPI transfer
	struct spi_ioc_transfer spi_transfer = {
		.tx_buf = (unsigned long)tx_data,
		.rx_buf = (unsigned long)rx_data,
		.len = sizeof(tx_data),
	};

	int ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &spi_transfer);
		//close the spidev everytime so that we can still power it down externally when we want to
	close(spi_fd);

	if (ret < 0) {
		perror("Error performing SPI transfer");
		fclose(fp);
		return ERROR_DAQ_CAPTURE;
	}


	//Write to the file
	int bytes_wriiten = fwrite(rx_data, sizeof(rx_data), 1, fp);


    fclose(fp);
    isScanReady = FALSE;

    //Handle error case
    if(retValue == ERROR_DAQ_CAPTURE)
    {
    	//Remove the dat file
    	if(remove(result_raw_filename) == 0 )
    	{
    		//successfully removed the file
    		if(debugMode) printf("Output file deleted successfully\n");
    	}
    	else
    	{
    	  //couldn't remove the file ..maybe file doesn't exist (in case the error happens in the first read)
    	  if(debugMode) printf("File error while trying to delete \n");
    	}

    }

    return retValue;
}


//Start Scanning
int DAQ_StartCapture()
{
    int retValue = DAQ_SUCCESS;
    isScanReady = FALSE;
    if(result_raw_filename[0] == '\0')
    {

        return ERROR_NO_FILE_NAME;
    } //resultfilename

    retValue = StartDAQCapture();
    if(!retValue)
    {
        if(debugMode) printf("Results Captured successfully\n");
    }


    result_raw_filename[0] = '\0'; //resetting the file name

    return retValue;
}

void DAQ_Terminate()
{
    if(debugMode) printf(" IN DAQ_Terminate ()\n");
    if(isDeviceConnected)
    {
    	//close the SPI interface
    	   close(spi_fd);
    }
}

int DAQ_IsScanReady()
{
	return isScanReady;
}


bool DAQ_IsDeviceConnected()
{
	return isDeviceConnected;
}

int DAQ_ScanFailures()
{
	return nScanFailures;
}

int DAQ_IsDAQResponding()
{
  bool bResponding = TRUE;
  //TODO
  return bResponding;
}

int DAQ_EnableDebugMode(bool dmode)
{
    int retValue = DAQ_SUCCESS;
    debugMode = dmode;
    return retValue;
}


int DAQ_SetResultFileName(const char* fname)
{
    int retValue = DAQ_SUCCESS;
    if(debugMode) printf(" setting filename %s\n", fname);
    sprintf(result_raw_filename, "%s.dat", fname);
    if(debugMode) printf("Result Filename : %s\n", result_raw_filename);

    //create filename processed results
    sprintf(result_processed_filename, "%s_result_processed.txt", fname);
    if(debugMode) printf("Result Filename processed : %s\n", result_processed_filename);
    return retValue;
}

int DAQ_ConfigureSpiFrequency(int freq)
{
    int retValue = DAQ_SUCCESS;

    if(freq > 0 && freq < 101 )
    {
    	printf("Configuring SPI Frequency %d\n", freq);
    	spiFrequency = (__u8)freq;
    }
    else
    {
    	printf("Invalid SPI Frequency %d\n", freq);
    }
    //Need to configure again in case gain changed
    return retValue;
}


//Main function for standalone implementation (only for testing)
int main() {
    int spi_fd, i;
    uint8_t tx_data[512] = {0x01};  // Example data to be sent
    uint8_t rx_data[sizeof(tx_data)];  // Buffer for received data

    // Open the SPI device
    spi_fd = open("/dev/spidev1.1", O_RDWR);
    if (spi_fd < 0) {
        perror("Error opening SPI device");
        return 1;
    }

    // Set SPI parameters
    int mode = SPI_MODE_0;  // Set SPI mode 0
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    int bits = 8;  // Set data word size to 8 bits
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);

    int speed_hz = 1000000;  // Set SPI clock speed to 1 MHz
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);

    // Perform SPI transfer
    struct spi_ioc_transfer spi_transfer = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = sizeof(tx_data),
    };

    int ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &spi_transfer);
    if (ret < 0) {
        perror("Error performing SPI transfer");
        close(spi_fd);
        return 1;
    }

    // Print received data
    printf("Received data: ");
    for (i = 0; i < sizeof(rx_data); i++) {
        printf("0x%02X ", rx_data[i]);
    }
    printf("\n");

    // Close the SPI device
    close(spi_fd);

    return 0;
}
