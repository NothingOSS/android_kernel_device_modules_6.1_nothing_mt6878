/*****************************************************************************
* DEBUG function define here
*****************************************************************************/
//#if FTS_DEBUG_EN
#define EPH_INFO(dev, fmt, args...) do { \
    printk(KERN_INFO "[EPH_TS %s]%s:"fmt"\n",dev_name(dev), __func__, ##args); \
} while (0)

#define EPH_DBG(dev, fmt, args...) do { \
    printk(KERN_DEBUG "[EPH_TS %s]%s:"fmt"\n",dev_name(dev), __func__, ##args); \
} while (0)

#define EPH_WARN(dev, fmt, args...) do { \
    printk(KERN_WARNING "[EPH_TS %s]%s:"fmt"\n",dev_name(dev), __func__, ##args); \
} while (0)

#define EPH_ERR(dev, fmt, args...) do { \
    printk(KERN_ERR "[EPH_TS %s]%s:"fmt"\n",dev_name(dev), __func__, ##args); \
} while (0)

#define FTS_INFO(fmt, args...) do { \
    printk(KERN_INFO "[FTS_TS/I]%s:"fmt"\n", __func__, ##args); \
} while (0)

#define FTS_ERROR(fmt, args...) do { \
    printk(KERN_ERR "[FTS_TS/E]%s:"fmt"\n", __func__, ##args); \
} while (0)
//#endif  __LINUX_FOCALTECH_COMMON_H__ 
