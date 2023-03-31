
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
#include <linux/spi/spi.h>

#include <linux/proc_fs.h>
#include <asm/div64.h>


#define SPI_BUS_NO (0) /*For BBB for raspberry pi4 0 is more likely value*/

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


static struct spi_master *bmp_controller = NULL;
static struct spi_device *bmp_device = NULL;
static struct proc_dir_entry *temp_entry;
static struct proc_dir_entry *press_entry;

static int bmp_probe( struct spi_device *bmp);
static int bmp_remove( struct spi_device *bmp);

static const struct spi_device_id bmp_id [] ={
	{"BMP280",0},
	{}
};
MODULE_DEVICE_TABLE(spi, bmp_id);
static struct spi_driver bmp_driver_spi = {
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
static struct spi_board_info bmp_info = {
	.bus_num = SPI_BUS_NO,
	.chip_select = 0,
	.max_speed_hz = 4000000,
	.modalias = "bmp-spi-280",
	.mode = SPI_MODE_0
};
static int __init bmp_driver_init(void)
{
	temp_entry = proc_create("temperature_spi", 0444, NULL, &pr_ops);
	temp_entry = proc_create("press_spi", 0444, NULL, &pr_ops_press);
	
	bmp_controller = spi_busnum_to_master(SPI_BUS_NO);
	if ( bmp_controller != NULL )
	{
		bmp_device = spi_new_device(bmp_controller, &bmp_info);
		if( bmp_device != NULL )
		{
			bmp_device->bits_per_word = 8;
			if( spi_setup(bmp_device) )
			{
				pr_err("FAILED TO SETUP Device\n");
				spi_unregister_device(bmp_device);
			}
			spi_register_driver(&bmp_driver_spi);
		}
	
	}else{
		pr_err("could not get controller\n");
		return -1;
	}
	pr_info( "driver added!!\n" );
	return 0;
}

static u16 calib_T1,calib_P1;
static s16 calib_T2,calib_T3,calib_P2,calib_P3,calib_P4,calib_P5,calib_P6,calib_P7,calib_P8,calib_P9;

static int bmp_probe( struct spi_device * bmp )
{
	u8 config_data [] = { (BMP_CONFIG&(0x7F)),5<<5,(BMP_CTRL_MEAS&(0x7F)),((5<<5)|(5<<2)|(3<<0)) };
	s32 id;
	id = spi_w8r8(bmp_device, BMP_ID);
	pr_info("BMP280 id:%x\n",id);

	calib_T1 = spi_w8r16(bmp_device,BMP_T1);
	calib_T2 = spi_w8r16(bmp_device,BMP_T2);
	calib_T3 = spi_w8r16(bmp_device,BMP_T3);
	
	calib_P1 = spi_w8r16(bmp_device, BMP_P1);
	calib_P2 = spi_w8r16(bmp_device, BMP_P2);
	calib_P3 = spi_w8r16(bmp_device, BMP_P3);
	calib_P4 = spi_w8r16(bmp_device, BMP_P4);
	calib_P5 = spi_w8r16(bmp_device, BMP_P5);
	calib_P6 = spi_w8r16(bmp_device, BMP_P6);
	calib_P7 = spi_w8r16(bmp_device, BMP_P7);
	calib_P8 = spi_w8r16(bmp_device, BMP_P8);
	calib_P9 = spi_w8r16(bmp_device, BMP_P9);

	pr_info("calib data : %d %d %d\n",calib_T1,calib_T2,calib_T3);
	spi_write(bmp_device, config_data, sizeof(config_data));

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
	d1 = spi_w8r8(bmp_device, BMP_TEMP_MSB);
	d2 = spi_w8r8(bmp_device, BMP_TEMP_LSB);
	d3 = spi_w8r8(bmp_device, BMP_TEMP_XLSB);
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
	d1 = spi_w8r8(bmp_device, BMP_PRESS_MSB);
	d2 = spi_w8r8(bmp_device, BMP_PRESS_LSB);
	d3 = spi_w8r8(bmp_device, BMP_PRESS_XLSB);
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
static int bmp_remove( struct spi_device *bmp )
{
	return 0;
}
void __exit bmp_driver_exit(void)
{
	proc_remove(temp_entry);
	proc_remove(press_entry);
	spi_unregister_device(bmp_device);
	pr_info("Driver Removed!!!\n");
}

module_init(bmp_driver_init);
module_exit(bmp_driver_exit);

MODULE_LICENSE("GPL");
