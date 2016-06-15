/**
 * @file   FWReflash.c (APP)
 * @author UICO (Qiuliang Fu)
 * @date   7 April 2016
 * @version 0.1
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
#define MINI_SECOND   1000

#define CMD_PROGRAM_START 0xFF5A0023
#define CMD_PROGRAM_STOP  0xFF5A0034
#define CMD_PROGRAM_DATA  0xFF5A0045
#define CMD_PROGRAM_CFG   0xFF5A0056

#define COMMAND_GET_DATA         0x85
#define SYSTEM_INFO              0x01
#define SINGNATURE_RESPONSE_LENGTH 18
#define PAD_LENGTH               0
#define UPGRADE_SIGNATURE_LENGTH 8
#define UPGRADE_FIRMWARE_START   16
#define FLASH_ROW_SIZE           64

#define UPGRADE_PREAMBLE_LENGTH  10
#define UPGRADE_BLOCK_LENGTH     (12+FLASH_ROW_SIZE+2)
#define UPGRADE_SEGMENT_LENGTH   16
#define UPGRADE_EXIT_LENGTH      10

#define UICO_BIN_SIGNATURE      0x49 //'U'
#define UICO_CMD_SYSTEMINFO     0x85
#define UICO_LEN_SYSTEMINFO     0x11


#define COMMAND_ENTER_BOOTLOAD  0x38
#define COMMAND_FLASH_WRITE     0x39
#define COMMAND_FLASH_VERIFY    0x3A
#define COMMAND_BOOTLOAD_DONE   0x3B
#define COMMAND_BOOTLOAD_SELECT 0x3c
#define COMMAND_FIRST_BYTE      0xFF

#define COMM_CECKSUM_ERROR      0x10
#define BOOTLOAD_MODE           0x20
#define INVALID_KEY             0x40
#define INVALID_COMMAND_ERROR   0x80

static void printCharArray(unsigned char *str, int len)
{
    int i;
    if(len > 0) {
		for(i=0; i<len; i++)
			printf("%2x ", str[i]);
		printf("\n");
	}
}

static int programBytesArrayOnly(int devhandle, unsigned char *str, int len)
{
    //unsigned char receive[BUFFER_LENGTH];     // The receive buffer from the LKM
    if(write(devhandle, str, len) < 0)    
    {
        perror("Failed to write the message to the device.");
        return errno;
    }
    // printCharArray(str, len);
	return 0;
}

#if 0
static int programCharArray(int devhandle, unsigned char *str, int len)
{
   unsigned char receive[BUFFER_LENGTH];     // The receive buffer from the LKM
   
   programBytesArrayOnly(devhandle, str, len);
   
   usleep(10*MINI_SECOND);  // sleep for X - ms (X * 1000us)

   if(read(devhandle, receive, len) < 0) 
   {
      perror("Failed to read the message from the device.");
      return errno;
   }
 
   printCharArray(receive, len);
}
#endif

static int checkSystemInfo(unsigned char *headInfo, unsigned char *icInfo)
{
    uint8_t i, oldVer[2], newVer[2];
    uint16_t oldV, newV;
	int32_t payload_checksum = 0;
	int32_t computed_checksum = 0;

	if((UICO_CMD_SYSTEMINFO == icInfo[0]) && (UICO_LEN_SYSTEMINFO == icInfo[1]))
	{
		printf("Head info may correct -1-\n");
		if(0x11 == icInfo[1])
		{
			oldVer[0] = icInfo[10];
			oldVer[1] = icInfo[11];
		}
		else
		{
			oldVer[0] = 0;
			oldVer[1] = 0;
		}
		printf("Old Firmware Version: %x %x\n", oldVer[0], oldVer[1]);
	}

    newVer[0] = headInfo[4];
    //newVer[1] = headInfo[5];
    newVer[1] = headInfo[5] + 1; // QFU, Test only

    printf("New Friwmare Version: %x %x \n", newVer[0], newVer[1]);

    if(UICO_BIN_SIGNATURE != headInfo[0])
    {   
        printf("Firmware signature is wrong\n");
        //printCharArray(headInfo, 16);
	    return -1; // Fail
    }   

    payload_checksum = (headInfo[UPGRADE_SIGNATURE_LENGTH-2] << 8) + headInfo[UPGRADE_SIGNATURE_LENGTH-1];

	//**********************************************
	// ------ Checksum verification -------
	//**********************************************
#if 0    
	for (i = UPGRADE_SIGNATURE_LENGTH+8; i < FW_DATA_LENGTH; i++)
    {   
        computed_checksum += (FW_dat[i] & 0xFF);
    }   
    computed_checksum &= 0xffff;
    payload_checksum  &= 0xffff;
    if(payload_checksum != computed_checksum) 
	{   
	    printf("Firmware Checksum is wrong somehow\n");
        return -1; // Fail
    }   
#endif

    oldV = (((uint16_t)oldVer[0])<<8) + ((uint16_t)oldVer[1]);
    newV = (((uint16_t)newVer[0])<<8) + ((uint16_t)newVer[1]);

    if(oldV > newV)      printf("The existing firmware in IC is even newer!\n");
	else if(oldV < newV) printf("The existing firmware in IC is older!\n");
    else {
        printf("The existing firmware in IC is the same as yours!\n");
        printf("The Firmware reflash is stop\n");
		return -1;
	}
	return 0;
}

static int stopFirmwareReflash(int devHandle)
{
	unsigned char dByte[BUFFER_LENGTH];
    unsigned char recv[BUFFER_LENGTH];     // The receive buffer from the LKM
			
	//*********************************************
	// Send command to driver to stop the reflash
	//*********************************************
    strcpy(dByte+16, "STOP duraTOUCH Reflash");
    programBytesArrayOnly(devHandle, dByte+16, 22);
    usleep(10*MINI_SECOND);  // sleep for X - ms (X * 1000us)
    if(read(devHandle, recv, 4) < 0) 
    {
	    perror("Failed to read the message from the device.");
	    return -1;;
    } 
	printf("We want: FEED AA55, We got: %x %x %x %x\n", recv[0], recv[1], recv[2], recv[3]);
	return 0;
}	

static void stopThenQuit(int devfd, FILE *fwfd)
{
	close(devfd);
	fclose(fwfd);
}

int main() 
{
   int           devfd;
   int           ret, pki = 0, DPLen, retryCount;
   FILE          *fwfd;
   unsigned char len, Ongoing=0;
   unsigned char dByte[BUFFER_LENGTH];
   unsigned char receive[BUFFER_LENGTH];     // The receive buffer from the LKM

   printf("Reflash the Firmware of duraTOUCH ic...\n");
   //*************************************************************
   // Open duraTOUCH as a Char device to write and read stream
   //*************************************************************
   devfd = open("/dev/duraTOUCH", O_RDWR);  // Open the device with read/write access
   if (devfd < 0) {
      perror("Failed to open the device...\n");
      return errno;
   }

   //***********************************************
   // Open the firmware data file (binary file)
   //***********************************************
   fwfd = fopen("fw2364.bin", "rb");
   if(fwfd == NULL) {
       close(devfd);
       printf("Cannot open the firmware file\n");
       exit(EXIT_FAILURE);
   }   

    //***********************************************
    // We read the 16 bytes header first
    //***********************************************
    if(0x10 != fread(dByte, sizeof(unsigned char), 16, fwfd))
    {
	   // Something is wrong when we read the header info
       printf("Cannot read header data from the firmware file\n");
	   stopThenQuit(devfd, fwfd);
       exit(EXIT_FAILURE);
    } 
    else 
    {
	    //**********************************************************************************
        // Handle the header info, to make sure if we need to update the firmware in the IC
	    //**********************************************************************************
        strcpy(dByte+16, "duraTOUCH Reflash Start");
		programBytesArrayOnly(devfd, dByte+16, 23);   // Send "Start" messager to driver
		usleep(10*MINI_SECOND);  // sleep for X - ms (X * 1000us)
		if(read(devfd, receive, UICO_LEN_SYSTEMINFO) < 0) // Read System Info (17Bytes) from Driver
		{
			perror("Failed to read the message from the device.");
			stopThenQuit(devfd, fwfd);
			exit(EXIT_FAILURE);
		}
        
		// Check the System Info and Cur Firmware, Judge whether we should reflash FW 
		if(checkSystemInfo(dByte, receive)<0)
		{
			//*********************************************
			// We think the reflash is not necessary.
			// Send command to driver to stop the reflash
			//*********************************************
			stopFirmwareReflash(devfd);
			stopThenQuit(devfd, fwfd);
			exit(EXIT_FAILURE);
		}	
		else
		{
	    	//*********************************************
			// The reflash will start now by the judgement
			//*********************************************
			Ongoing = 1;
            printf("Firmware will be updated now!\n");
		}

    	//printCharArray(receive, UICO_LEN_SYSTEMINFO); // Print the System info
    }
 
    //***********************************************
    // Read the others after the header
    //***********************************************
    len = 0;
    while(Ongoing) {
        printf("%d\n", pki++);
	    //*********************************************
	    // Read one byte from the file stream
	    // If we cann't read it, it is the end of file
	    //*********************************************
        if(0 == fread(dByte, sizeof(unsigned char), 2, fwfd)) break;
		if(COMMAND_FIRST_BYTE != dByte[0])
	    {
		   printf("Firmware Data error! %x, %d, \n", dByte[0], pki);
		   break;
	    } 
        fseek(fwfd, -2, SEEK_CUR);

		switch(dByte[1])
        {
	    case COMMAND_ENTER_BOOTLOAD:  // 0x38
	        DPLen = UPGRADE_PREAMBLE_LENGTH;
			//printf("DPLen = %2x; \n", DPLen);
	        break;

	    case COMMAND_FLASH_WRITE:     // 0x39
	        DPLen = UPGRADE_BLOCK_LENGTH;
	        break;

	    case COMMAND_FLASH_VERIFY:    // 0x3A, we did not find it in the fw file
	        DPLen = 2 + 8;
	        break;

	    case COMMAND_BOOTLOAD_DONE:   // 0x3B
	        DPLen      = UPGRADE_EXIT_LENGTH;
	        Ongoing = 0;   // Last Writing and program
	        break;

	    case COMMAND_BOOTLOAD_SELECT: //0x3C
	        DPLen = 0;
	        break;

	    default:
	        printf("Firmware Data Error\n");
			stopFirmwareReflash(devfd); // stop firmware upgradin
			stopThenQuit(devfd, fwfd);
			exit(EXIT_FAILURE);
		    break;
        }

		if(DPLen>0)
		{
            if(DPLen != fread(dByte, sizeof(unsigned char), DPLen, fwfd)) break;

			retryCount = 3;
			while(retryCount>0)
			{
    		    programBytesArrayOnly(devfd, dByte, DPLen);
				usleep(100*MINI_SECOND);  // sleep for X - ms (X * 1000us)
                if(dByte[1] == COMMAND_BOOTLOAD_DONE) break;
				if(read(devfd, receive, 2) < 0) 
				{
					perror("Failed to read the message from the device.");
					stopThenQuit(devfd, fwfd);
					exit(EXIT_FAILURE);
				} 
                if(receive[0] == BOOTLOAD_MODE) break;
                else retryCount--;
			}

            if(retryCount < 3) {
	            printf("Write [ %d ] times over the PACKAGE No.%d\n", 4-retryCount, pki);
	            printf("The data read back from is [ %x ] \n", receive[0]);
			}

            if((receive[0] != BOOTLOAD_MODE) && (dByte[1] != COMMAND_BOOTLOAD_DONE)) {
				// Stop program
				stopFirmwareReflash(devfd); // stop firmware upgrading
	            printf("There is a problem when the program is ongoing @ %d\n", pki);
				stopThenQuit(devfd, fwfd);
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			DPLen = 14;
            if(DPLen != fread(dByte, sizeof(unsigned char), DPLen, fwfd)) break;
		}

	    //usleep(2*MINI_SECOND);  // sleep for X - ms (X * 1000us)
	};

    if(Ongoing) 
	{
		Ongoing = 0;
        printf("\nFirmware Reflash : FAIL\n");
	    stopFirmwareReflash(devfd); // stop firmware upgrading
	} else {
		printf("\nFirmware Reflash : DONE\n");
	}

    fclose(fwfd);
    close(devfd);
    return 0;
}
