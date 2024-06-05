
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <video/mmp_disp.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/io.h>
//#include <soc/qcom/scm.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
static struct kobject *bootinfo_kobj = NULL;
static struct kobject *debug_info_kobj = NULL;

#define CREATE_DEVICE_ATTR(_name)                       \
		&_name##_attr.attr

#define CREATE_DEVICE_INFO(_name)                       \
extern char _name[32];                                  \
static ssize_t _name##_show(struct kobject *kobj,       \
		struct kobj_attribute *attr, char * buf)        \
{                                                       \
	char *s = buf;                                      \
	s += sprintf(s, "%s\n", _name);                     \
	return (s - buf);                                   \
}                                                       \
                                                        \
static ssize_t _name##_store(struct kobject *kobj,       \
		struct kobj_attribute *attr, const char * buf, size_t n)    \
{                                                       \
	return n;                                           \
}                                                       \
                                                        \
static struct kobj_attribute _name##_attr = {           \
	.attr = {                                           \
		.name = #_name,                                 \
		.mode = 0644,                                   \
	},                                                  \
	.show = _name##_show,                               \
	.store = _name##_store,                             \
}

char main_camera[32];
EXPORT_SYMBOL(main_camera);

char sub_front_camera[32];
EXPORT_SYMBOL(sub_front_camera);

char depth_camera[32];
EXPORT_SYMBOL(depth_camera);

char main_camera_sn[64];
EXPORT_SYMBOL(main_camera_sn);

char depth_camera_sn[64];
EXPORT_SYMBOL(depth_camera_sn);

char front_camera_sn[64];
EXPORT_SYMBOL(front_camera_sn);

char main_module_id[64];
EXPORT_SYMBOL(main_module_id);
//const u8 * sub_front_camera[]={"sub_front_camera error to find!","SHK1_sc520cs_lh","SHK2_gc5035_cxt"}; //front_camera module information
//const u8 * main_camera[]={"main_camera error to find!","SHK1_ov13b10_lh","SHK2_sc1320cs_sunwin","SHK1_hi1634b_lh"}; // main_camera module information
const u8 * wide_camera[]={"wide_camera not found!"}; //wide_camera module information
//const u8 * depth_camera[]={"depth_camera error to find!","SHK1_sc202cs_sega","SHK2_gc02m0b_cxt"}; //depth_camera module information
const u8 * macro_camera[]={"macro_camera not found!"}; //macro_camera module information

int torch_flash_level=0;
int main_camera_find_success = 0;
int wide_camera_find_success = 0;
int depth_camera_find_success = 0;
int macro_camera_find_success = 0;
int front_camera_find_success = 0;

bool main_camera_probe_ok = 0;
bool wide_camera_probe_ok = 0;
bool depth_camera_probe_ok = 0;
bool macro_camera_probe_ok = 0;
bool front_camera_probe_ok = 0;

//char main_camera_msn[64] = {0};
char wide_camera_msn[64] = {0};
//char depth_camera_msn[64] = {0};
//char front_camera_msn[64] = {0};
char cam_depth_lens_efuse[64] = {0};
char cam_back_lens_efuse[64] = {0};
char cam_front_lens_efuse[64] = {0};

char touch_version[32] = "tp unknow";
EXPORT_SYMBOL(touch_version);
extern char panel_name_find[128];
extern char haptic_info[64];

#ifdef CAM_ERR_STATUS
u32 cam_err_code = 0x00000000;
#endif

static ssize_t vibrator_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%s\n",haptic_info);
	return (s - buf);
}

static ssize_t vibrator_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute vibrator_info_attr = {
	.attr = {
		.name = "vibrator_info",
		.mode = 0644,
	},
	.show =&vibrator_info_show,
	.store= &vibrator_info_store,
};

static ssize_t tp_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%s\n",touch_version);
	return (s - buf);
}

static ssize_t tp_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute tp_info_attr = {
	.attr = {
		.name = "tp_info",
		.mode = 0644,
	},
	.show =&tp_info_show,
	.store= &tp_info_store,
};

static ssize_t cable_state_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	unsigned int cable_gpio = 8;
	unsigned int gpio_base = 296;
	int pin_val = 0;
	char *s = buf;

	pin_val = gpio_get_value(gpio_base + cable_gpio) & 0x01;

	printk(KERN_INFO "%s: cable_gpio is %d ;\n",__func__, pin_val);

	s += sprintf(s, "%d\n",pin_val);
	return (s - buf);
}

static ssize_t cable_state_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute cable_state_attr = {
	.attr = {
		.name = "cablestate",
		.mode = 0644,
	},
	.show =&cable_state_show,
	.store= &cable_state_store,
};

extern void lcm_panel_get_data(void);
static ssize_t lcd_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	lcm_panel_get_data();
	s += sprintf(s, "%s\n",panel_name_find);
	return (s - buf);
}
static ssize_t lcd_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute lcd_info_attr = {
	.attr = {
		.name = "lcd_info",
		.mode = 0644,
	},
	.show =&lcd_info_show,
	.store= &lcd_info_store,
};

extern char slave_charger_info_1[32];
extern char slave_charger_info_2[32];
static ssize_t slave_charger_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char * buf)
{
	char *s = buf;

	if (slave_charger_info_1[0]
			&& (memcmp(slave_charger_info_1, "unknown", 7) != 0))
		s += sprintf(s, "%s\n", slave_charger_info_1);
	else if (slave_charger_info_2[0]
			&& (memcmp(slave_charger_info_2, "unknown", 7) != 0))
		s += sprintf(s, "%s\n", slave_charger_info_2);
	else
		s += sprintf(s, "%s\n", "unknown");

	return (s - buf);
}

static ssize_t slave_charger_info_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

static struct kobj_attribute slave_charger_info_attr = {
	.attr = {
		.name = "slave_charger_info",
		.mode = 0644,
	},
	.show = slave_charger_info_show,
	.store = slave_charger_info_store,
};

CREATE_DEVICE_INFO(typec_info);
CREATE_DEVICE_INFO(fuel_gauge_info);
CREATE_DEVICE_INFO(main_charger_info);
CREATE_DEVICE_INFO(bat_info);

#define BASE_ADDRESS  	   0x000A0138
#define BIT_COUNT_SBL1	   0x00000FFE
#define BIT_COUNT_APPSBL0  0xFFFC0000
#define BIT_COUNT_APPSBL1  0xFFFFFFFF
#define BIT_COUNT_APPSBL2  0x0000000F
#define BIT_COUNT_MSS	   0xFFFF0000
#define JTAG_ADDRESS  	   0x000A0150
#define BASE_JTAG_OFFSET   (BASE_ADDRESS-JTAG_ADDRESS)
#define ANTI_BACK_ADDR_LEN 24
#define JUDGE_BIT(D,N)     (( D >> N) & 1)         
void *ptr;
char show_ver[10];
phys_addr_t p = JTAG_ADDRESS;

static int countBits(unsigned int n) {
	int count = 0;
	while(n != 0) {
		n = n & (n-1);
		count++;
	}
	return count;
}

static char *show_tag(size_t cnt){
	char *tag;
	switch(cnt){
		case 1: 
			tag = "1";
			break;
		case 2: 
			tag = "2";
			break;
		case 3: 
			tag = "3";
			break;
		case 4: 
			tag = "4";
			break;
		case 5: 
			tag = "5";
			break;
		case 6: 
			tag = "6";
			break;
		case 7: 
			tag = "7";
			break;
		case 8: 
			tag = "8";
			break;
		case 9: 
			tag = "9";
			break;
		case 10: 
			tag = "A";
			break;
		case 11: 
			tag = "B";
			break;
		case 12: 
			tag = "C";
			break;
		case 13: 
			tag = "D";
			break;
		default: 
			tag = "F";
			break;
	}
	return tag;
}

static inline unsigned long size_inside_page(unsigned long start,
                                             unsigned long size)
{
        unsigned long sz;

        sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));

        return min(sz, size);
}

static ssize_t read_address(char *buf,int offset,int cnt)
{
	ssize_t read,sz;
	ssize_t count = cnt;
	char pm[ANTI_BACK_ADDR_LEN] = {'\0'};
	read = 0;

	while (count > 0) {

		sz = size_inside_page(p+offset, count);
		if (!ptr)
			return -EFAULT;

		memcpy(pm,ptr+offset,sz);
		count -= sz;
		p += sz;
		read += sz;
	}
	memcpy(buf,pm,sizeof(pm));
	return read;
}
static ssize_t image_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	size_t ret;
	int addrLen = ANTI_BACK_ADDR_LEN;
	char verBuf[ANTI_BACK_ADDR_LEN] = {'\0'};
	char *ver = "V-";
	
	unsigned int modem = 0;
	unsigned int sbl1 = 0;
	unsigned int appsbl0 = 0;
	unsigned int appsbl1 = 0;
	unsigned int appsbl2 = 0;
	ret = read_address(verBuf,BASE_JTAG_OFFSET,addrLen);
	if(!ret)
		printk("img version get err\n");
	
	memset(show_ver,'\0',sizeof(show_ver));
	memcpy(show_ver,ver,strlen(ver));
	
	//modem:base+0x0010[31:16]
	memcpy(&modem,verBuf+0x0010,4);
	modem &= BIT_COUNT_MSS;
	ret = countBits(modem);
	memcpy(show_ver+strlen(ver),show_tag(ret),1);
		
	//SBL1:base+0x0000[11:1]
	ret = 0;
	memcpy(&sbl1,verBuf,4);
	sbl1 &= BIT_COUNT_SBL1;
	ret = countBits(sbl1);
	memcpy(show_ver+strlen(ver)+1,show_tag(ret),1);

	//Appsbl:base+0x0004[31:18];base+0x0008[31:0];base+0x000c[3:0]
	ret = 0;
	memcpy(&appsbl0,verBuf+0x0004,4);
	appsbl0 &= BIT_COUNT_APPSBL0;
	ret += countBits(appsbl0);

	memcpy(&appsbl1,verBuf+0x0008,4);
	appsbl1 &= BIT_COUNT_APPSBL1;
	ret += countBits(appsbl1);
	
	memcpy(&appsbl2,verBuf+0x000c,4);
	appsbl2 &= BIT_COUNT_APPSBL2;
	ret += countBits(appsbl2);
	
	memcpy(show_ver+strlen(ver)+1+1,show_tag(ret),1);

	return sprintf(buf,"%s\n",show_ver);
 }

static struct kobj_attribute sboot_image_info_attr = { 
        .attr = { 
                .name = "hw_image_info",
                .mode = 0444,
        },
        .show =&image_info_show,
};

static ssize_t jtag_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	size_t ret;
        int addrLen = ANTI_BACK_ADDR_LEN;
        char verBuf[ANTI_BACK_ADDR_LEN] = {'\0'};
	char *str;
        ret = read_address(verBuf,0,addrLen);
        if(!ret)
                printk("img version get err\n");
	
	//base:0x000A0150 +bit[36]
	if( JUDGE_BIT(verBuf[4],4) &&		
	     //base:0x000A0150 +bit[42]
	     JUDGE_BIT(verBuf[5],2) &&		
             //base:0x000A0150 +bit[44]
	     JUDGE_BIT(verBuf[5],4) &&		
	     //base:0x000A0150 +bit[46]
	     JUDGE_BIT(verBuf[5],6) &&		
	     //base:0x000A0150 +bit[48]
	     JUDGE_BIT(verBuf[6],0)) {		
		str = "Blown";
	}else{
		str = "Not Blown";
	}	
	return sprintf(buf,"%s\n",str);	
}
static struct kobj_attribute sboot_jtag_fuse_attr = { 
        .attr = { 
                .name = "hw_jtag_info",
                .mode = 0444,
        },
        .show =&jtag_info_show,
};

static ssize_t main_camera_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(main_camera)){
		sprintf(buf, "main_camera error to find!\n");
	} else {
		sprintf(buf, "%s\n", main_camera);
	}
	return sizeof(main_camera);
}

static ssize_t main_camera_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute main_camera_info_attr = {
	.attr = {
		.name = "cam_back_lens_mfr",
		.mode = 0644,
	},
	.show =&main_camera_info_show,
	.store= &main_camera_info_store,
};

static ssize_t front_camera_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(sub_front_camera)){
		sprintf(buf, "sub_front_camera error to find!\n");
	} else {
		sprintf(buf, "%s\n", sub_front_camera);
	}
	return sizeof(sub_front_camera);
}

static ssize_t front_camera_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute front_camera_info_attr = {
	.attr = {
		.name = "cam_front_lens_mfr",
		.mode = 0644,
	},
	.show =&front_camera_info_show,
	.store= &front_camera_info_store,
};

static ssize_t wide_camera_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	if((wide_camera_find_success >= sizeof(wide_camera) / sizeof(char *)) || (wide_camera_find_success < 0)) {
		s += sprintf(s, "%s\n", wide_camera[0]);
	}else{
		s += sprintf(s, "3_%s\n", wide_camera[wide_camera_find_success]);
	}	
	return (s - buf);
}

static ssize_t wide_camera_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute wide_camera_info_attr = {
	.attr = {
		.name = "wide_camera",
		.mode = 0644,
	},
	.show =&wide_camera_info_show,
	.store= &wide_camera_info_store,
};
static ssize_t depth_camera_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(depth_camera)){
		sprintf(buf, "depth_camera error to find!\n");
	} else {
		sprintf(buf, "%s\n", depth_camera);
	}
	return sizeof(depth_camera);
}

static ssize_t depth_camera_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute depth_camera_info_attr = {
	.attr = {
		.name = "cam_depth_lens_mfr",
		.mode = 0644,
	},
	.show =&depth_camera_info_show,
	.store= &depth_camera_info_store,
};


#ifdef CAM_ERR_STATUS
//The camera error code is defined in camera_err_code.h Add by zuoerfan
static ssize_t cam_err_status_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
  	char *s = buf;
  
  	if( 0 == (cam_err_code & 0x7FFFFFFF))
  	{
  		s += sprintf(s, "%s\n", "pass");
  	}
  	else
  	{
  		s += sprintf(s, "0x%x\n", (cam_err_code & 0x7FFFFFFF)); //QCOM
  	}
  
  	return (s - buf);
}
  
static ssize_t cam_err_status_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
  	int ret = 0;
  	u32 temp = 0x0;
  
  	ret = kstrtouint(buf, 0, &temp);
  	cam_err_code = temp & 0x7FFFFFFF;
  
  	return n;
}
  
static struct kobj_attribute cam_err_status_attr = {
  	.attr = {
  		.name = "cam_err_status",
  		.mode = 0644,
  	},
  	.show =&cam_err_status_show,
  	.store= &cam_err_status_store,
 };
#endif

static ssize_t main_camera_msn_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(main_camera_sn)){
		sprintf(buf, "main_camera_sn error to find!\n");
	} else {
		sprintf(buf, "%s\n", main_camera_sn);
	}
	return sizeof(main_camera_sn);
}

static ssize_t hs_main_depth_camera_msn_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "MainCamera:%s,;AuxCamera:%s,;", main_camera_sn, depth_camera_sn);
}

static ssize_t main_camera_msn_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute main_camera_msn_attr = {
	.attr = {
		.name = "main_camera_msn",
		.mode = 0644,
	},
	.show =&main_camera_msn_show,
	.store= &main_camera_msn_store,
};

static struct kobj_attribute hs_main_depth_camera_msn_attr = {
	.attr = {
		.name = "hs_cam_msn",
		.mode = 0644,
	},
	.show =&hs_main_depth_camera_msn_show,
};

static ssize_t front_camera_msn_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(front_camera_sn)){
		sprintf(buf, "front_camera_sn error to find!\n");
	} else {
		sprintf(buf, "%s\n", front_camera_sn);
	}
	return sizeof(front_camera_sn);
}

static ssize_t front_camera_msn_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute front_camera_msn_attr = {
	.attr = {
		.name = "front_camera_msn",
		.mode = 0644,
	},
	.show =&front_camera_msn_show,
	.store= &front_camera_msn_store,
};

static ssize_t wide_camera_msn_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(main_module_id)){
		sprintf(buf, "main_module_id error to find!\n");
	} else {
		sprintf(buf, "%s\n", main_module_id);
	}
	return sizeof(main_module_id);
}

static ssize_t wide_camera_msn_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute wide_camera_msn_attr = {
	.attr = {
		.name = "wide_camera_msn",
		.mode = 0644,
	},
	.show =&wide_camera_msn_show,
	.store= &wide_camera_msn_store,
};

static ssize_t depth_camera_msn_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	if(0 == strlen(depth_camera_sn)){
		sprintf(buf, "depth_camera_sn error to find!\n");
	} else {
		sprintf(buf, "%s\n", depth_camera_sn);
	}
	return sizeof(depth_camera_sn);

}

static ssize_t depth_camera_msn_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute depth_camera_msn_attr = {
	.attr = {
		.name = "depth_camera_msn",
		.mode = 0644,
	},
	.show =&depth_camera_msn_show,
	.store= &depth_camera_msn_store,
};

static ssize_t cam_depth_lens_efuse_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%s", cam_depth_lens_efuse);

}

static ssize_t cam_depth_lens_efuse_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute cam_depth_lens_efuse_attr = {
	.attr = {
		.name = "cam_depth_lens_efuse",
		.mode = 0644,
	},
	.show =&cam_depth_lens_efuse_show,
	.store= &cam_depth_lens_efuse_store,
};
static ssize_t cam_back_lens_efuse_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%s", cam_back_lens_efuse);

}

static ssize_t cam_back_lens_efuse_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute cam_back_lens_efuse_attr = {
	.attr = {
		.name = "cam_back_lens_efuse",
		.mode = 0644,
	},
	.show =&cam_back_lens_efuse_show,
	.store= &cam_back_lens_efuse_store,
};
static ssize_t cam_front_lens_efuse_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%s", cam_front_lens_efuse);

}

static ssize_t cam_front_lens_efuse_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute cam_front_lens_efuse_attr = {
	.attr = {
		.name = "cam_front_lens_efuse",
		.mode = 0644,
	},
	.show =&cam_front_lens_efuse_show,
	.store= &cam_front_lens_efuse_store,
};

#include <linux/gpio.h>
int get_hw_prj(void)
{
	unsigned int gpio_base =343;
	unsigned int pin0=93;
	unsigned int pin1=92;

	int pin_val = 0;
	int hw_prj=0;

	
	pin_val =    gpio_get_value(gpio_base+pin0) & 0x01;
	pin_val |= (gpio_get_value(gpio_base+pin1) & 0x01) << 1;
	hw_prj = pin_val;
	
	printk(KERN_ERR "%s: hw_prj is %x ;\n",__func__, hw_prj);

	return  hw_prj;
	
}
static ssize_t get_hw_prj_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;

      s += sprintf(s, "0x%02x\n",get_hw_prj());
	
	return (s - buf);
	
}

static struct kobj_attribute get_hw_prj_attr = {
	.attr = {
		.name = "hw_prj",
		.mode = 0644,
	},
	.show =&get_hw_prj_show,
};

int get_hw_ver_info(void)
{
    unsigned int gpio_base =343;
	unsigned int pin0=121;
	unsigned int pin1=54;
	unsigned int pin2=53;
	unsigned int pin3=5;
	unsigned int pin4=11;
	int pin_val = 0;
	int hw_ver=0;

	
	pin_val =    gpio_get_value(gpio_base+pin0) & 0x01;
	pin_val |= (gpio_get_value(gpio_base+pin1) & 0x01) << 1;
	pin_val |= (gpio_get_value(gpio_base+pin2) & 0x01) << 2;
	pin_val |= (gpio_get_value(gpio_base+pin3) & 0x01) << 3;
	pin_val |= (gpio_get_value(gpio_base+pin4) & 0x01) << 4;
	hw_ver = pin_val;
	
	printk(KERN_ERR "%s: hw_ver is %x ;\n",__func__, hw_ver);

	return  hw_ver;

}
static ssize_t get_hw_ver_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;

      s += sprintf(s, "0x%02x\n",get_hw_ver_info());
	
	return (s - buf);
}

static struct kobj_attribute get_hw_ver_attr = {
	.attr = {
		.name = "hw_ver",
		.mode = 0644,
	},
	.show =&get_hw_ver_show,
};


int get_sd_status_info(void)
{ 
	unsigned int cd_gpios =310;
	int pin_val = 0;
	
	pin_val = gpio_get_value(cd_gpios);
	
	printk(KERN_ERR "%s: hw_sd_tray is %d ;\n",__func__, !!pin_val);

	return  !!pin_val;

}
static ssize_t get_sd_status_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;

	s += sprintf(s, "%d\n",get_sd_status_info());
	
	return (s - buf);
}

static struct kobj_attribute get_sd_status_attr = {
	.attr = {
		.name = "hw_sd_tray",
		.mode = 0644,
	},
	.show =&get_sd_status_show,
};

static struct attribute * g[] = {
	&get_hw_prj_attr.attr,
	&get_hw_ver_attr.attr,
	&tp_info_attr.attr,
	&lcd_info_attr.attr,
	&get_sd_status_attr.attr,
	//&nfc_info_attr.attr,
	&main_camera_info_attr.attr,
	&wide_camera_info_attr.attr,
	&depth_camera_info_attr.attr,
	&front_camera_info_attr.attr,
	
#ifdef CAM_ERR_STATUS
	&cam_err_status_attr.attr,
#endif
	&main_camera_msn_attr.attr,
	&front_camera_msn_attr.attr,
	&wide_camera_msn_attr.attr,
	&depth_camera_msn_attr.attr,
	&cam_depth_lens_efuse_attr.attr,
	&cam_back_lens_efuse_attr.attr,
	&cam_front_lens_efuse_attr.attr,
	&sboot_image_info_attr.attr,
	&sboot_jtag_fuse_attr.attr,
	&cable_state_attr.attr,
	&slave_charger_info_attr.attr,
	CREATE_DEVICE_ATTR(typec_info),
	CREATE_DEVICE_ATTR(fuel_gauge_info),
	CREATE_DEVICE_ATTR(main_charger_info),
	CREATE_DEVICE_ATTR(bat_info),
	&vibrator_info_attr.attr,
	NULL,
};

static struct attribute * debug_info[] = {
	&hs_main_depth_camera_msn_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static struct attribute_group debug_attr_group = {
	.attrs = debug_info,
};


static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;
	
	//printk("%s,line=%d\n",__func__,__LINE__);  

	bootinfo_kobj = kobject_create_and_add("ontim_bootinfo", NULL);
	debug_info_kobj = kobject_create_and_add("debug_control", NULL);

	if ((bootinfo_kobj == NULL) || (debug_info_kobj == NULL)) {
		printk("bootinfo_init: kobject_create_and_add failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		printk("bootinfo_init: sysfs_create_group failed\n");
		goto sys_fail;
	}
	
	ret = sysfs_create_group(debug_info_kobj, &debug_attr_group);
	if (ret) {
		printk("bootinfo_init: sysfs_create_group failed\n");
		goto sys_fail;
	}
	
	if(!request_mem_region(p,SZ_4K*16,"anti_rollback")){
		printk("Failed to request core resources");
		goto sys_fail;
	}
		
	ptr = ioremap(p,SZ_4K*16);
	if (!ptr) {
		printk("error to ioremap anti rollback addr\n");
		goto map_fail;
	}	
	return ret;
map_fail:
	release_mem_region(p,SZ_4K*16);
sys_fail:
	kobject_del(bootinfo_kobj);
	kobject_del(debug_info_kobj);
fail:
	return ret;

}


static void __exit bootinfo_exit(void)
{

	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}

	if(ptr)
		iounmap(ptr);
	
	release_mem_region(p,SZ_4K*16);
}

arch_initcall(bootinfo_init);
module_exit(bootinfo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Boot information collector");
