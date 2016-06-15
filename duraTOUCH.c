/************************************************************
 * @file   duraTOUCH.c
 * @author Qiuliang Fu
 * @date   19 April 2016
 * @brief  A kernel module for controlling a duraTOUCH IC.
 * The driver is wokring in Beaglebone Black (Revision C)
 *
 * I2C_SDA: P9-20; I2C_SCL: P9-19; PIN_Interrupt: P9-12.
 *
 * @see http://www.uico.com/
************************************************************/
#ifndef _DURATOUCH_C_
#define _DURATOUCH_C_

#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#ifdef ANDROID_FUNC
#ifdef BBB
#else
#include <linux/earlysuspend.h>
#endif
#else
#include <linux/slab.h>
#endif

#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/interrupt.h>            // Required for the IRQ code
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/gpio.h>                 // Required for the GPIO functions
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>                 // Required for the GPIO functions

#define DEVICE_NAME "duraTOUCH"
#define CLASS_NAME  "duraTOUCH_dTx01"

#define duraTOUCH_I2C_ADDR 0x48
#define duraTOUCH_BOOTLOAD 0x58
#define COMMAND_BOOTLOAD_DONE 0x3B

#define SCREEN_MAX_X    800
#define SCREEN_MAX_Y    480
#define PRESS_MAX       200

#define MAX_POINT       1
#define AREA_SETTING    20

#define DRIVER_STATUS_NORMAL     0
#define DRIVER_STATUS_REFRESH    1
#define DRIVER_STATUS_BLE_COMM   2
#define BOOTLOAD_MODE            0x20
#define COMMAND_BOOTLOAD_DONE    0x3B

struct duraTOUCH_event {
	uint16_t   flag;
	uint16_t   x;
	uint16_t   y;
	uint16_t   pressure;
	uint16_t   w;
};

//static struct duraTOUCH_event dT_point[MAX_POINT];

struct duraTOUCH_data {
	struct i2c_client *client;
	struct input_dev  *dev;
	int               reset_gpio;
	int               touch_en_gpio;
	int               last_point_num;

	struct workqueue_struct *workqueue;
	struct work_struct duraTOUCH_event_work;
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qiuliang Fu");
MODULE_DESCRIPTION("UICO duraTOUCH driver for the BBB");
MODULE_VERSION("0.5");

//************************************************
// Global VAR prototype for the driver 
//************************************************
static struct i2c_client *g_i2c_client;
static struct duraTOUCH_event dTevent[MAX_POINT];
static struct duraTOUCH_data *duraTouchDev = NULL;     // GPIO of IRQ Pin in the board system

#ifdef TQ210
static unsigned int duraTOUCH_int = S5PV210_GPH1(6); // hard coding the button gpio to P9_12 (GPIO60)
#endif

#ifdef RASPI
static unsigned int duraTOUCH_int = 17;              // hard coding the button gpio to J8(PIN11, GIO17)
#endif

#ifdef BBB
static unsigned int duraTOUCH_int = 60;              // hard coding the button gpio to P9_12 (GPIO60)
#endif

static unsigned int irqNumber;                       // Used to share the IRQ number within this file
static uint16_t CurFingerNum;
static int      DriverStatus;    // NORMAL, PROGRAM

static int            dTmajorNumber;
static char           dTdata[256] = {0};
static short          sizeof_dTdata;
static int            dTnumberOpens;
static struct class*  duraTouchClass = NULL;
static struct device* duraTouchDevice = NULL;
static DEFINE_MUTEX(duraTOUCH_mutex);

static int      duraTOUCH_open(struct inode *, struct file *);
static int      duraTOUCH_release(struct inode *, struct file *);
static ssize_t  duraTOUCH_read(struct file *, char *, size_t, loff_t *);
static ssize_t  duraTOUCH_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.open = duraTOUCH_open,
	.read = duraTOUCH_read,
	.write = duraTOUCH_write,
	.release = duraTOUCH_release,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend duraTOUCH_early_suspend;
static void duraTOUCH_i2c_early_suspend(struct early_suspend *h); 
static void duraTOUCH_i2c_late_resume(struct early_suspend *h);
#endif

//*********************************************************************************************
// Function prototype for the custom IRQ handler function -- see below for the implementation
//*********************************************************************************************
static irq_handler_t  duraTOUCH_irq_handler(unsigned int irq, void *handle);

static int duraTOUCH_i2cWrite(uint8_t num, uint8_t *bytes)
{
	int status = 0, mytry = 0, retryLimit = 2;
        struct i2c_msg mymsg;


	if(!duraTouchDev->client->adapter) {
		printk(KERN_ALERT "Cannot get I2C Adapter\n");
		status = -1;
		return status;
	}

    if(DriverStatus == DRIVER_STATUS_REFRESH)
	{
		mymsg.addr = duraTOUCH_BOOTLOAD;
	} 
	else if(DriverStatus == DRIVER_STATUS_REFRESH)
	{
		mymsg.addr = duraTOUCH_I2C_ADDR;
	}
	else
	{
		mymsg.addr = duraTOUCH_I2C_ADDR;
	}

	mymsg.flags = 0;
	mymsg.buf = bytes;
	mymsg.len = num;

	mytry = 0;
#if 1
	while(mytry < retryLimit) {
                //status = i2c_master_send(duraTouchDev->client, bytes, num);
		//if(status < 0)
		status = i2c_transfer(duraTouchDev->client->adapter, &mymsg, 1);
		if(status < 0)
		{
			mytry++;
			printk(KERN_ALERT "I2C Transfer Retry %d, return val: %d\n", mytry, status);
		}
		//else if(status != num) {
		else if(status != 1) {
			mytry++;
			printk(KERN_ALERT "I2C Write Transfer %d bytes\n", status);
		} 
		else break;
	}

	if(mytry >= retryLimit) {
		printk(KERN_ALERT "Cannot write data into i2C Adapter\n");
	}
#else
	status = i2c_transfer(duraTouchDev->client->adapter, &mymsg, 1);
#endif
	return status;
}


static int duraTOUCH_i2cRead(uint8_t num, uint8_t *bytes)
{
	int status = 0, mytry = 0, retryLimit = 2;

	if(!duraTouchDev->client->adapter) {
		printk(KERN_ALERT "Cannot get I2C Adapter\n");
		status = -1;
		return status;
	}

	mytry = 0;
#if 1
	while(mytry < retryLimit) {
		//if(num != i2c_master_recv(duraTouchDev->client, bytes, num))
		status = i2c_master_recv(duraTouchDev->client, bytes, num);
		if(status < 0)
		{
			mytry++;
			printk(KERN_ALERT "I2C Transfer Retry %d, return val: %d\n", mytry, status);
		} 
		else if(status != num) {
			mytry++;
			printk(KERN_ALERT "I2C Transfer %d bytes\n", status);
		} 
		else break;
	}

	if(mytry >= retryLimit) {
		printk(KERN_ALERT "Cannot read data from I2C Adapter\n");
	}
#else
	i2c_master_recv(duraTouchDev->client, bytes, num);
#endif
	return status;
}

static int duraTOUCH_ChipInit(struct i2c_client *client)
{
        return 0;
}


static void duraTOUCH_getSystemInfo(struct i2c_client *client)
{
    uint8_t /*rdata[32],*/ wdata[32], num;

	// Write System Info Command into Touch IC
 	wdata[0] = 0x00; // Write Register Address
 	wdata[1] = 0x85; // System Info command
 	wdata[2] = 0x01; // Data0
    num = 3;
    duraTOUCH_i2cWrite(num, wdata);
}


static int duraTOUCH_ReadDataFromIC(struct work_struct *work)
{
    uint8_t rdata[32], wdata[32], num;
      
	if(DriverStatus == DRIVER_STATUS_REFRESH)
		return -1;

	//*********************************************
	// Write 0x20 into Touch IC
	//*********************************************
 	wdata[0] = 0x20;
    num = 1;
    duraTOUCH_i2cWrite(num, wdata);
        
	//*********************************************
	// Read 2 bytes from Touch IC
	//*********************************************
	num = 2;
    duraTOUCH_i2cRead(num, rdata);
	
	//*********************************************
	// Check the length of rdata[1]
	//*********************************************
	if(rdata[1] >0)
	{
	    //*********************************************
		// Read the whole data
	    //*********************************************
        duraTOUCH_i2cRead(rdata[1]+2, rdata);
	}

    //*********************************************
    // Clear the INT to the touch IC
    //*********************************************
    wdata[0] = 0x20;
    wdata[1] = 1;
    num = 2;
    duraTOUCH_i2cWrite(num, wdata);

	switch(DriverStatus)
	{
		case DRIVER_STATUS_NORMAL:
            //****************************************
			// [0] Command Index
			// [1] Length - 2
			// [2] Serial Index
			// [3] Finger Number
			// [4] X - High Byte
			// [5] X - Low Byte
			// [6] Y - High Byte
			// [7] Y - Low Byte
			//****************************************
			CurFingerNum = rdata[3]; 
			dTevent[0].x = (uint16_t)rdata[4];
			dTevent[0].x = (dTevent[0].x<<8) + (uint16_t)rdata[5];
			dTevent[0].y = (uint16_t)rdata[6];
			dTevent[0].y = (dTevent[0].y<<8) + (uint16_t)rdata[7];
			break;

		case DRIVER_STATUS_REFRESH:
			//for(num = 0; num<(rdata[1]+2); num++) dTdata[num] = rdata[num];
			//sizeof_dTdata = rdata[1]+2;
			break;

		case DRIVER_STATUS_BLE_COMM:
			break;
	}
	return 0;
}

static void duraTOUCH_ISR(struct work_struct *work)
{
    int i;

    //printk(KERN_INFO "ISR-1\n");
	if(duraTOUCH_ReadDataFromIC(work) < 0)
	{
		//*****************************************************
		// Nothing should be report, Enable the IRQ
		// The case is working in, like DRIVER_STATUS_REFRESH
		//*****************************************************
		enable_irq(duraTouchDev->client->irq);
		return;
	}
    //printk(KERN_INFO "ISR-2\n");

	//****************************************************************
	// Report the finger information or other acts based on status
	//****************************************************************
	switch(DriverStatus)
	{
		case DRIVER_STATUS_NORMAL:
	        for(i=0; i<MAX_POINT; i++)
			{
				if(CurFingerNum>0) {
				    input_mt_slot(duraTouchDev->dev, i);
					input_mt_report_slot_state(duraTouchDev->dev, MT_TOOL_FINGER, true);
					input_report_abs(duraTouchDev->dev, ABS_MT_TRACKING_ID, i);
					input_report_abs(duraTouchDev->dev, ABS_MT_TOUCH_MAJOR, AREA_SETTING);
					input_report_abs(duraTouchDev->dev, ABS_MT_POSITION_X, dTevent[0].x+100);
					input_report_abs(duraTouchDev->dev, ABS_MT_POSITION_Y, dTevent[0].y+100);
					input_report_abs(duraTouchDev->dev, ABS_MT_WIDTH_MAJOR, AREA_SETTING);
				} else {
					input_mt_slot(duraTouchDev->dev, i);
					input_report_abs(duraTouchDev->dev, ABS_MT_TOUCH_MAJOR, -1);
					input_mt_report_slot_state(duraTouchDev->dev, MT_TOOL_FINGER, false);
				}
				input_mt_sync(duraTouchDev->dev);
				//printk(KERN_INFO "INPUT system reported @ %d", i);
			}
			input_sync(duraTouchDev->dev);
			break;

		case DRIVER_STATUS_REFRESH:
			break;

		case DRIVER_STATUS_BLE_COMM:
			break;
	}

	// Enable the IRQ
    enable_irq(duraTouchDev->client->irq);
}

static void duraTOUCH_echoTest(struct i2c_client *client)
{
         uint8_t rdata[32], wdata[32], num;

		// Write ECHO command into Touch IC
	 	wdata[0] = 0x00; // Write Register Address
		wdata[1] = 0x8A; // ECHO command
		wdata[2] = 0x55; // Data0
		wdata[3] = 0xAA; // Data1
		wdata[4] = 0xFF; // Data2
		wdata[5] = 0xE7; // Data3
        num = 6;
        duraTOUCH_i2cWrite(num, wdata);
     
		msleep(6);
		wdata[0] = 0x20; // Read Register Address
        num = 1;
        duraTOUCH_i2cWrite(num, wdata);
	
		// Read 6 bytes from Touch IC
		num = 6;
        duraTOUCH_i2cRead(num, rdata);
		printk("Read Data back from dT101: ");
		for(num=0; num<6; num++) printk("%x ", rdata[num]);
		printk("\n");
}

static int duraTOUCH_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int status = 0;

    //******************************************************
    // Call i2c_set_clientdata();
    //******************************************************
    g_i2c_client = client;
    duraTouchDev->client = g_i2c_client;
    duraTouchDev->last_point_num = 0;
    i2c_set_clientdata(client, duraTouchDev);

    //******************************************************
    // Do what a Chip should do @ its Reset/Init procedure 
    //******************************************************
    duraTOUCH_ChipInit(client);

    //******************************************************
    // Check I2C channel works or NOT ?
    //******************************************************
    if(!i2c_check_functionality(duraTouchDev->client->adapter, I2C_FUNC_I2C)) 
    {
	    dev_err(&duraTouchDev->client->dev, "I2C functionality not supported\n");
	    printk(KERN_ALERT "CAN NOT check i2c function properly\n");
	    return -ENODEV;
    }
    printk(KERN_INFO "i2c_probe is done correctly\n");


    //if( (i2c_detect(duraTouchDev->client->adapter, duraTOUCH_i2c_driver) < 0) {
    //	    printk(KERN_ALERT "I2C Detection is fail\n");
    //}

    duraTOUCH_echoTest(g_i2c_client);

    //******************************************************
    // 1. ISR is mapped by calling INIT_WORK;
    // 2. creat a working queue for the Interrupt;
    //******************************************************
    INIT_WORK(&duraTouchDev->duraTOUCH_event_work, duraTOUCH_ISR);
    duraTouchDev->workqueue = create_singlethread_workqueue("duraTOUCH_wq");
    if(duraTouchDev->workqueue == NULL) 
    {
	    printk(KERN_ALERT "CAN NOT create work queue properly\n");
            return -ESRCH;
    }
    printk(KERN_INFO "work queue is created correctly\n");

#if 1
    //******************************************************
    // 1. Setup a INPUT system
    // 2. Setup parameter for the input system
    //******************************************************
    duraTouchDev->dev = input_allocate_device();
    if(duraTouchDev->dev == NULL) 
    {
	    destroy_workqueue(duraTouchDev->workqueue);
	    printk(KERN_ALERT "CAN NOT get the input device\n");
	    return -ENOMEM;
    }
    printk(KERN_INFO "Input device is allocated!\n");

    __set_bit(EV_ABS, duraTouchDev->dev->evbit); // ABS Axial Location
    __set_bit(EV_KEY, duraTouchDev->dev->evbit); // Event KEY
    __set_bit(EV_REP, duraTouchDev->dev->evbit); // Repeat
    __set_bit(INPUT_PROP_DIRECT, duraTouchDev->dev->propbit); // TouchScreen Device

#ifdef ANDROID_FUNC
#ifdef BBB
    input_mt_init_slots(duraTouchDev->dev, MAX_POINT, 0);
#else
    input_mt_init_slots(duraTouchDev->dev, MAX_POINT);
#endif
#else
    input_mt_init_slots(duraTouchDev->dev, MAX_POINT, 0);
#endif

    set_bit(ABS_MT_TOUCH_MAJOR, duraTouchDev->dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, duraTouchDev->dev->absbit);
    set_bit(ABS_MT_POSITION_X, duraTouchDev->dev->absbit);
    set_bit(ABS_MT_POSITION_Y, duraTouchDev->dev->absbit);
    set_bit(ABS_MT_TRACKING_ID, duraTouchDev->dev->absbit);

    input_set_abs_params(duraTouchDev->dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(duraTouchDev->dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(duraTouchDev->dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);

    duraTouchDev->dev->name = "duraTOUCHic";
    duraTouchDev->dev->phys = "input/duraTOUCH";
    duraTouchDev->dev->id.bustype = BUS_I2C;
    duraTouchDev->dev->id.vendor = 0xDEAD;
    duraTouchDev->dev->id.product = 0xBEEF;
    duraTouchDev->dev->id.version = 0x0101;
    duraTouchDev->dev->dev.parent = &client->dev;

    status = input_register_device(duraTouchDev->dev);
    if(status < 0) {
	    printk(KERN_ALERT "CANNOT regsiter input device");
	    input_free_device(duraTouchDev->dev);
	    destroy_workqueue(duraTouchDev->workqueue);
	    return -ENOMEM;
    }
    printk(KERN_INFO "Input device is registered !\n");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
    duraTOUCH_early_suspend.suspend = duraTOUCH_i2c_early_suspend;
    duraTOUCH_early_suspend.resume = duraTOUCH_i2c_late_resume;
    duraTOUCH_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
    register_early_suspend(&duraTOUCH_early_suspend);
    printk(KERN_INFO "early suspend is registered!\n");
#endif
    return status;
}


static int duraTOUCH_i2c_remove(struct i2c_client *client)
{

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&duraTOUCH_early_suspend);
#endif
	
	input_unregister_device(duraTouchDev->dev);
	input_free_device(duraTouchDev->dev);
        destroy_workqueue(duraTouchDev->workqueue);
	duraTouchDev->client = NULL;
	//if(duraTouchDev) kfree(duraTouchDev);
	return 0;
}

//*****************************************************
// 1. early_suspend is pointed to suspend finally
// 2. late_resume is pointed to resume finally
//
//     Whatever we define CONFIG_HAS_EARLYSUSPEND,
// we need the following function for sure.
//*****************************************************
#ifdef ANDROID_FUNC 
static int duraTOUCH_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
    int status = 0;

	return status;
}

static int duraTOUCH_i2c_resume(struct i2c_client *client)
{
    int status = 0;

	return status;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void duraTOUCH_i2c_early_suspend(struct early_suspend *h) 
{
	duraTOUCH_i2c_suspend(duraTouchDev->client, PMSG_SUSPEND);
}

static void duraTOUCH_i2c_late_resume(struct early_suspend *h)
{
	duraTOUCH_i2c_resume(duraTouchDev->client);
}
#endif

static int duraTOUCH_open(struct inode *inodep, struct file *filep) {
    if(!mutex_trylock(&duraTOUCH_mutex)) {
		printk(KERN_ALERT "duraTOUCH: Device in use by another process");
		return -EBUSY;
	}
	dTnumberOpens++;
	DriverStatus = DRIVER_STATUS_NORMAL;
	printk(KERN_INFO "duraTOUCH Open, open the device %d time(s)\n", dTnumberOpens);
	return 0;
}

static int duraTOUCH_release(struct inode *inodep, struct file *filep) {
	dTnumberOpens--;
	printk(KERN_INFO "duraTOUCH release, Close the device now\n");
	mutex_unlock(&duraTOUCH_mutex);
	return 0;
}

//***************************************************************
// Read bytes from dTdata(kernel)) to Buffer (User Space)
//***************************************************************
static ssize_t duraTOUCH_read(struct file *filep, char *buffer, size_t len, loff_t *offset) 
{
	int error_count = 0;

    if(DriverStatus == DRIVER_STATUS_REFRESH)
	{
		if(sizeof_dTdata == 1)
		{

            if(dTdata[1] == COMMAND_BOOTLOAD_DONE) {
				DriverStatus = DRIVER_STATUS_NORMAL;
				printk(KERN_INFO "duraTOUCH: Reflash is finished!\n");
			}

			// Read the data from Device
			duraTOUCH_i2cRead(2, dTdata);
			sizeof_dTdata = 4;
		}
	}

    //******************************************************************
    // Since return of copy_to_user is the rest byte which is not copied
    // so, the return value should be 0 when it is successful.
    //******************************************************************
	error_count = copy_to_user(buffer, dTdata, sizeof_dTdata);
	
    if(error_count==0) {
		return(sizeof_dTdata=0);
	} else {
		printk(KERN_INFO "duraTOUCH: fail to send %d bytes to the user space", error_count);
		return -EFAULT;
	}
}

//***************************************************************
// Received data from APP
//***************************************************************
static ssize_t duraTOUCH_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) 
{

	int i;
    for(i=0;i<len; i++) dTdata[i] = buffer[i];
    sizeof_dTdata = len;
	//printk(KERN_INFO "duarTOUCH: Received %d bytes from the user space\n", len);

	switch(DriverStatus)
	{
		case DRIVER_STATUS_NORMAL:
			if(0 == strncmp(dTdata, "duraTOUCH Reflash Start", len))
			{
				//***********************************************
				// We get info of reflash, then we will do it
				//***********************************************
                printk(KERN_INFO "duraTOUCH: Reflash now \n");
                duraTOUCH_getSystemInfo(g_i2c_client); // Prepare to read system info
			    DriverStatus = DRIVER_STATUS_REFRESH;  // Set to status 
			} 
			break;

		case DRIVER_STATUS_REFRESH:
			//******************************************
			// Write Reflash Data into Touch IC
			//******************************************
			if(0 == strncmp(dTdata, "STOP duraTOUCH Reflash", len))
			{
				printk(KERN_INFO "STOP duraTOUCH: Reflash\n");
				DriverStatus = DRIVER_STATUS_NORMAL;
				sizeof_dTdata = 4; // 4bytes;
				dTdata[0] = 0xFE;  // Tell Host reflash is stop
				dTdata[1] = 0xED;
				dTdata[2] = 0xAA;
				dTdata[3] = 0x55;
			}

			// Write the data into The Chip
            if((0xFF == dTdata[0]) && ((0x38 == dTdata[1]) || (0x39 == dTdata[1]) || (0x3A == dTdata[1]) || (0x3B == dTdata[1]))) {
				    //********************************************************
				    // This action should be here before write to Bootloader
				    //********************************************************
				    if(dTdata[1] == COMMAND_BOOTLOAD_DONE) {
						DriverStatus = DRIVER_STATUS_NORMAL;
					}
				    
					//********************************************************
				    // Write the data to bootloader
					//********************************************************
					duraTOUCH_i2cWrite(len, dTdata);
					dTdata[0] = BOOTLOAD_MODE + 1;     // Init to failure 
			        sizeof_dTdata = 1; // 4bytes;
			} else {
				// Since data is wrong, send failure info to APP
			    sizeof_dTdata = 4; // 4bytes;
			    dTdata[0] = BOOTLOAD_MODE<<1;
				dTdata[1] = BOOTLOAD_MODE<<2;
				dTdata[2] = BOOTLOAD_MODE<<2;
				dTdata[3] = BOOTLOAD_MODE<<2;
			}
			break;

		case DRIVER_STATUS_BLE_COMM:
			break;
	}
	return len;
}

static const struct i2c_device_id duraTOUCH_i2c_id[] = {
    {"duraTOUCHic", 0}, 
	{},
};

static struct i2c_driver duraTOUCH_i2c_driver = {
	.probe = duraTOUCH_i2c_probe,
	.remove = duraTOUCH_i2c_remove,
	.id_table = duraTOUCH_i2c_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
#ifdef ANDROID_FUNC 
	.suspend = duraTOUCH_i2c_suspend,
	.resume = duraTOUCH_i2c_resume,
#endif
#endif
	.driver = {
		.name = "duraTOUCHic",
		.owner = THIS_MODULE,
	},
};

static void duraTOUCH_Vars_Init(void)
{
	DriverStatus = DRIVER_STATUS_NORMAL;
}

/** ibrief The Driver initialization function
 *  The static keyword restricts the visibility of the function to within this C file. 
 *  The __init macro means that for a built-in driver (not a LKM) 
 *  ihe function is only used at initialization time and that it can be discarded 
 *  and its memory freed up after that point. In this example: 
 *  this function sets up the GPIOs and the IRQ
 *  @return returns 0 if successful
 */
static int __init duraTOUCH_init(void){
   int result = 0;

   printk(KERN_INFO "UICO duraTOUCH: Init the UICO_duraTOUCH driver\n");

   duraTOUCH_Vars_Init(); // Init the var what driver used in the code;

   // Try to register to get device major number
   dTmajorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if(dTmajorNumber<0) {
	   printk(KERN_ALERT "duraTOUCH fail to register a major number\n");
	   return dTmajorNumber;
   }
   printk(KERN_INFO "duraTOUCH, registered with major number %d\n", dTmajorNumber);

   // Register the device class
   duraTouchClass = class_create(THIS_MODULE, CLASS_NAME);
   if(IS_ERR(duraTouchClass)) {
	   unregister_chrdev(dTmajorNumber, DEVICE_NAME);
	   printk(KERN_ALERT "fail to register device class\n");
	   return PTR_ERR(duraTouchClass);
   }
   printk(KERN_INFO "duraTOUCH: device class is registered\n");

   duraTouchDevice = device_create(duraTouchClass, NULL, MKDEV(dTmajorNumber, 0), NULL, DEVICE_NAME);
   if(IS_ERR(duraTouchDevice)) {
           class_destroy(duraTouchClass);
	   unregister_chrdev(dTmajorNumber, DEVICE_NAME);
	   printk(KERN_ALERT "fail to create the device (driver)\n");
	   return PTR_ERR(duraTouchDevice);
   }
   mutex_init(&duraTOUCH_mutex);
   printk(KERN_INFO "duraTOUCH: device class created correctly, mutex is inited\n");

   //************************************************************************
   // Extract kernel memory for duraTouchDev
   //************************************************************************
   duraTouchDev = kmalloc(sizeof(*duraTouchDev), GFP_KERNEL);
   if(!duraTouchDev) {
           printk(KERN_ALERT "duraTouchDev cannot get memory in kernel\n");
	   return PTR_ERR(duraTouchDev); 
   }
   printk(KERN_INFO "duraTouchDev got memory for duraTouchDev in kernel\n");

   //************************************************************************
   // Add I2C driver with probe function mainly
   //************************************************************************
   result = i2c_add_driver(&duraTOUCH_i2c_driver);
   if(result<0) {
	   printk(KERN_ALERT "Unable add I2C driver properly\n");
	   kfree(duraTouchDev);
	   return result;
   }
   printk(KERN_INFO "duraTouchDev add I2C driver correctly\n");

   //*********************************************************************************
   // Config the interrupt GPIO(Pin)
   // GPIO numbers and IRQ numbers are not the same! This function performs the mapping for us
   //*********************************************************************************
   gpio_request(duraTOUCH_int, "sysfs");    // Set up the duraTOUCH_int
   gpio_direction_input(duraTOUCH_int);     // Set the button GPIO to be an input
   gpio_export(duraTOUCH_int, false);       // Causes gpio60 to appear in /sys/class/gpio
			                    // the bool argument prevents the direction from being changed
   gpio_set_debounce(duraTOUCH_int, 5);     // Debounce the button with a delay of 200ms(5ms now)
   //printk(KERN_INFO "UICO duraTOUCH: The INT pin is currently: %d\n", gpio_get_value(duraTOUCH_int));
   irqNumber = gpio_to_irq(duraTOUCH_int);
   printk(KERN_INFO "UICO duraTOUCH: The INT-Pin is mapped to IRQ: %d\n", irqNumber);
   g_i2c_client->irq = irqNumber;
   
   //*********************************************************************************
   // This next call requests an interrupt line
   //*********************************************************************************
   result = request_irq(irqNumber,                             // The interrupt number requested
                        (irq_handler_t) duraTOUCH_irq_handler, // The pointer to the handler function below
                        IRQF_TRIGGER_FALLING,                  // Interrupt on rising edge (button press, not release)
                        "duraTOUCH_ISR_handler",               // Used in /proc/interrupts to identify the owner
                        NULL);                                 // The *dev_id for shared interrupt lines, NULL is okay
   printk(KERN_INFO "UICO duraTOUCH: The interrupt request result is: %d\n", result);
   return result;
}
/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required. Used to release the
 *  GPIOs and display cleanup messages.
 */
static void __exit duraTOUCH_exit(void) {
   
   free_irq(irqNumber, NULL);               // Free the IRQ number, no *dev_id required in this case
   gpio_unexport(duraTOUCH_int);            // Unexport the Button GPIO
   gpio_free(duraTOUCH_int);                // Free the Button GPIO
   
   i2c_del_driver(&duraTOUCH_i2c_driver);
   kfree(duraTouchDev);
 
   mutex_destroy(&duraTOUCH_mutex);

   device_destroy(duraTouchClass, MKDEV(dTmajorNumber,0));
   class_unregister(duraTouchClass);
   class_destroy(duraTouchClass);
   unregister_chrdev(dTmajorNumber, DEVICE_NAME);
   
   printk(KERN_INFO "UICO duraTOUCH: Goodbye from the duraTOUCH driver!\n\n");
}

/** @brief The GPIO IRQ Handler function
 *  This function is a custom interrupt handler that is attached to the GPIO above. The same interrupt
 *  handler cannot be invoked concurrently as the interrupt line is masked out until the function is complete.
 *  This function is static as it should not be invoked directly from outside of this file.
 *  @param irq       the IRQ number that is associated with the GPIO -- useful for logging.
 *  @param dev_id    the *dev_id that is provided -- can be used to identify which device caused the interrupt
 *                   Not used in this example as NULL is passed.
 *  @param regs      h/w specific register values -- only really ever used for debugging.
 *  return returns   IRQ_HANDLED if successful -- should return IRQ_NONE otherwise.
 */
static irq_handler_t duraTOUCH_irq_handler(unsigned int irq, void *handle)
{
   disable_irq_nosync(irq);
   queue_work(duraTouchDev->workqueue, &duraTouchDev->duraTOUCH_event_work);

   return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
}

///*****************************************************************************
/// This next calls are  mandatory -- they identify the initialization function
/// and the cleanup function (as above).
///*****************************************************************************
module_init(duraTOUCH_init);
module_exit(duraTOUCH_exit);

#endif

