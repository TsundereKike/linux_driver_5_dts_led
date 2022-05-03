#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#define DTS_lED_NUM     1
#define DTS_LED_NAME    "dts_led"

#define LED_ON          1
#define LED_OFF         0
/*LED相关寄存器地址映射后的虚拟地址指针*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/*dts_led设备结构体*/
struct dts_led_dev
{
    dev_t devid;            /*设备号*/
    struct cdev cdev;       /*字符设备*/
    struct class *class;    /*类*/
    struct device *device;  /*设备*/
    int major;              /*主设备号*/
    int minor;              /*次设备号*/
    struct device_node *nd;  /*设备节点*/
};
struct dts_led_dev dts_led; /*led设备*/

void led_switch(u8 sta)
{
    u32 val = 0;
    switch (sta)
    {
    case LED_ON:
        val = readl(GPIO1_DR);
        val &= ~(1<<3);
        writel(val,GPIO1_DR);
        break;
    case LED_OFF:
        val = readl(GPIO1_DR);
        val |= (1<<3);
        writel(val,GPIO1_DR);
        break;
    default:
        break;
    }
}

static int dts_led_open(struct inode *inode,struct file *filp)
{
    filp->private_data = &dts_led;
    return 0;
}
static int dts_led_release(struct inode *inode,struct file *filp)
{
    struct dts_led_dev *dev = (struct dts_led_dev*)filp->private_data;
    return 0;
}

static ssize_t dts_led_write(struct file *filp,const char __user *buf,
                                size_t count,loff_t *ppos)
{
    int ret;
    unsigned char databuf[1];
    ret = copy_from_user(databuf,buf,count);
    if(ret<0)
    {
        return -EFAULT;
    }
    led_switch(databuf[0]);
    return 0;
}
static const struct file_operations dts_led_fops = {
    .owner = THIS_MODULE,
    .write = dts_led_write,
    .open = dts_led_open,
    .release = dts_led_release,
};
/*入口*/
static int __init dts_led_init(void)
{
    int ret = 0;
    int i = 0;
    const char *str1;
    const char *str2;
    u32 reg_data[10];
    u32 val = 0;
    /*注册字符设备*/
    dts_led.major = 0;/*设备号由内核分配*/
    if(dts_led.major)/*定义了设备号*/
    {
        dts_led.devid = MKDEV(dts_led.major,0);
        ret = register_chrdev_region(dts_led.devid,DTS_lED_NUM,DTS_LED_NAME);
    }
    else             /*没有定义设备号*/
    {
        ret = alloc_chrdev_region(&dts_led.devid,0,DTS_lED_NUM,DTS_LED_NAME);
        dts_led.major = MAJOR(dts_led.devid);
        dts_led.minor = MINOR(dts_led.devid);
    }
    if(ret<0)
    {
        goto fail_devid;
    }
    /*添加字符设备*/
    dts_led.cdev.owner = THIS_MODULE;
    cdev_init(&dts_led.cdev,&dts_led_fops);
    ret = cdev_add(&dts_led.cdev,dts_led.devid,DTS_lED_NUM);
    if(ret<0)
    {
        goto fail_cdev;
    }
    /*自动创建设备节点*/
    dts_led.class =  class_create(THIS_MODULE,DTS_LED_NAME);
    if(IS_ERR(dts_led.class))
    {
        ret = PTR_ERR(dts_led.class);
        goto fail_class;
    }
    dts_led.device = device_create(dts_led.class,NULL,dts_led.devid,NULL,DTS_LED_NAME);
    if(IS_ERR(dts_led.device))
    {
        ret = PTR_ERR(dts_led.device);
        goto fail_device;
    }

    /*获取设备树属性内容*/
    dts_led.nd = of_find_node_by_path("/alphaled");
    if(dts_led.nd==NULL)
    {
        ret = -EINVAL;
        goto fail_find_nd;
    }
    /*获取属性*/
    ret = of_property_read_string(dts_led.nd,"status",&str1);
    if(ret<0)
    {
        goto fail_read_string;
    }
    else
    {
        printk("status = %s\r\n",str1);
    }
    ret = of_property_read_string(dts_led.nd,"compatible",&str2);
    if(ret<0)
    {
        goto fail_read_string;
    }
    else
    {
        printk("compatible = %s\r\n",str2);
    }
    ret = of_property_read_u32_array(dts_led.nd,"reg",reg_data,10);
    if(ret<0)
    {
        goto fail_read_u32_arr;
    }
    else
    {
        printk("reg=\r\n");
        for(i=0;i<10;i++)
        {
            printk("%#X ",reg_data[i]);  
        }
        printk("\r\n");
    }
    /*LED初始化*/
#if 0
    /*物理寄存器地址映射为虚拟地址*/
    IMX6U_CCM_CCGR1 = ioremap(reg_data[0],reg_data[1]);
    SW_MUX_GPIO1_IO03 = ioremap(reg_data[2],reg_data[3]);
    SW_PAD_GPIO1_IO03 = ioremap(reg_data[4],reg_data[5]);
    GPIO1_DR = ioremap(reg_data[6],reg_data[7]);
    GPIO1_GDIR = ioremap(reg_data[8],reg_data[9]);
#else
    IMX6U_CCM_CCGR1 = of_iomap(dts_led.nd,0);
    SW_MUX_GPIO1_IO03 = of_iomap(dts_led.nd,1);
    SW_PAD_GPIO1_IO03 = of_iomap(dts_led.nd,2);
    GPIO1_DR = of_iomap(dts_led.nd,3);
    GPIO1_GDIR = of_iomap(dts_led.nd,4);
#endif
    /*使能GPIO1时钟*/
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3<<26);
    val |= (3<<26);
    writel(val,IMX6U_CCM_CCGR1);
    /*设置GPIO_IO03的复用功能*/
    writel(5,SW_MUX_GPIO1_IO03);
    /*设置GPIO1_IO03电气属性*/
    writel(0x10b0,SW_PAD_GPIO1_IO03);
    /*设置GPIO1_IO03为输出功能*/
    val = readl(GPIO1_GDIR);
    val &= ~(1<<3);
    val |= (1<<3);
    writel(val,GPIO1_GDIR);
    /*默认关闭LED*/
    val = readl(GPIO1_DR);
    val |= (1<<3);
    writel(val,GPIO1_DR);
    
    return 0;

fail_read_u32_arr:

fail_read_string:

fail_find_nd:
    device_destroy(dts_led.class,dts_led.devid);
fail_device:
    class_destroy(dts_led.class);
fail_class:
    cdev_del(&dts_led.cdev);
fail_cdev:
    unregister_chrdev_region(dts_led.devid,DTS_lED_NUM);
fail_devid:
    return ret;
}
/*出口*/
static void __exit dts_led_exit(void)
{
    /*关闭LED*/
    led_switch(LED_OFF);
    /*取消地址映射*/
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    /*删除字符设备*/
    cdev_del(&dts_led.cdev);
    /*释放设备号*/
    unregister_chrdev_region(dts_led.devid,DTS_lED_NUM);
    /*摧毁设备*/
    device_destroy(dts_led.class,dts_led.devid);
    /*摧毁类*/
    class_destroy(dts_led.class);
}
/*注册驱动和卸载驱动*/
module_init(dts_led_init);
module_exit(dts_led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("tanminghang");


