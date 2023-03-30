
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/types.h>
#include <linux/kdev_t.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <asm/div64.h>


/* @TODO
 * Get I2C Adapter
 * Create i2c_board_info structure and create a device using the same 
 * create i2c_device_id for slave device and register with that
 * create i2c_driver and add it to i2c subsystem
 * Transfer data between devices
 * Finally remove the device
 */


/* ADDRESS MAP 
 * { TEMP_MSB,TEMP_LSB,{TEMP_XLSB[7:4],0000} } ===> {0xFA,0xFB,0xFC}
 * {PRESS_MSB,PRESS_LSB,{PRESS_XLSB[7:4],0000}} ===> {0xF7,0xF8,0xF9}
 * {CONFIG}===>{T_LSB[2:0],FILTER[2:0],[RESERVED],SPI3W_EN}===> {0xF5}
 * {CTRL_MEAS} ===> {OSRS_T[2:0],OSRS_P[2:0],MODE[1:0]} ===> {0xF4}
 * {STATUS} ===> {RESERVED[7:5],MEASURING[0],IM_UPDATE[0]} ===> {0xF3}
 * {RESET} ===> {RESET[7:0]} ===> {0xE0}
 * {ID} ===>{CHIP_ID[7:0]} ===> 0xD0
 */

#define I2C_BUS_NO (2) /*For BBB for raspberry pi4 0 is more likely value*/

#define BMP_ADDR (0x76)
/* #define BMP_ADDR (0x3C) */
#define BMP_TEMP_MSB (0xFA)
#define BMP_TEMP_LSB (0xFB)
#define BMP_TEMP_XLSB (0xFC)

#define BMP_PRESS_MSB (0xF7)
#define BMP_PRESS_LSB (0xF8)
#define BMP_PRESS_XLSB (0xF9)

#define BMP_CONFIG (0xF5)
#define BMP_CTRL_MEAS (0xF4)
#define BMP_STATUS (0xF3)

#define BMP_T1 (0x88)
#define BMP_T2 (0x8a)
#define BMP_T3 (0x8c)

#define BMP_P1 (0x8E)
#define BMP_P2 (0x90)
#define BMP_P3 (0x92)
#define BMP_P4 (0x94)
#define BMP_P5 (0x96)
#define BMP_P6 (0x98)
#define BMP_P7 (0x9A)
#define BMP_P8 (0x9C)
#define BMP_P9 (0x9E)

#define BMP_RESET (0xE0)
#define BMP_ID (0xD0)

/* typedef struct{ */
/* 	uint8_t osrs_t:3; */
/* 	uint8_t osrs_p:3; */
/* 	uint8_t mode:2; */


/* }CTRL_MEAS; */
/* typedef uint8_t reset; */
/* typedef uint8_t ID; */

/* typedef struct{ */	
/* 	uint8_t temp_xlsb:4; */
/* 	uint8_t reserved:4; */
/* }XLSB; */
/* typedef struct{ */
/* 	uint8_t temp_msb; */
/* 	uint8_t temp_lsb; */
/* 	unsigned xlsb; */
/* }Temp; */
/* typedef struct{ */
/* 	uint8_t press_msb; */
/* 	uint8_t press_lsb; */
/* 	XLSB xlsb; */
/* }Pressure; */

/* typedef struct{ */
/* 	uint8_t t_sb:3; */
/*     uint8_t filter:3; */
/* 	uint8_t reserved:1; */
/* 	uint8_t spi_en:1; */
/* }Config; */

static struct i2c_adapter * bmp_adapter = NULL;
static struct i2c_client * bmp_client = NULL;

static struct proc_dir_entry *temp_entry;
static struct proc_dir_entry *press_entry;

static int bmp_probe( struct i2c_client * bmp_client, const struct i2c_device_id * bmp_id );
static int bmp_remove( struct i2c_client * bmp_client );

static const struct i2c_device_id bmp_id [] = {
	{ "BMP280", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, bmp_id);
static struct i2c_driver bmp_driver = {
	
	.driver = {
		.name = "BMP280",
		.owner = THIS_MODULE
	},
	.probe = bmp_probe,
	.remove = bmp_remove,
	.id_table = bmp_id
};
static int temp_open( struct inode * ind, struct file * fp );
static int temp_release( struct inode * ind, struct file * fp );
static ssize_t temp_read( struct file * fp, char * buf, size_t len,loff_t * loff );
static ssize_t press_read( struct file * fp, char * buf, size_t len,loff_t * loff );
static struct proc_ops pr_ops = {
	.proc_open = temp_open,
	.proc_release = temp_release,
	.proc_read = temp_read
};
static struct proc_ops pr_ops_press = {
	.proc_open = temp_open,
	.proc_release = temp_release,
	.proc_read = press_read
};
static struct i2c_board_info bmp_info = {
	I2C_BOARD_INFO("BMP280", BMP_ADDR)
};

static int __init bmp_driver_init(void)
{
	temp_entry = proc_create("temp", 0444, NULL, &pr_ops);
	temp_entry = proc_create("press", 0444, NULL, &pr_ops_press);
	/* Get I2C Adapter */
	bmp_adapter = i2c_get_adapter(I2C_BUS_NO);
	
	if( bmp_adapter != NULL )
	{
		bmp_client = i2c_new_client_device(bmp_adapter, &bmp_info);
		if( bmp_client != NULL )
		{
			i2c_register_driver(THIS_MODULE, &bmp_driver);
		}
		i2c_put_adapter(bmp_adapter);
	}
	pr_info( "driver added!!\n" );
	return 0;
}

static u16 calib_T1,calib_P1;
static s16 calib_T2,calib_T3,calib_P2,calib_P3,calib_P4,calib_P5,calib_P6,calib_P7,calib_P8,calib_P9;

static int bmp_probe( struct i2c_client * bmp_client, const struct i2c_device_id * bmp_id )
{
	s32 id;
	id = i2c_smbus_read_byte_data(bmp_client, BMP_ID);
	pr_info("BMP280 id:%x\n",id);

	calib_T1 = i2c_smbus_read_word_data(bmp_client, BMP_T1);
	calib_T2 = i2c_smbus_read_word_data(bmp_client, BMP_T2);
	calib_T3 = i2c_smbus_read_word_data(bmp_client, BMP_T3);
	
	calib_P1 = i2c_smbus_read_word_data(bmp_client, BMP_P1);
	calib_P2 = i2c_smbus_read_word_data(bmp_client, BMP_P2);
	calib_P3 = i2c_smbus_read_word_data(bmp_client, BMP_P3);
	calib_P4 = i2c_smbus_read_word_data(bmp_client, BMP_P4);
	calib_P5 = i2c_smbus_read_word_data(bmp_client, BMP_P5);
	calib_P6 = i2c_smbus_read_word_data(bmp_client, BMP_P6);
	calib_P7 = i2c_smbus_read_word_data(bmp_client, BMP_P7);
	calib_P8 = i2c_smbus_read_word_data(bmp_client, BMP_P8);
	calib_P9 = i2c_smbus_read_word_data(bmp_client, BMP_P9);

	pr_info("calib data : %d %d %d\n",calib_T1,calib_T2,calib_T3);
	i2c_smbus_write_byte_data(bmp_client, BMP_CONFIG, 5<<5);
	i2c_smbus_write_byte_data(bmp_client, BMP_CTRL_MEAS, ((5<<5) | (5<<2) | (3<<0)));

	/* Read Temperature */

	return 0;
}
static int temp_open( struct inode * ind, struct file * fp )
{
	return 0;
}
static int temp_release( struct inode * ind, struct file * fp )
{
	return 0;
}

s64 t_fine;
static s32 read_temperature(void)
{
	int var1, var2;
	s32 raw_temp;
	s32 d1, d2, d3;

	/* Read Temperature */
	d1 = i2c_smbus_read_byte_data(bmp_client, BMP_TEMP_MSB);
	d2 = i2c_smbus_read_byte_data(bmp_client, BMP_TEMP_LSB);
	d3 = i2c_smbus_read_byte_data(bmp_client, BMP_TEMP_XLSB);
	raw_temp = ((d1<<16) | (d2<<8) | d3) >> 4;

	/* Calculate temperature in degree */
	var1 = ((((raw_temp >> 3) - (calib_T1 << 1))) * (calib_T2)) >> 11;

	var2 = (((((raw_temp >> 4) - (calib_T1)) * ((raw_temp >> 4) - (calib_T1))) >> 12) * (calib_T3)) >> 14;
	t_fine = var1 + var2;
	return ((t_fine) *5 +128) >> 8;
}

static u32 read_pressure(void)
{
	s64 var1, var2,p;
	s32 raw_press;
	s32 d1, d2, d3;
	s64 n;
	d1 = i2c_smbus_read_byte_data(bmp_client, BMP_PRESS_MSB);
	d2 = i2c_smbus_read_byte_data(bmp_client, BMP_PRESS_LSB);
	d3 = i2c_smbus_read_byte_data(bmp_client, BMP_PRESS_XLSB);
	raw_press = ((d1<<16) | (d2<<8) | d3) >> 4;

	var1 = t_fine - 128000;

	var2 = var1 * var1*(s64)calib_P6;
	var2 += ((var1*(s64)calib_P5)<<17);
	var2 += (((s64)calib_P4)<<35);
	
	var1 = ((var1*var1*(s64)calib_P3)>>8) + ((var1 *(s64)calib_P2)<<12);
	var1 = (((((s64)1)<<47)+var1))*((s64)calib_P1)>>33;

	if(var1 == 0)
		return 0;
	p = 1048576 - raw_press;
	n = (((p<<31)-var2)*3125);
	p = do_div(n,var1);
	var1 = ((p + var1 + var2) >> 8)+(((s64)calib_P7)<<4);
	return (u32)p;
}

static ssize_t temp_read( struct file * fp, char * buf, size_t len,loff_t * loff )
{
	s32 temperature;
	char temp[32];
	temperature = read_temperature();
	sprintf(temp, "%d.%d\n",temperature/100,temperature%100);
    temperature = copy_to_user(buf, &temp,strlen(temp));
	*loff=strlen(temp);
	return strlen(temp);
}

static ssize_t press_read( struct file * fp, char * buf, size_t len,loff_t * loff )
{
	u32 press;
	char temp[32];
	s32 temperature = read_temperature();
	press = read_pressure();
	sprintf(temp, "%d.%d\n",(press/256)/100,(press/256)%100);
    press = copy_to_user(buf, &temp,strlen(temp));
	*loff=strlen(temp);
	return strlen(temp);

}
static int bmp_remove( struct i2c_client * bmp_client )
{

	return 0;
}
void __exit bmp_driver_exit(void)
{
	proc_remove(temp_entry);
	proc_remove(press_entry);
	i2c_unregister_device(bmp_client);
    i2c_del_driver(&bmp_driver);
    pr_info("Driver Removed!!!\n");
}

module_init(bmp_driver_init);
module_exit(bmp_driver_exit);

MODULE_LICENSE("GPL");
