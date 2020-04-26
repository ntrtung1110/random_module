#include <linux/module.h> 
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/random.h>

#include "vchar_driver.h"
#define DRIVER_AUTHOR "Nguyen Trong Tung ntrtung17@apcs.vn"
#define DRIVER_DESC   "A sample character device driver"
#define DRIVER_VERSION "0.1"

typedef struct vchar_dev{
	unsigned char * control_regs;
	unsigned char * status_regs;
	unsigned char * data_regs;
} vchar_dev_t;

struct _vchar_drv{
	dev_t dev_num;
	struct class *dev_class;
	struct device *dev;
	vchar_dev_t * vchar_hw;
	struct cdev *vcdev;
	unsigned int open_cnt;
} vchar_drv;

/****************************** device specific - START *****************************/
/* function to initialize device */
int vchar_hw_init(vchar_dev_t *hw)
{
	char *buf;
	buf = kzalloc(NUM_DEV_REGS * REG_SIZE, GFP_KERNEL);
	if (!buf){
		return -ENOMEM;
	}

	hw->control_regs = buf;
	hw->status_regs = hw->control_regs + NUM_CTRL_REGS;
	hw->data_regs = hw->status_regs + NUM_STS_REGS;
	
	// initialize value for register
	hw->control_regs[CONTROL_ACCESS_REG] = 0x03;
	hw->status_regs[DEVICE_STATUS_REG] = 0x03;

	return 0;
}
/* function to release device */

void vchar_hw_exit(vchar_dev_t *hw)
{
	kfree(hw->control_regs);
}

/* function to read from data register of device */
int vchar_hw_read_data(vchar_dev_t *hw, int start_reg, int num_regs, char* kbuf)
{
	int read_bytes = num_regs;

	// check permisson
	if ((hw->control_regs[CONTROL_ACCESS_REG] & CTRL_READ_DATA_BIT) == DISABLE)
		return -1;
	// check valid of address of kernel buffer
	if(kbuf == NULL)
		return -1;
	// check reasonable address of read register
	if(start_reg > NUM_DATA_REGS)
		return -1;
	// modify number of read register
	if(num_regs > (NUM_DATA_REGS - start_reg))
		read_bytes = NUM_DATA_REGS - start_reg;

	//generate random numbers
	int i;
	get_random_bytes(&i, sizeof(i));
	printk("Random number generated from kernel mode: %d", i);
	memcpy(hw->data_regs + start_reg, &i, sizeof(i));
	read_bytes = sizeof(i)+1;

	// write data from kernel buffer into data register
	memcpy(kbuf, hw->data_regs + start_reg, read_bytes);
		
	// update number of read from register
	hw->status_regs[READ_COUNT_L_REG] += 1;
	if(hw->status_regs[READ_COUNT_L_REG] == 0)
		hw->status_regs[READ_COUNT_H_REG] += 1;
	// return number of readable byte from data register
	return read_bytes;



}

/******************************* device specific - END *****************************/

/******************************** OS specific - START *******************************/
/* cac ham entry points */
static int vchar_driver_open(struct inode *inode, struct file *filp)
{
	vchar_drv.open_cnt++;
	printk("Handle opened event (%d)\n", vchar_drv.open_cnt);
	return 0;
}

static int vchar_driver_release(struct inode *inode, struct file *filp)
{
	printk("Handle closed event\n");
	return 0;
}

static ssize_t vchar_driver_read(struct file *filp, char __user *user_buf, size_t len, loff_t * off)
{
	char *kernel_buf = NULL;
	int num_bytes = 0;
	
	printk("Handle read event start from %lld, %zu bytes\n", *off, len);
	
	kernel_buf = kzalloc(len, GFP_KERNEL);
	if(kernel_buf == NULL)
		return 0;

	num_bytes = vchar_hw_read_data(vchar_drv.vchar_hw, *off, len, kernel_buf);
	printk("read %d bytes from HW\n", num_bytes);
	if(num_bytes < 0)
		return -EFAULT;
	if(copy_to_user(user_buf, kernel_buf, num_bytes))
		return -EFAULT;

	*off += num_bytes;
	return num_bytes;
}

static struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open = vchar_driver_open,
	.release = vchar_driver_release,
	.read = vchar_driver_read,
};
/* init driver */


static int __init vchar_driver_init(void)
{
	int ret = 0;

	vchar_drv.dev_num = 0;
	ret = alloc_chrdev_region(&vchar_drv.dev_num, 0, 1, "vchar_device");
	if (ret < 0){
		printk("failed to register device number dynamically\n");
		goto failed_register_devnum;
	}
	printk("allocated device number (%d,%d)\n", MAJOR(vchar_drv.dev_num), MINOR(vchar_drv.dev_num));
	
	/*create device file*/
	vchar_drv.dev_class = class_create(THIS_MODULE, "class_vchar_dev");
	if(vchar_drv.dev_class == NULL){
		printk("failed to create a device class\n");
		goto failed_create_class;
	}	
	vchar_drv.dev = device_create(vchar_drv.dev_class, NULL, vchar_drv.dev_num, NULL, "vchar_dev");
	if(IS_ERR(vchar_drv.dev)) {
		printk("failed to create a device\n");
		goto failed_create_device;
	}
	
	/* allocate mem for driver's struct and inializing */
	vchar_drv.vchar_hw = kzalloc(sizeof(vchar_dev_t), GFP_KERNEL);
	if(!vchar_drv.vchar_hw){
		printk("failed to allocate data structure of the driver\n");
		ret = -ENOMEM;
		goto failed_allocate_structure;
	}	
	/* initialize physical device*/
	ret = vchar_hw_init(vchar_drv.vchar_hw);
	if(ret < 0){
		printk("failed to initialize a virtual character device\n");
		goto failed_init_hw;
	}
	
	/*register entry point with kernel*/
	vchar_drv.vcdev = cdev_alloc();
	if(vchar_drv.vcdev == NULL){
		printk("failed to allocate cdev structure\n");
		goto failed_allocate_cdev;
	}
	cdev_init(vchar_drv.vcdev, &fops);
	ret = cdev_add(vchar_drv.vcdev, vchar_drv.dev_num, 1);
	if(ret < 0){
		printk("failed to add a char device to the system\n");
		goto failed_allocate_cdev;
	}
	/* register interrupts*/

	printk("Initialize vchar driver successfully\n");
	return 0;

failed_allocate_cdev:
	vchar_hw_exit(vchar_drv.vchar_hw);
failed_init_hw:
	kfree(vchar_drv.vchar_hw);
failed_allocate_structure:
	device_destroy(vchar_drv.dev_class, vchar_drv.dev_num);
failed_create_device:
	class_destroy(vchar_drv.dev_class);
failed_create_class:
	unregister_chrdev_region(vchar_drv.dev_num, 1);
failed_register_devnum:
	return ret;
}

/* exit driver */
static void __exit vchar_driver_exit(void)
{

	/* destroy registering entry point with kernel */

	cdev_del(vchar_drv.vcdev);	
	
	/* release physical device */
	vchar_hw_exit(vchar_drv.vchar_hw);	

	/* release memory updating struct of driver */
	kfree(vchar_drv.vchar_hw);	

	/* delete device file */
	device_destroy(vchar_drv.dev_class, vchar_drv.dev_num);
	class_destroy(vchar_drv.dev_class);	
	
	/* release device number */
	unregister_chrdev_region(vchar_drv.dev_num, 1);

	printk("Exit vchar driver\n");
}
/********************************* OS specific - END ********************************/

module_init(vchar_driver_init);
module_exit(vchar_driver_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR(DRIVER_AUTHOR); 
MODULE_DESCRIPTION(DRIVER_DESC); 
MODULE_VERSION(DRIVER_VERSION); 
MODULE_SUPPORTED_DEVICE("testdevice"); 
