#include <linux/module.h> 
#include <linux/init.h>   
#include <linux/fs.h>   
#include <linux/device.h>  //device create and class create 
#include <asm/uaccess.h>	//copy to user / copy from user
#include <linux/interrupt.h>   
#include <linux/sched.h>   //wake_up_interruptible
#include <linux/gpio.h>   //gpio
#include <linux/irq.h>   //IRQ_TYPE_EDGE_FALLING
#include <linux/poll.h>   //poll wait

#define	BUTTON_MAJOR	239
#define DEV_NAME		"button"
#define BUF_SIZE		128
#define INT_PIN			25

static struct class *button_class ;
static volatile char ev_press = 0;
static DECLARE_WAIT_QUEUE_HEAD(button_wait_queue);
static struct fasync_struct *button_fasync;  

static irqreturn_t button_handler (int irq, void *dev_id) 
{ 
    printk("the irq number is: %d\n",irq); 
    ev_press = 1; 
    wake_up_interruptible(&button_wait_queue);
	/* 鐢╧ill_fasync鍑芥暟鍛婅瘔搴旂敤绋嬪簭锛屾湁鏁版嵁鍙浜? 
    * button_fasync缁撴瀯浣撻噷鍖呭惈浜嗗彂缁欒皝(PID鎸囧畾) 
    * SIGIO琛ㄧず瑕佸彂閫佺殑淇″彿绫诲瀷 
    * POLL_IN琛ㄧず鍙戦€佺殑鍘熷洜(鏈夋暟鎹彲璇讳簡) 
    */  
	kill_fasync( &button_fasync , SIGIO , POLL_IN); 
    return IRQ_HANDLED; 
} 


static ssize_t button_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{	
	ssize_t status ;
	unsigned char data[] = {"RaspberryPi"};
	printk("Now entering %s() \n", __FUNCTION__ );
	wait_event_interruptible(button_wait_queue, ev_press);
	status = copy_to_user(user_buf,data,count);
	printk("user read data is %s\n",user_buf );
	ev_press = 0 ;
	return status ;
}

static ssize_t button_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t	status ;
	unsigned char data[BUF_SIZE];
	printk("Now entering %s() \n", __FUNCTION__ );
	status = copy_from_user( data,user_buf,count);
	printk("user write data is %s\n",data);
	return  status ;
}

static int button_open(struct inode *inode, struct file *file)
{
	int err ;
	printk("Now entering %s() \n", __FUNCTION__ );
	err = request_irq(gpio_to_irq(INT_PIN), button_handler, IRQ_TYPE_EDGE_BOTH, "S0",NULL);
	return err ;
}

static int button_close(struct inode *inode, struct file *file)
{
	printk("Now entering %s() \n", __FUNCTION__ );
	free_irq(gpio_to_irq(INT_PIN), NULL );
	return 0 ;
}

static unsigned int button_poll(struct file *file , struct poll_table_struct *wait)
{
	unsigned int mask = 0 ;
	poll_wait(file , &button_wait_queue , wait );
	if(ev_press){
		mask |= POLLIN | POLLRDNORM ; 
	}
	return mask ; 
}

static int _button_fasync(int fd , struct file *file , int on)
{
	return fasync_helper(fd, file, on, &button_fasync);
}


struct file_operations button_ops = {
	.owner = THIS_MODULE ,
	.open = button_open ,
	.release = button_close ,
	.write = button_write ,
	.read = button_read ,
	.poll = button_poll ,
	.fasync = _button_fasync ,
};

static int __init button_init(void){
	int status ;
	printk("Hello driver world\n");
	status = register_chrdev( BUTTON_MAJOR , DEV_NAME , &button_ops);
	if ( status < 0 ){
		printk("Failed to register char device - %d\n" , BUTTON_MAJOR );
		return status ;
	}

	button_class = class_create(THIS_MODULE , "button-class");
	if (IS_ERR(button_class)){
		unregister_chrdev( BUTTON_MAJOR , DEV_NAME );
	}

	device_create(button_class ,NULL , MKDEV(BUTTON_MAJOR , 0), NULL ,  "hubuyu" );
	return 0;
}
	 
static void __exit button_exit(void){
	printk("Goodbye driver world\n");
	device_destroy(button_class , MKDEV(BUTTON_MAJOR ,0));
    class_destroy(button_class);
    unregister_chrdev(BUTTON_MAJOR , DEV_NAME);

}

module_init(button_init);
module_exit(button_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("hubuyu");
MODULE_DESCRIPTION("Lesson 8-5 : gpio-key button interrupt by asynchronous");

