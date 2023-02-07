// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: debug " fmt

/*
 * For IOMMU EP/bring up phase, you must be enable "IOMMU_BRING_UP".
 * If you need to do some special config, you can also use this macro.
 */
#define IOMMU_BRING_UP	(0)

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include <trace/hooks/iommu.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#include <aee.h>
#endif
#include "mtk_iommu.h"
#include "iommu_secure.h"
#include "iommu_debug.h"
#include "iommu_port.h"

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(12, 2)
#define F_MMU_INT_TF_CCU_MSK		GENMASK(12, 7)
#define F_MMU_INT_TF_LARB(id)		FIELD_GET(GENMASK(13, 7), id)
#define F_MMU_INT_TF_PORT(id)		FIELD_GET(GENMASK(6, 2), id)
#define F_APU_MMU_INT_TF_MSK(id)	FIELD_GET(GENMASK(11, 7), id)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)

#else
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)
#endif

#define MAU_CONFIG_INIT(iommu_type, iommu_id, slave, mau, start, end,\
	port_mask, larb_mask, wr, virt, io, start_bit32, end_bit32) {\
	iommu_type, iommu_id, slave, mau, start, end, port_mask, larb_mask,\
	wr, virt, io, start_bit32, end_bit32\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%llx,pa=0x%llx\n"

#define mau_assert_log_format \
	"CRDISPATCH_KEY:IOMMU\nMAU ASRT:ASRT_ID=0x%x,FALUT_ID=0x%x(%s),ADDR=0x%x(0x%x)\n"

#define FIND_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */
#define MAP_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */

struct mtk_iommu_cb {
	int port;
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct proc_dir_entry	*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

struct mtk_m4u_plat_data {
	struct peri_iommu_data		*peri_data;
	const struct mtk_iommu_port	*port_list[TYPE_NUM];
	u32				port_nr[TYPE_NUM];
	const struct mau_config_info	*mau_config;
	u32				mau_config_nr;
	u32				mm_tf_ccu_support;
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	char *(*peri_tf_analyse)(enum peri_iommu bus_id, u32 id);
};

struct peri_iommu_data {
	enum peri_iommu id;
	u32 bus_id;
};

static struct mtk_m4u_data *m4u_data;

/**********iommu trace**********/
#define IOMMU_EVENT_COUNT_MAX	(8000)

#define iommu_dump(file, fmt, args...) \
	do {\
		if (file)\
			seq_printf(file, fmt, ##args);\
		else\
			pr_info(fmt, ##args);\
	} while (0)

struct iommu_event_mgr_t {
	char name[11];
	unsigned int dump_trace;
	unsigned int dump_log;
};

static struct iommu_event_mgr_t event_mgr[IOMMU_EVENT_MAX];

struct iommu_event_t {
	unsigned int event_id;
	u64 time_high;
	u32 time_low;
	unsigned long data1;
	unsigned long data2;
	unsigned long data3;
	struct device *dev;
};

struct iommu_global_t {
	unsigned int enable;
	unsigned int dump_enable;
	unsigned int map_record;
	unsigned int start;
	unsigned int write_pointer;
	spinlock_t	lock;
	struct iommu_event_t *record;
};

static struct iommu_global_t iommu_globals;

/* iova statistics info for size and count */
#define IOVA_DUMP_TOP_MAX	(10)

struct iova_count_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	u64 size;
	u32 count;
	struct list_head list_node;
};

struct iova_count_list {
	spinlock_t		lock;
	struct list_head	head;
};

static struct iova_count_list count_list = {};

enum mtk_iova_space {
	MTK_IOVA_SPACE0, /* 0GB ~ 4GB */
	MTK_IOVA_SPACE1, /* 4GB ~ 8GB */
	MTK_IOVA_SPACE2, /* 8GB ~ 12GB */
	MTK_IOVA_SPACE3, /* 12GB ~ 16GB */
	MTK_IOVA_SPACE_NUM
};

/* iova alloc info */
struct iova_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	struct iova_domain *iovad;
	dma_addr_t iova;
	size_t size;
	u64 time_high;
	u32 time_low;
	struct list_head list_node;
};

struct iova_buf_list {
	atomic_t init_flag;
	struct list_head head;
	spinlock_t lock;
};

static struct iova_buf_list iova_list = {.init_flag = ATOMIC_INIT(0)};

/* iova map info */
struct iova_map_info {
	u64			tab_id;
	u64			iova;
	u64			time_high;
	u32			time_low;
	size_t			size;
	struct list_head	list_node;
};

struct iova_map_list {
	atomic_t		init_flag;
	spinlock_t		lock;
	struct list_head	head[MTK_IOVA_SPACE_NUM];
};

static struct iova_map_list map_list = {.init_flag = ATOMIC_INIT(0)};

static void mtk_iommu_iova_trace(int event, dma_addr_t iova, size_t size,
				 u64 tab_id, struct device *dev);
static void mtk_iommu_iova_alloc_dump_top(struct seq_file *s,
					  struct device *dev);
static void mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev);
static void mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id);

static void mtk_iommu_system_time(u64 *high, u32 *low)
{
	u64 temp;

	temp = sched_clock();
	do_div(temp, 1000);
	*low = do_div(temp, 1000000);
	*high = temp;
}

void mtk_iova_map(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *iova_buf;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.map_record == 0)
		goto iova_trace;

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->iova = iova;
	iova_buf->size = size;
	spin_lock_irqsave(&map_list.lock, flags);
	list_add(&iova_buf->list_node, &map_list.head[id]);
	spin_unlock_irqrestore(&map_list.lock, flags);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_MAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_map);

void mtk_iova_unmap(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	u64 start_t, end_t;
	unsigned long flags;
	struct iova_map_info *plist;
	struct iova_map_info *tmp_plist;
	int find_iova = 0;
	int i = 0;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.map_record == 0)
		goto iova_trace;

	spin_lock_irqsave(&map_list.lock, flags);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &map_list.head[id], list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->size == size &&
		    plist->tab_id == tab_id) {
			list_del(&plist->list_node);
			kfree(plist);
			find_iova = 1;
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock_irqrestore(&map_list.lock, flags);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info("%s warnning, find iova:0x%llx in %d timeout:%llu\n",
			__func__, iova, i, (end_t - start_t));

	if (!find_iova)
		pr_info("%s warnning, iova:0x%llx is not find in %d\n",
			__func__, iova, i);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_UNMAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_unmap);

/* For smmu, tab_id is smmu hardware id */
void mtk_iova_map_dump(u64 iova, u64 tab_id)
{
	mtk_iommu_iova_map_dump(NULL, iova, tab_id);
}
EXPORT_SYMBOL_GPL(mtk_iova_map_dump);

/* For smmu, tab_id is smmu hardware id */
static void mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id)
{
	u32 i, id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *plist = NULL;
	struct iova_map_info *n = NULL;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	iommu_dump(s, "iommu iova map dump:\n");
	iommu_dump(s, "%-6s %-14s %-10s %17s\n",
			"tab_id", "iova", "size", "time");

	spin_lock_irqsave(&map_list.lock, flags);
	if (!iova) {
		for (i = 0; i < MTK_IOVA_SPACE_NUM; i++) {
			list_for_each_entry_safe(plist, n, &map_list.head[i], list_node)
				if (plist->tab_id == tab_id)
					iommu_dump(s, "%-6llu 0x%-12llx 0x%-8zx %10llu.%06u\n",
						plist->tab_id, plist->iova,
						plist->size,
						plist->time_high,
						plist->time_low);
		}
		spin_unlock_irqrestore(&map_list.lock, flags);
		return;
	}

	list_for_each_entry_safe(plist, n, &map_list.head[id], list_node)
		if (plist->tab_id == tab_id &&
		    iova <= (plist->iova + plist->size) &&
		    iova >= (plist->iova))
			iommu_dump(s, "%-6llu 0x%-12llx 0x%-8zx %10llu.%06u\n",
				plist->tab_id, plist->iova,
				plist->size,
				plist->time_high,
				plist->time_low);
	spin_unlock_irqrestore(&map_list.lock, flags);
}

static void mtk_iommu_trace_dump(struct seq_file *s)
{
	int event_id;
	int i = 0;

	if (iommu_globals.dump_enable == 0)
		return;

	iommu_dump(s, "iommu trace dump:\n");
	iommu_dump(s, "%-8s %-4s %-14s %-12s %-14s %17s %s\n",
			"action", "tab_id", "iova_start", "size", "iova_end", "time", "dev");
	for (i = 0; i < IOMMU_EVENT_COUNT_MAX; i++) {
		unsigned long end_iova = 0;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;
		event_id = iommu_globals.record[i].event_id;
		if (event_id < 0 || event_id >= IOMMU_EVENT_MAX)
			continue;

		if (event_id <= IOMMU_UNSYNC)
			end_iova = iommu_globals.record[i].data1 +
				iommu_globals.record[i].data2 - 1;

		iommu_dump(s,
			"%-8s %-6lu 0x%-12lx 0x%-10zx 0x%-12lx %10llu.%06u %s\n",
			event_mgr[event_id].name,
			iommu_globals.record[i].data3,
			iommu_globals.record[i].data1,
			iommu_globals.record[i].data2,
			end_iova,
			iommu_globals.record[i].time_high,
			iommu_globals.record[i].time_low,
			(iommu_globals.record[i].dev != NULL ?
			dev_name(iommu_globals.record[i].dev) : ""));
	}
}

void mtk_iommu_debug_reset(void)
{
	iommu_globals.enable = 1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_debug_reset);

static int mtk_iommu_get_tf_port_idx(int tf_id,
	enum mtk_iommu_type type, int id)
{
	int i;
	u32 vld_id, port_nr;
	const struct mtk_iommu_port *port_list;
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return m4u_data->plat_data->port_nr[MM_IOMMU];
	}

	if (type == APU_IOMMU)
		vld_id = F_APU_MMU_INT_TF_MSK(tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	pr_info("get vld tf_id:0x%x\n", vld_id);
	port_nr =  m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	/* check (larb | port) for smi_larb or apu_bus */
	for (i = 0; i < port_nr; i++) {
		if (port_list[i].port_type == NORMAL &&
		    port_list[i].tf_id == vld_id &&
		    port_list[i].m4u_id == id)
			return i;
	}
	/* check larb for smi_common */
	if (type == MM_IOMMU && m4u_data->plat_data->mm_tf_ccu_support) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == CCU_FAKE &&
			    (port_list[i].tf_id & F_MMU_INT_TF_CCU_MSK) ==
			    (vld_id & F_MMU_INT_TF_CCU_MSK) &&
			    port_list[i].m4u_id == id)
				return i;
		}
	}

	/* check gce/video_uP */
	mm_tf_is_gce_videoup = m4u_data->plat_data->mm_tf_is_gce_videoup;
	if (type == MM_IOMMU && mm_tf_is_gce_videoup) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == GCE_VIDEOUP_FAKE &&
			    mm_tf_is_gce_videoup(port_list[i].tf_id, tf_id) &&
			    port_list[i].m4u_id == id)
				return i;
		}
	}

	return port_nr;
}

static int mtk_iommu_port_idx(int id, enum mtk_iommu_type type)
{
	int i;
	u32 port_nr = m4u_data->plat_data->port_nr[type];
	const struct mtk_iommu_port *port_list;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return m4u_data->plat_data->port_nr[MM_IOMMU];
	}

	port_list = m4u_data->plat_data->port_list[type];
	for (i = 0; i < port_nr; i++) {
		if ((port_list[i].larb_id == MTK_M4U_TO_LARB(id)) &&
		     (port_list[i].port_id == MTK_M4U_TO_PORT(id)))
			return i;
	}
	return port_nr;
}

static void report_custom_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, u32 type, int id)
{
	int idx;
	u32 port_nr;
	const struct mtk_iommu_port *port_list;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return;
	}

	pr_info("error, tf report start fault_id:0x%x\n", fault_id);
	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(fault_id, type, id);
	if (idx >= port_nr) {
		pr_warn("fail,iova:0x%llx, port:0x%x\n",
			fault_iova, fault_id);
		return;
	}

	/* Only MM_IOMMU support fault callback */
	if (type == MM_IOMMU) {
		pr_info("error, tf report larb-port:(%u--%u), idx:%d\n",
			port_list[idx].larb_id,
			port_list[idx].port_id, idx);

		if (port_list[idx].enable_tf &&
			m4u_data->m4u_cb[idx].fault_fn)
			m4u_data->m4u_cb[idx].fault_fn(m4u_data->m4u_cb[idx].port,
			fault_iova, m4u_data->m4u_cb[idx].fault_data);
	}

	m4u_aee_print(mmu_translation_log_format,
		port_list[idx].name,
		port_list[idx].name, fault_iova,
		fault_pa);
}

void report_custom_iommu_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, enum mtk_iommu_type type,
	int id)
{
	report_custom_fault(fault_iova, fault_pa, fault_id, type, id);
}
EXPORT_SYMBOL_GPL(report_custom_iommu_fault);

void report_iommu_mau_fault(
	u32 assert_id, u32 falut_id, char *port_name,
	u32 assert_addr, u32 assert_b32)
{
	m4u_aee_print(mau_assert_log_format,
		      assert_id, falut_id, port_name, assert_addr, assert_b32);
}
EXPORT_SYMBOL_GPL(report_iommu_mau_fault);

int mtk_iommu_register_fault_callback(int port,
	mtk_iommu_fault_callback_t fn,
	void *cb_data, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].port = port;
	m4u_data->m4u_cb[idx].fault_fn = fn;
	m4u_data->m4u_cb[idx].fault_data = cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_register_fault_callback);

int mtk_iommu_unregister_fault_callback(int port, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].port = -1;
	m4u_data->m4u_cb[idx].fault_fn = NULL;
	m4u_data->m4u_cb[idx].fault_data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_unregister_fault_callback);

char *mtk_iommu_get_port_name(enum mtk_iommu_type type, int id, int tf_id)
{
	const struct mtk_iommu_port *port_list;
	u32 port_nr;
	int idx;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_notice("%s fail, invalid type %d\n", __func__, type);
		return "m4u_port_unknown";
	}

	if (type == PERI_IOMMU)
		return peri_tf_analyse(id, tf_id);

	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(tf_id, type, id);
	if (idx >= port_nr) {
		pr_notice("%s err, iommu(%d,%d) tf_id:0x%x\n",
			  __func__, type, id, tf_id);
		return "m4u_port_unknown";
	}

	return port_list[idx].name;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_port_name);

const struct mau_config_info *mtk_iommu_get_mau_config(
	enum mtk_iommu_type type, int id,
	unsigned int slave, unsigned int mau)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	const struct mau_config_info *mau_config;
	int i;

	for (i = 0; i < m4u_data->plat_data->mau_config_nr; i++) {
		mau_config = &m4u_data->plat_data->mau_config[i];
		if (mau_config->iommu_type == type &&
		    mau_config->iommu_id == id &&
		    mau_config->slave == slave &&
		    mau_config->mau == mau)
			return mau_config;
	}
#endif

	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_mau_config);

/* peri_iommu */
static struct peri_iommu_data mt6983_peri_iommu_data[PERI_IOMMU_NUM] = {
	[PERI_IOMMU_M4] = {
		.id = PERI_IOMMU_M4,
		.bus_id = 4,
	},
	[PERI_IOMMU_M6] = {
		.id = PERI_IOMMU_M6,
		.bus_id = 6,
	},
	[PERI_IOMMU_M7] = {
		.id = PERI_IOMMU_M7,
		.bus_id = 7,
	},
};

static char *mt6983_peri_m7_id(u32 id)
{
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id4_2 = FIELD_GET(GENMASK(4, 2), id);

	if (id1_0 == 0)
		return "MCU_AP_M";
	else if (id1_0 == 1)
		return "DEBUG_TRACE_LOG";
	else if (id1_0 == 2)
		return "PERI2INFRA1_M";

	switch (id4_2) {
	case 0:
		return "CQ_DMA";
	case 1:
		return "DEBUGTOP";
	case 2:
		return "GPU_EB";
	case 3:
		return "CPUM_M";
	case 4:
		return "DXCC_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_m6_id(u32 id)
{
	return "PERI2INFRA0_M";
}

static char *mt6983_peri_m4_id(u32 id)
{
	u32 id0 = id & 0x1;
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id3_2 = FIELD_GET(GENMASK(3, 2), id);

	if (id0 == 0)
		return "DFD_M";
	else if (id1_0 == 1)
		return "DPMAIF_M";

	switch (id3_2) {
	case 0:
		return "ADSPSYS_M0_M";
	case 1:
		return "VLPSYS_M";
	case 2:
		return "CONN_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_tf(enum peri_iommu id, u32 fault_id)
{
	switch (id) {
	case PERI_IOMMU_M4:
		return mt6983_peri_m4_id(fault_id);
	case PERI_IOMMU_M6:
		return mt6983_peri_m6_id(fault_id);
	case PERI_IOMMU_M7:
		return mt6983_peri_m7_id(fault_id);
	default:
		return "UNKNOWN";
	}
}

enum peri_iommu get_peri_iommu_id(u32 bus_id)
{
	int i;

	for (i = PERI_IOMMU_M4; i < PERI_IOMMU_NUM; i++) {
		if (bus_id == m4u_data->plat_data->peri_data[i].bus_id)
			return i;
	}

	return PERI_IOMMU_NUM;
};
EXPORT_SYMBOL_GPL(get_peri_iommu_id);

char *peri_tf_analyse(enum peri_iommu iommu_id, u32 fault_id)
{
	if (m4u_data->plat_data->peri_tf_analyse)
		return m4u_data->plat_data->peri_tf_analyse(iommu_id, fault_id);

	pr_info("%s is not support\n", __func__);
	return NULL;
}
EXPORT_SYMBOL_GPL(peri_tf_analyse);

static int mtk_iommu_debug_help(struct seq_file *s)
{
	iommu_dump(s, "iommu debug file:\n");
	iommu_dump(s, "help: description debug file and command\n");
	iommu_dump(s, "debug: iommu main debug file, receive debug command\n");
	iommu_dump(s, "iommu_dump: iova trace dump file\n");
	iommu_dump(s, "iova_alloc: iova alloc list dump file\n");
	iommu_dump(s, "iova_map: iova map list dump file\n\n");

	iommu_dump(s, "iommu debug command:\n");
	iommu_dump(s, "echo 1 > /proc/iommu_debug/debug: iommu debug help\n");
	iommu_dump(s, "echo 2 > /proc/iommu_debug/debug: mm translation fault test\n");
	iommu_dump(s, "echo 3 > /proc/iommu_debug/debug: apu translation fault test\n");
	iommu_dump(s, "echo 4 > /proc/iommu_debug/debug: peri translation fault test\n");
	iommu_dump(s, "echo 5 > /proc/iommu_debug/debug: secure bank init\n");
	iommu_dump(s, "echo 6 > /proc/iommu_debug/debug: secure bank irq enable\n");
	iommu_dump(s, "echo 7 > /proc/iommu_debug/debug: secure bank backup\n");
	iommu_dump(s, "echo 8 > /proc/iommu_debug/debug: secure bank restore\n");
	iommu_dump(s, "echo 9 > /proc/iommu_debug/debug: secure switch enable\n");
	iommu_dump(s, "echo 10 > /proc/iommu_debug/debug: secure switch disable\n");
	iommu_dump(s, "echo 11 > /proc/iommu_debug/debug: enable trace log\n");
	iommu_dump(s, "echo 12 > /proc/iommu_debug/debug: disable trace log\n");
	iommu_dump(s, "echo 13 > /proc/iommu_debug/debug: enable trace dump\n");
	iommu_dump(s, "echo 14 > /proc/iommu_debug/debug: disable trace dump\n");
	iommu_dump(s, "echo 15 > /proc/iommu_debug/debug: reset to default trace log & dump\n");
	iommu_dump(s, "echo 16 > /proc/iommu_debug/debug: dump iova trace\n");
	iommu_dump(s, "echo 17 > /proc/iommu_debug/debug: dump iova alloc list\n");
	iommu_dump(s, "echo 18 > /proc/iommu_debug/debug: dump iova map list\n");
	iommu_dump(s, "echo 19 > /proc/iommu_debug/debug: dump bank base address\n");
	iommu_dump(s, "echo 20 > /proc/iommu_debug/debug: dump DISP_IOMMU bank0 value\n");
	iommu_dump(s, "echo 21 > /proc/iommu_debug/debug: dump DISP_IOMMU bank1 page table\n");

	return 0;
}

/* Notice: Please also update help info if debug command changes */
static int m4u_debug_set(void *data, u64 val)
{
	int ret = 0;

	pr_info("%s:val=%llu\n", __func__, val);

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	switch (val) {
	case 1:	/* show help info */
		ret = mtk_iommu_debug_help(NULL);
		break;
	case 2: /* mm translation fault test */
		report_custom_iommu_fault(0, 0, 0x500000f, MM_IOMMU, 0);
		break;
	case 3: /* apu translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, APU_IOMMU, 0);
		break;
	case 4: /* peri translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, PERI_IOMMU, 0);
		break;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	case 5:
		ret = mtk_iommu_sec_bk_init_by_atf(MM_IOMMU, DISP_IOMMU);
		break;
	case 6:
		ret = mtk_iommu_sec_bk_irq_en_by_atf(MM_IOMMU, DISP_IOMMU, 1);
		break;
	case 7:
		ret = mtk_iommu_secure_bk_backup_by_atf(MM_IOMMU, DISP_IOMMU);
		break;
	case 8:
		ret = mtk_iommu_secure_bk_restore_by_atf(MM_IOMMU, DISP_IOMMU);
		break;
	case 9:
		ret = ao_secure_dbg_switch_by_atf(MM_IOMMU, DISP_IOMMU, 1);
		break;
	case 10:
		ret = ao_secure_dbg_switch_by_atf(MM_IOMMU, DISP_IOMMU, 0);
		break;
#endif
	case 11:	/* enable trace log */
		event_mgr[IOMMU_ALLOC].dump_log = 1;
		event_mgr[IOMMU_FREE].dump_log = 1;
		event_mgr[IOMMU_MAP].dump_log = 1;
		event_mgr[IOMMU_UNMAP].dump_log = 1;
		break;
	case 12:	/* disable trace log */
		event_mgr[IOMMU_ALLOC].dump_log = 0;
		event_mgr[IOMMU_FREE].dump_log = 0;
		event_mgr[IOMMU_MAP].dump_log = 0;
		event_mgr[IOMMU_UNMAP].dump_log = 0;
		break;
	case 13:	/* enable trace dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 1;
		event_mgr[IOMMU_FREE].dump_trace = 1;
		event_mgr[IOMMU_MAP].dump_trace = 1;
		event_mgr[IOMMU_UNMAP].dump_trace = 1;
		event_mgr[IOMMU_SYNC].dump_trace = 1;
		event_mgr[IOMMU_UNSYNC].dump_trace = 1;
		break;
	case 14:	/* disable trace dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 0;
		event_mgr[IOMMU_FREE].dump_trace = 0;
		event_mgr[IOMMU_MAP].dump_trace = 0;
		event_mgr[IOMMU_UNMAP].dump_trace = 0;
		event_mgr[IOMMU_SYNC].dump_trace = 0;
		event_mgr[IOMMU_UNSYNC].dump_trace = 0;
		break;
	case 15:	/* reset to default trace log & dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 1;
		event_mgr[IOMMU_FREE].dump_trace = 1;
		event_mgr[IOMMU_SYNC].dump_trace = 1;
		event_mgr[IOMMU_UNSYNC].dump_trace = 1;
		event_mgr[IOMMU_MAP].dump_trace = 0;
		event_mgr[IOMMU_UNMAP].dump_trace = 0;
		event_mgr[IOMMU_ALLOC].dump_log = 0;
		event_mgr[IOMMU_FREE].dump_log = 0;
		event_mgr[IOMMU_MAP].dump_log = 0;
		event_mgr[IOMMU_UNMAP].dump_log = 0;
		break;
	case 16:	/* dump iova trace */
		mtk_iommu_trace_dump(NULL);
		break;
	case 17:	/* dump iova alloc list */
		mtk_iommu_iova_alloc_dump_top(NULL, NULL);
		mtk_iommu_iova_alloc_dump(NULL, NULL);
		break;
	case 18:	/* dump iova map list */
		mtk_iommu_iova_map_dump(NULL, 0, MM_TABLE);
		mtk_iommu_iova_map_dump(NULL, 0, APU_TABLE);
		break;
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	case 19:
		mtk_iommu_dump_bank_base();
		break;
	case 20:
		ret = mtk_iommu_dump_bk0_val(MM_IOMMU, DISP_IOMMU);
		break;
	case 21:	/* dump DISP_IOMMU bank1 pagetable */
		ret = mtk_iommu_sec_bk_pgtable_dump(MM_IOMMU, DISP_IOMMU,
				IOMMU_BK1, 0);
		break;
#endif
	default:
		pr_err("%s error,val=%llu\n", __func__, val);
		break;
	}
#endif

	if (ret)
		pr_info("%s failed:val=%llu, ret=%d\n", __func__, val, ret);

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_PROC_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_FOPS_RO(name)				\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			pde_data(inode));			\
	}							\
	static const struct proc_ops name = {			\
		.proc_open		= name ## _proc_open,	\
		.proc_read		= seq_read,		\
		.proc_lseek		= seq_lseek,		\
		.proc_release	= single_release,		\
	}

static int mtk_iommu_help_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_debug_help(s);
	return 0;
}

static int mtk_iommu_dump_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_trace_dump(s);
	mtk_iommu_iova_alloc_dump(s, NULL);
	mtk_iommu_iova_alloc_dump_top(s, NULL);
	return 0;
}

static int mtk_iommu_iova_alloc_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_iova_alloc_dump_top(s, NULL);
	mtk_iommu_iova_alloc_dump(s, NULL);
	return 0;
}

static int mtk_iommu_iova_map_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_iova_map_dump(s, 0, MM_TABLE);
	mtk_iommu_iova_map_dump(s, 0, APU_TABLE);
	return 0;
}

/* adb shell cat /proc/iommu_debug/xxx */
DEFINE_PROC_FOPS_RO(mtk_iommu_help_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_dump_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_alloc_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_map_fops);

static void mtk_iommu_trace_init(struct mtk_m4u_data *data)
{
	int total_size = IOMMU_EVENT_COUNT_MAX * sizeof(struct iommu_event_t);

	strncpy(event_mgr[IOMMU_ALLOC].name, "alloc", 10);
	strncpy(event_mgr[IOMMU_FREE].name, "free", 10);
	strncpy(event_mgr[IOMMU_MAP].name, "map", 10);
	strncpy(event_mgr[IOMMU_UNMAP].name, "unmap", 10);
	strncpy(event_mgr[IOMMU_SYNC].name, "sync", 10);
	strncpy(event_mgr[IOMMU_UNSYNC].name, "unsync", 10);
	strncpy(event_mgr[IOMMU_SUSPEND].name, "suspend", 10);
	strncpy(event_mgr[IOMMU_RESUME].name, "resume", 10);
	strncpy(event_mgr[IOMMU_POWER_ON].name, "pwr_on", 10);
	strncpy(event_mgr[IOMMU_POWER_OFF].name, "pwr_off", 10);
	event_mgr[IOMMU_ALLOC].dump_trace = 1;
	event_mgr[IOMMU_FREE].dump_trace = 1;
	event_mgr[IOMMU_SYNC].dump_trace = 1;
	event_mgr[IOMMU_UNSYNC].dump_trace = 1;
	event_mgr[IOMMU_SUSPEND].dump_trace = 1;
	event_mgr[IOMMU_RESUME].dump_trace = 1;
	event_mgr[IOMMU_POWER_ON].dump_trace = 1;
	event_mgr[IOMMU_POWER_OFF].dump_trace = 1;

	event_mgr[IOMMU_SUSPEND].dump_log = 1;
	event_mgr[IOMMU_RESUME].dump_log = 1;
	event_mgr[IOMMU_POWER_ON].dump_log = 1;
	event_mgr[IOMMU_POWER_OFF].dump_log = 1;

	iommu_globals.record = vmalloc(total_size);
	if (!iommu_globals.record) {
		iommu_globals.enable = 0;
		return;
	}

	memset(iommu_globals.record, 0, total_size);
	iommu_globals.enable = 1;
	iommu_globals.dump_enable = 1;
	iommu_globals.write_pointer = 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	iommu_globals.map_record = 1;
#else
	iommu_globals.map_record = 0;
#endif

	spin_lock_init(&iommu_globals.lock);
}

static void mtk_iommu_trace_rec_write(int event,
	unsigned long data1, unsigned long data2,
	unsigned long data3, struct device *dev)
{
	unsigned int index;
	struct iommu_event_t *p_event = NULL;
	unsigned long flags;

	if (iommu_globals.enable == 0)
		return;
	if ((event >= IOMMU_EVENT_MAX) ||
	    (event < 0))
		return;

	if (event_mgr[event].dump_log)
		pr_info("[trace] %s |0x%lx |%lx |0x%lx |%s\n",
			event_mgr[event].name,
			data3, data1, data2,
			(dev != NULL ? dev_name(dev) : ""));

	if (event_mgr[event].dump_trace == 0)
		return;

	index = (atomic_inc_return((atomic_t *)
			&(iommu_globals.write_pointer)) - 1)
	    % IOMMU_EVENT_COUNT_MAX;

	spin_lock_irqsave(&iommu_globals.lock, flags);

	p_event = (struct iommu_event_t *)
		&(iommu_globals.record[index]);
	mtk_iommu_system_time(&(p_event->time_high), &(p_event->time_low));
	p_event->event_id = event;
	p_event->data1 = data1;
	p_event->data2 = data2;
	p_event->data3 = data3;
	p_event->dev = dev;

	spin_unlock_irqrestore(&iommu_globals.lock, flags);
}

static void mtk_iommu_iova_trace(int event,
	dma_addr_t iova, size_t size,
	u64 tab_id, struct device *dev)
{
	u32 id = (iova >> 32);

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	mtk_iommu_trace_rec_write(event, (unsigned long) iova, size, tab_id, dev);
}

void mtk_iommu_tlb_sync_trace(u64 iova, size_t size, int iommu_ids)
{
	mtk_iommu_trace_rec_write(IOMMU_SYNC, (unsigned long) iova, size,
				  (unsigned long) iommu_ids, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iommu_tlb_sync_trace);

void mtk_iommu_pm_trace(int event, int iommu_id, int pd_sta,
	unsigned long flags, struct device *dev)
{
	mtk_iommu_trace_rec_write(event, (unsigned long) pd_sta, flags,
				  (unsigned long) iommu_id, dev);
}
EXPORT_SYMBOL_GPL(mtk_iommu_pm_trace);

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu_debug", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_err("failed to create debug dir\n");

	debug_file = proc_create_data("debug",
		S_IFREG | 0644, data->debug_root, &m4u_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to create debug file\n");

	debug_file = proc_create_data("help",
		S_IFREG | 0644, data->debug_root, &mtk_iommu_help_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create help file\n");

	debug_file = proc_create_data("iommu_dump",
		S_IFREG | 0644, data->debug_root, &mtk_iommu_dump_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iommu_dump file\n");

	debug_file = proc_create_data("iova_alloc",
		S_IFREG | 0644, data->debug_root, &mtk_iommu_iova_alloc_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_alloc file\n");

	debug_file = proc_create_data("iova_map",
		S_IFREG | 0644, data->debug_root, &mtk_iommu_iova_map_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_map file\n");

	mtk_iommu_trace_init(data);

	spin_lock_init(&iova_list.lock);
	INIT_LIST_HEAD(&iova_list.head);

	spin_lock_init(&map_list.lock);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE0]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE1]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE2]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE3]);

	spin_lock_init(&count_list.lock);
	INIT_LIST_HEAD(&count_list.head);

	return 0;
}

static int iova_size_cmp(void *priv, const struct list_head *a,
	const struct list_head *b)
{
	struct iova_count_info *ia, *ib;

	ia = list_entry(a, struct iova_count_info, list_node);
	ib = list_entry(b, struct iova_count_info, list_node);

	if (ia->size < ib->size)
		return 1;
	if (ia->size > ib->size)
		return -1;

	return 0;
}

static void mtk_iommu_clear_iova_size(void)
{
	struct iova_count_info *plist;
	struct iova_count_info *tmp_plist;

	list_for_each_entry_safe(plist, tmp_plist, &count_list.head, list_node) {
		list_del(&plist->list_node);
		kfree(plist);
	}
}

static void mtk_iommu_count_iova_size(
	struct device *dev, dma_addr_t iova, size_t size)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_count_info *plist = NULL;
	struct iova_count_info *n = NULL;
	struct iova_count_info *new_info;

	fwspec = dev_iommu_fwspec_get(dev);
	if (fwspec == NULL) {
		pr_notice("%s fail! dev:%s, fwspec is NULL\n",
			  __func__, dev_name(dev));
		return;
	}

	/* Add to iova_count_info if exist */
	spin_lock(&count_list.lock);
	list_for_each_entry_safe(plist, n, &count_list.head, list_node) {
		if (plist->dev == dev) {
			plist->count++;
			plist->size += (unsigned long) (size / 1024);
			spin_unlock(&count_list.lock);
			return;
		}
	}

	/* Create new iova_count_info if no exist */
	new_info = kzalloc(sizeof(*new_info), GFP_ATOMIC);
	if (!new_info) {
		spin_unlock(&count_list.lock);
		pr_notice("%s, alloc iova_count_info fail! dev:%s\n",
			  __func__, dev_name(dev));
		return;
	}

	new_info->tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	new_info->dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	new_info->dev = dev;
	new_info->size = (unsigned long) (size / 1024);
	new_info->count = 1;
	list_add_tail(&new_info->list_node, &count_list.head);
	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_dump_top(
	struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	struct iova_count_info *p_count_list = NULL;
	struct iova_count_info *n_count = NULL;
	int total_cnt = 0, dom_count = 0, i = 0;
	u64 size = 0, total_size = 0, dom_size = 0;
	u64 tab_id = 0;
	u32 dom_id = 0;

	/* check fwspec by device */
	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_notice("%s fail! dev:%s, fwspec is NULL\n",
				  __func__, dev_name(dev));
			return;
		}
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	}

	/* count iova size by device */
	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node) {
		size = (unsigned long) (plist->size / 1024);
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			mtk_iommu_count_iova_size(plist->dev, plist->iova, plist->size);
			dom_size += size;
			dom_count++;
		}
		total_size += size;
		total_cnt++;
	}
	spin_unlock(&iova_list.lock);

	spin_lock(&count_list.lock);
	/* sort count iova size by device */
	list_sort(NULL, &count_list.head, iova_size_cmp);

	/* dump top max user */
	iommu_dump(s, "iommu iova alloc total:(%d/%lluKB), dom:(%d/%lluKB,%llu,%d) top %d user:\n",
		   total_cnt, total_size, dom_count, dom_size, tab_id, dom_id, IOVA_DUMP_TOP_MAX);
	iommu_dump(s, "%6s %6s %8s %10s %3s\n", "tab_id", "dom_id", "count", "size", "dev");
	list_for_each_entry_safe(p_count_list, n_count, &count_list.head, list_node) {
		iommu_dump(s, "%6llu %6u %8u %8lluKB %s\n",
			   p_count_list->tab_id,
			   p_count_list->dom_id,
			   p_count_list->count,
			   p_count_list->size,
			   dev_name(p_count_list->dev));
		i++;
		if (i >= IOVA_DUMP_TOP_MAX)
			break;
	}

	/* clear count iova size */
	mtk_iommu_clear_iova_size();

	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_info("%s fail! dev:%s, fwspec is NULL\n",
				__func__, dev_name(dev));
			return;
		}
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	iommu_dump(s, "iommu iova alloc dump:\n");
	iommu_dump(s, "%6s %6s %-18s %-10s %17s %s\n",
		   "tab_id", "dom_id", "iova", "size", "time", "dev");

	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node)
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id))
			iommu_dump(s, "%6llu %6u %-18pa 0x%-8zx %10llu.%06u %s\n",
				   plist->tab_id,
				   plist->dom_id,
				   &plist->iova,
				   plist->size,
				   plist->time_high,
				   plist->time_low,
				   dev_name(plist->dev));
	spin_unlock(&iova_list.lock);
}

static void mtk_iova_dbg_alloc(struct device *dev,
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	struct iova_info *iova_buf;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (!fwspec) {
		pr_info("%s fail, dev(%s) is not iommu-dev\n",
			__func__, dev_name(dev));
		return;
	}

	tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
	dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);

	if (!iova) {
		pr_info("%s fail! dev:%s, size:0x%zx\n",
			__func__, dev_name(dev), size);

		if (dom_id > 0)
			mtk_iommu_iova_alloc_dump(NULL, dev);

		return mtk_iommu_iova_alloc_dump_top(NULL, dev);
	}

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->dom_id = dom_id;
	iova_buf->dev = dev;
	iova_buf->iovad = iovad;
	iova_buf->iova = iova;
	iova_buf->size = size;
	spin_lock(&iova_list.lock);
	list_add(&iova_buf->list_node, &iova_list.head);
	spin_unlock(&iova_list.lock);

	mtk_iommu_iova_trace(IOMMU_ALLOC, iova, size, tab_id, dev);
}

static void mtk_iova_dbg_free(
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	u64 start_t, end_t;
	struct iova_info *plist;
	struct iova_info *tmp_plist;
	struct device *dev = NULL;
	u64 tab_id = 0;
	int i = 0;

	spin_lock(&iova_list.lock);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &iova_list.head, list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->size == size &&
		    plist->iovad == iovad) {
			tab_id = plist->tab_id;
			dev = plist->dev;
			list_del(&plist->list_node);
			kfree(plist);
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock(&iova_list.lock);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info("%s warnning, dev:%s, find iova:0x%llx in %d timeout:%llu\n",
			__func__, (dev ? dev_name(dev) : "NULL"),
			iova, i, (end_t - start_t));

	if (dev == NULL)
		pr_info("%s warnning, iova:0x%llx is not find in %d\n",
			__func__, iova, i);

	mtk_iommu_iova_trace(IOMMU_FREE, iova, size, tab_id, dev);
}

/* all code inside alloc_iova_hook can't be scheduled! */
static void alloc_iova_hook(void *data,
	struct device *dev, struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_alloc(dev, iovad, iova, size);
}

/* all code inside free_iova_hook can't be scheduled! */
static void free_iova_hook(void *data,
	struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_free(iovad, iova, size);
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 total_port;
	int ret = 0;

	pr_info("%s start\n", __func__);
	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	total_port = m4u_data->plat_data->port_nr[MM_IOMMU] +
		     m4u_data->plat_data->port_nr[APU_IOMMU] +
		     m4u_data->plat_data->port_nr[PERI_IOMMU];
	m4u_data->m4u_cb = devm_kzalloc(dev, total_port *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);

	ret = register_trace_android_vh_iommu_iovad_alloc_iova(alloc_iova_hook,
							       "mtk_m4u_dbg_probe");
	pr_debug("add alloc iova hook %s\n", (ret ? "fail" : "pass"));
	ret = register_trace_android_vh_iommu_iovad_free_iova(free_iova_hook,
							      "mtk_m4u_dbg_probe");
	pr_debug("add free iova hook %s\n", (ret ? "fail" : "pass"));

	return 0;
}

static const struct mau_config_info mau_config_default[] = {
	/* Monitor each IOMMU input IOVA<4K and output PA=0 */
	MAU_CONFIG_INIT(0, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(0, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
};

static int mt6855_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6879_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 9), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6886_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6897_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6983_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 10), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);

}

static int mt6985_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static const struct mtk_m4u_plat_data mt6855_data = {
	.port_list[MM_IOMMU] = mm_port_mt6855,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6855),
	.mm_tf_is_gce_videoup = mt6855_tf_is_gce_videoup,
	.mm_tf_ccu_support = 0,
};

static const struct mtk_m4u_plat_data mt6983_data = {
	.port_list[MM_IOMMU] = mm_port_mt6983,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6983),
	.port_list[APU_IOMMU] = apu_port_mt6983,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6983),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6879_data = {
	.port_list[MM_IOMMU] = mm_port_mt6879,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6879),
	.port_list[APU_IOMMU] = apu_port_mt6879,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6879),
	.port_list[PERI_IOMMU] = peri_port_mt6879,
	.port_nr[PERI_IOMMU]   = ARRAY_SIZE(peri_port_mt6879),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6879_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6886_data = {
	.port_list[MM_IOMMU] = mm_port_mt6886,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6886),
	.port_list[APU_IOMMU] = apu_port_mt6886,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6886),
	.mm_tf_is_gce_videoup = mt6886_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6895_data = {
	.port_list[MM_IOMMU] = mm_port_mt6895,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6895),
	.port_list[APU_IOMMU] = apu_port_mt6895,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6895),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6897_data = {
	.port_list[MM_IOMMU] = mm_port_mt6897,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6897),
	.port_list[APU_IOMMU] = apu_port_mt6897,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6897),
	.mm_tf_is_gce_videoup = mt6897_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6985_data = {
	.port_list[MM_IOMMU] = mm_port_mt6985,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6985),
	.port_list[APU_IOMMU] = apu_port_mt6985,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6985),
	.mm_tf_is_gce_videoup = mt6985_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct of_device_id mtk_m4u_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6855-iommu-debug", .data = &mt6855_data},
	{ .compatible = "mediatek,mt6879-iommu-debug", .data = &mt6879_data},
	{ .compatible = "mediatek,mt6886-iommu-debug", .data = &mt6886_data},
	{ .compatible = "mediatek,mt6895-iommu-debug", .data = &mt6895_data},
	{ .compatible = "mediatek,mt6897-iommu-debug", .data = &mt6897_data},
	{ .compatible = "mediatek,mt6983-iommu-debug", .data = &mt6983_data},
	{ .compatible = "mediatek,mt6985-iommu-debug", .data = &mt6985_data},
	{},
};

static struct platform_driver mtk_m4u_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "mtk-m4u-debug",
		.of_match_table = of_match_ptr(mtk_m4u_dbg_of_ids),
	}
};

module_platform_driver(mtk_m4u_dbg_drv);
MODULE_LICENSE("GPL v2");
