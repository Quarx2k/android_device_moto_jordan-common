/*
  ICS Compatibility sysfs wrapper Module For moto 2.6.32 kernels

  Tanguy Pruvot 2011. GPL Licence.

  Made for the Motorola Defy (jordan)

*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <linux/usb/android_composite.h>
#include <linux/usb/ch9.h>

#define TAG "android_usb"
#define SYSDEV "android0"

#define FUNCTIONS_SZ 255
struct android_usb_data {

    struct device dev;
    struct device *pdev;

    int enable;
    char functions[FUNCTIONS_SZ];

    struct usb_device_descriptor desc;
};

struct android_usb_dev {
    struct device *dev;
};

static struct class *android_usb_class = NULL;
static struct android_usb_data *android0 = NULL;

static struct file_operations android_usb_fops = {
    .owner = THIS_MODULE,
};

static int android0_major = -1;

#define KDBG(format, ...) printk(KERN_DEBUG TAG ": " format, ## __VA_ARGS__)
#define KINFO(format, ...) printk(KERN_INFO TAG ": " format, ## __VA_ARGS__)

#define to_android_usb_data(d) \
    container_of(d, struct android_usb_data, dev)

/*****************************************************************************/
//enable : 0/1

static ssize_t show_enable(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    if (android0 != NULL) {
       return sprintf(buf, "%d\n", android0->enable);
    }
    return sprintf(buf, "\n");
}
static ssize_t set_enable(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
    int value;
    if (sscanf(buf, "%d", &value) != 1) return -EINVAL;
    if (android0 != NULL) {
       android0->enable = (int) (value != 0);
    }
    return count;
}
static DEVICE_ATTR(enable, 0664, show_enable, set_enable);

//functions : string
static ssize_t show_functions(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    if (android0 != NULL) {
       return sprintf(buf, "%s\n", android0->functions);
    }
    return sprintf(buf, "\n");
}
static ssize_t set_functions(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
    char buffer[FUNCTIONS_SZ];
    if (sscanf(buf, "%s", buffer) != 1) return -EINVAL;
    if (android0 != NULL) {
       strncpy(android0->functions, buffer, FUNCTIONS_SZ);
    }
    return count;
}
static DEVICE_ATTR(functions, 0664, show_functions, set_functions);

//idVendor : usb descriptor
static ssize_t show_desc_idVendor(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    if (android0 != NULL) {
       return sprintf(buf, "%x\n", android0->desc.idVendor);
    }
    return sprintf(buf, "\n");
}
static ssize_t set_desc_idVendor(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
    int value;
    if (sscanf(buf, "%x", &value) != 1) return -EINVAL;
    if (android0 != NULL) {
       android0->desc.idVendor = value;
    }
    return count;
}
static DEVICE_ATTR(idVendor, 0664, show_desc_idVendor, set_desc_idVendor);

//idProduct
static ssize_t show_desc_idProduct(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    if (android0 != NULL) {
       return sprintf(buf, "%x\n", android0->desc.idProduct);
    }
    return sprintf(buf, "\n");
}
static ssize_t set_desc_idProduct(struct device *dev,
                   struct device_attribute *attr, const char *buf, size_t count)
{
    int value;
    if (sscanf(buf, "%x", &value) != 1) return -EINVAL;
    if (android0 != NULL) {
       android0->desc.idProduct = value;
    }
    return count;
}
static DEVICE_ATTR(idProduct, 0664, show_desc_idProduct, set_desc_idProduct);

/*****************************************************************************/

int android_usb_create_sysfs_files(struct android_usb_data *data)
{
   int ret;
   ret  = device_create_file(data->pdev, &dev_attr_enable);
   ret |= device_create_file(data->pdev, &dev_attr_functions);
   ret |= device_create_file(data->pdev, &dev_attr_idVendor);
   ret |= device_create_file(data->pdev, &dev_attr_idProduct);
   return ret;
}

void android_usb_remove_sysfs_files(struct android_usb_data *data)
{
    device_remove_file(data->pdev, &dev_attr_enable);
    device_remove_file(data->pdev, &dev_attr_functions);
    device_remove_file(data->pdev, &dev_attr_idVendor);
    device_remove_file(data->pdev, &dev_attr_idProduct);
}

/*****************************************************************************/

static int __init android_usb_init(void)
{
    int ret;
    struct android_usb_data *data;

    android_usb_class = class_create(THIS_MODULE, TAG);
    if (!android_usb_class) {
        return -ENODEV;
    }
    KDBG("registered android_usb class");

    android0_major = register_chrdev(0, SYSDEV, &android_usb_fops);
    if (android0_major < 0) {
        return -ENODEV;
    }

    data = kzalloc(sizeof(struct android_usb_data), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }

    data->pdev = device_create(
        android_usb_class, NULL,
        MKDEV(android0_major, 0),
        data, SYSDEV
    );

    ret = android_usb_create_sysfs_files(data);
    if (ret < 0) {
        return -ENODEV;
    }

    //init data :
    data->desc.idVendor  = 0x22b8;
    data->desc.idProduct = 0x428c;

    data->enable = 1;
    strcpy(data->functions,"adb");

    android0 = data;

    KINFO("registered android0 device");

    return ret;
}

static void __exit android_usb_exit(void)
{
    if (android0 != NULL) {
        android_usb_remove_sysfs_files(android0);
        device_destroy(android_usb_class, MKDEV(android0_major, 0));
        kfree(android0);
        KDBG("unregistered android0 device");
        android0 = NULL;
    }
    if (android0_major >= 0) {
        unregister_chrdev(android0_major, SYSDEV);
        android0_major = -1;
    }
    if (android_usb_class != NULL) {
        class_destroy(android_usb_class);
        KINFO("unregistered android_usb class");
        android_usb_class = NULL;
    }
}

module_init(android_usb_init);
module_exit(android_usb_exit);

MODULE_AUTHOR("Tanguy Pruvot");
MODULE_DESCRIPTION("ICS sysfs usb wrapper");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");

