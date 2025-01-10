/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __LINUX_RT_TCPCI_CORE_H
#define __LINUX_RT_TCPCI_CORE_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/alarmtimer.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <uapi/linux/sched/types.h>

#include "tcpm.h"
#include "tcpci_timer.h"
#include "tcpci_config.h"

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "pd_core.h"
#if CONFIG_USB_PD_WAIT_BC12
#include <linux/power_supply.h>
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif

/* The switch of log message */
#define TYPEC_INFO_ENABLE	1
#define PE_EVENT_DBG_ENABLE	1
#define PE_STATE_INFO_ENABLE	1
#define TCPC_INFO_ENABLE	1
#define TCPC_TIMER_DBG_ENABLE	0
#define PE_INFO_ENABLE		1
#define TCPC_DBG_ENABLE		0
#define TCPC_DBG2_ENABLE	0
#define DPM_INFO_ENABLE		1
#define DPM_INFO2_ENABLE	1
#define DPM_DBG_ENABLE		0
#define PD_ERR_ENABLE		1
#define PE_DBG_ENABLE		0
#define TYPEC_DBG_ENABLE	0


#define DP_INFO_ENABLE		1
#define DP_DBG_ENABLE		1

#define UVDM_INFO_ENABLE	1
#define TCPM_DBG_ENABLE		1

#define TCPC_ENABLE_ANYMSG	\
		((TCPC_DBG_ENABLE)|(TCPC_DBG2_ENABLE)|\
		(DPM_DBG_ENABLE)|\
		(PD_ERR_ENABLE)|(PE_INFO_ENABLE)|\
		(PE_DBG_ENABLE)|(PE_EVENT_DBG_ENABLE)|\
		(PE_STATE_INFO_ENABLE)|(TCPC_INFO_ENABLE)|\
		(TCPC_TIMER_DBG_ENABLE)|(TYPEC_DBG_ENABLE)|\
		(TYPEC_INFO_ENABLE)|\
		(DP_INFO_ENABLE)|(DP_DBG_ENABLE)|\
		(UVDM_INFO_ENABLE)|(TCPM_DBG_ENABLE))

/* Disable VDM DBG Msg */
#define PE_STATE_INFO_VDM_DIS	0
#define PE_EVT_INFO_VDM_DIS	0
#define PE_DBG_RESET_VDM_DIS	1

/* sender response timer will sub delta between transmit & tx_success */
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#define PD_DYNAMIC_SENDER_RESPONSE	1
#else
#define PD_DYNAMIC_SENDER_RESPONSE	0
#endif /* CONFIG_USB_POWER_DELIVERY */

#define PD_BUG_ON(x)	WARN_ON(x)

struct tcpc_device;

struct tcpc_desc {
	uint8_t role_def;
	uint8_t rp_lvl;
	uint8_t vconn_supply;
	const char *name;
	bool en_wd;
	bool en_wd_dual_port;
	bool en_ctd;
	bool en_fod;
	bool en_typec_otp;
	bool en_floatgnd;
	bool en_vbus_short_cc;
	u32 wd_sbu_calib_init;
	u32 wd_sbu_pl_bound;
	u32 wd_sbu_pl_lbound_c2c;
	u32 wd_sbu_pl_ubound_c2c;
	u32 wd_sbu_ph_auddev;
	u32 wd_sbu_ph_lbound;
	u32 wd_sbu_ph_lbound1_c2c;
	u32 wd_sbu_ph_ubound1_c2c;
	u32 wd_sbu_ph_ubound2_c2c;
	u32 wd_sbu_aud_ubound;
};

/*---------------------------------------------------------------------------*/

#if CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK
#define CONFIG_TYPEC_NOTIFY_ATTACHWAIT 1
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */

#if CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC
#undef CONFIG_TYPEC_NOTIFY_ATTACHWAIT
#define CONFIG_TYPEC_NOTIFY_ATTACHWAIT 1
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */


#if CONFIG_TCPC_FORCE_DISCHARGE_EXT
#define CONFIG_TCPC_EXT_DISCHARGE 1
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */
/*---------------------------------------------------------------------------*/

/* TCPC Power Register Define */
#define TCPC_REG_POWER_STATUS_EXT_VSAFE0V	(1<<15)	/* extend */
#define TCPC_REG_POWER_STATUS_VBUS_PRES		(1<<2)

/* TCPC Alert Register Define */
#define TCPC_REG_ALERT_EXT_VBUS_80		(1<<(16+1))
#define TCPC_REG_ALERT_EXT_WAKEUP		(1<<(16+0))

#define TCPC_REG_ALERT_VBUS_DISCNCT (1<<11)
#define TCPC_REG_ALERT_RX_BUF_OVF   (1<<10)
#define TCPC_REG_ALERT_FAULT        (1<<9)
#define TCPC_REG_ALERT_V_ALARM_LO   (1<<8)
#define TCPC_REG_ALERT_V_ALARM_HI   (1<<7)
#define TCPC_REG_ALERT_TX_SUCCESS   (1<<6)
#define TCPC_REG_ALERT_TX_DISCARDED (1<<5)
#define TCPC_REG_ALERT_TX_FAILED    (1<<4)
#define TCPC_REG_ALERT_RX_HARD_RST  (1<<3)
#define TCPC_REG_ALERT_RX_STATUS    (1<<2)
#define TCPC_REG_ALERT_POWER_STATUS (1<<1)
#define TCPC_REG_ALERT_CC_STATUS    (1<<0)

#define TCPC_REG_ALERT_RX_MASK	\
	(TCPC_REG_ALERT_RX_STATUS | TCPC_REG_ALERT_RX_BUF_OVF)

#define TCPC_REG_ALERT_RX_ALL_MASK	\
	(TCPC_REG_ALERT_RX_MASK | TCPC_REG_ALERT_RX_HARD_RST)

#define TCPC_REG_ALERT_HRESET_SUCCESS	\
	(TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED)

#define TCPC_REG_ALERT_TX_MASK (TCPC_REG_ALERT_TX_SUCCESS | \
	TCPC_REG_ALERT_TX_FAILED | TCPC_REG_ALERT_TX_DISCARDED)

/* TCPC Behavior Flags */
#define TCPC_FLAGS_RETRY_CRC_DISCARD		(1<<0)
#define TCPC_FLAGS_PD_REV30			(1<<2)
#define TCPC_FLAGS_WATER_DETECTION		(1<<4)
#define TCPC_FLAGS_CABLE_TYPE_DETECTION		(1<<5)
#define TCPC_FLAGS_VCONN_SAFE5V_ONLY		(1<<6)
#define TCPC_FLAGS_ALERT_V10                    (1<<7)
#define TCPC_FLAGS_FOREIGN_OBJECT_DETECTION	(1<<8)
#define TCPC_FLAGS_TYPEC_OTP			(1<<9)
#define TCPC_FLAGS_FLOATING_GROUND		(1<<10)
#define TCPC_FLAGS_WD_DUAL_PORT			(1<<11)
#define TCPC_FLAGS_VBUS_SHORT_CC		(1<<12)

#define TYPEC_CC_PULL(rp_lvl, res)	((rp_lvl & 0x03) << 3 | (res & 0x07))

enum tcpc_rp_lvl {
	TYPEC_RP_DFT,
	TYPEC_RP_1_5,
	TYPEC_RP_3_0,
	TYPEC_RP_RSV,
};

enum tcpc_cc_pull {
	TYPEC_CC_RA = 0,
	TYPEC_CC_RP = 1,
	TYPEC_CC_RD = 2,
	TYPEC_CC_OPEN = 3,
	TYPEC_CC_DRP = 4,		/* from Rd */

	TYPEC_CC_RP_DFT = TYPEC_CC_PULL(TYPEC_RP_DFT, TYPEC_CC_RP),
	TYPEC_CC_RP_1_5 = TYPEC_CC_PULL(TYPEC_RP_1_5, TYPEC_CC_RP),
	TYPEC_CC_RP_3_0 = TYPEC_CC_PULL(TYPEC_RP_3_0, TYPEC_CC_RP),

	TYPEC_CC_DRP_DFT = TYPEC_CC_PULL(TYPEC_RP_DFT, TYPEC_CC_DRP),
	TYPEC_CC_DRP_1_5 = TYPEC_CC_PULL(TYPEC_RP_1_5, TYPEC_CC_DRP),
	TYPEC_CC_DRP_3_0 = TYPEC_CC_PULL(TYPEC_RP_3_0, TYPEC_CC_DRP),
};

#define TYPEC_CC_PULL_GET_RES(pull)	(pull & 0x07)
#define TYPEC_CC_PULL_GET_RP_LVL(pull)	((pull & 0x18) >> 3)

enum tcpm_rx_cap_type {
	TCPC_RX_CAP_SOP = 1 << 0,
	TCPC_RX_CAP_SOP_PRIME = 1 << 1,
	TCPC_RX_CAP_SOP_PRIME_PRIME = 1 << 2,
	TCPC_RX_CAP_SOP_DEBUG_PRIME = 1 << 3,
	TCPC_RX_CAP_SOP_DEBUG_PRIME_PRIME = 1 << 4,
	TCPC_RX_CAP_HARD_RESET = 1 << 5,
	TCPC_RX_CAP_CABLE_RESET = 1 << 6,
};

struct tcpc_ops {
	int (*init)(struct tcpc_device *tcpc, bool sw_reset);
	int (*init_alert_mask)(struct tcpc_device *tcpc);
	int (*alert_status_clear)(struct tcpc_device *tcpc, uint32_t mask);
	int (*fault_status_clear)(struct tcpc_device *tcpc, uint8_t status);
	int (*set_alert_mask)(struct tcpc_device *tcpc, uint32_t mask);
	int (*get_alert_mask)(struct tcpc_device *tcpc, uint32_t *mask);
	int (*get_alert_status)(struct tcpc_device *tcpc, uint32_t *alert);
	int (*get_power_status)(struct tcpc_device *tcpc, uint16_t *pwr_status);
	int (*get_fault_status)(struct tcpc_device *tcpc, uint8_t *status);
	int (*get_cc)(struct tcpc_device *tcpc, int *cc1, int *cc2);
	int (*set_cc)(struct tcpc_device *tcpc, int pull);
	int (*set_polarity)(struct tcpc_device *tcpc, int polarity);
	int (*set_vconn)(struct tcpc_device *tcpc, int enable);
	int (*deinit)(struct tcpc_device *tcpc);
	int (*alert_vendor_defined_handler)(struct tcpc_device *tcpc);
	int (*set_auto_dischg_discnt)(struct tcpc_device *tcpc, bool en);
	int (*get_vbus_voltage)(struct tcpc_device *tcpc, u32 *vbus);

	int (*is_vsafe0v)(struct tcpc_device *tcpc);

#if CONFIG_WATER_DETECTION
	int (*set_water_protection)(struct tcpc_device *tcpc, bool en);
#endif /* CONFIG_WATER_DETECTION */
	int (*set_cc_hidet)(struct tcpc_device *tcpc, bool en);
	int (*get_cc_hi)(struct tcpc_device *tcpc);

	int (*set_vbus_short_cc_en)(struct tcpc_device *tcpc, bool cc1, bool cc2);

	int (*set_low_power_mode)(struct tcpc_device *tcpc, bool en, int pull);

#if CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#if CONFIG_TCPC_AUTO_DISCHARGE_IC
	int (*set_auto_discharge)(struct tcpc_device *tcpc, bool en);
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
	int (*set_force_discharge)(struct tcpc_device *tcpc, bool en, int mv);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	int (*set_msg_header)(struct tcpc_device *tcpc,
			uint8_t power_role, uint8_t data_role);
	int (*set_rx_enable)(struct tcpc_device *tcpc, uint8_t enable);
	int (*get_message)(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *head, enum tcpm_transmit_type *type);
	int (*protocol_reset)(struct tcpc_device *tcpc);
	int (*transmit)(struct tcpc_device *tcpc,
			enum tcpm_transmit_type type,
			uint16_t header, const uint32_t *data);
	int (*set_bist_test_mode)(struct tcpc_device *tcpc, bool en);
	int (*set_bist_carrier_mode)(struct tcpc_device *tcpc, uint8_t pattern);

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	int (*retransmit)(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#endif	/* CONFIG_USB_POWER_DELIVERY */
};

#define TCPC_VBUS_SOURCE_0V		(0)
#define TCPC_VBUS_SOURCE_5V		(5000)

#define TCPC_VBUS_SINK_0V		(0)
#define TCPC_VBUS_SINK_5V		(5000)

struct tcpc_managed_res;

struct tcpc_timer {
	struct alarm alarm;
	struct tcpc_device *tcpc;
};

/*
 * tcpc device
 */

struct tcpc_device {
	struct tcpc_ops *ops;
	void *drv_data;
	struct tcpc_desc desc;
	struct device dev;
	uint8_t wake_lock_pd;
	struct wakeup_source *attach_wake_lock;
	struct wakeup_source *detach_wake_lock;

	/* time test */
#if PD_DYNAMIC_SENDER_RESPONSE
	u64 t[2];
	u64 tx_time_diff;
#endif /* PD_DYNAMIC_SENDER_RESPONSE */

	struct tcpc_timer tcpc_timer[PD_TIMER_NR];

	uint32_t typec_lpm_tout;

	struct mutex access_lock;
	struct mutex typec_lock;
	struct mutex timer_lock;
	spinlock_t timer_tick_lock;
	atomic_t pending_event;
	atomic_t suspend_pending;
	uint64_t timer_tick;
	wait_queue_head_t event_wait_que;
	wait_queue_head_t timer_wait_que;
	struct task_struct *event_task;
	struct task_struct *timer_task;

	struct delayed_work event_init_work;
	struct workqueue_struct *evt_wq;
	struct srcu_notifier_head evt_nh[TCP_NOTIFY_IDX_NR];
	struct tcpc_managed_res *mr_head;
	struct mutex mr_lock;

	/* For TCPC TypeC */
	uint8_t typec_state;
	uint8_t typec_role;
	uint8_t typec_role_new;
	uint8_t typec_attach_old;
	uint8_t typec_attach_new;
	uint8_t typec_local_cc;
	uint8_t typec_local_rp_level;
	uint8_t typec_remote_cc[2];
	uint8_t typec_remote_rp_level;
	uint8_t typec_wait_ps_change;
	bool typec_polarity;
	bool typec_drp_try_timeout;
	bool typec_lpm;
	bool typec_power_ctrl;

	int typec_usb_sink_curr;

#if CONFIG_TYPEC_CAP_ROLE_SWAP
	uint8_t typec_during_role_swap;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

#if CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#if CONFIG_TCPC_AUTO_DISCHARGE_IC
	bool typec_auto_discharge;
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
	bool typec_force_discharge;
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

#if CONFIG_TCPC_EXT_DISCHARGE
	bool typec_ext_discharge;
#endif	/* CONFIG_TCPC_EXT_DISCHARGE */

#if CONFIG_TCPC_VCONN_SUPPLY_MODE
	uint8_t tcpc_vconn_supply;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#if CONFIG_TCPC_SOURCE_VCONN
	bool tcpc_source_vconn;
#endif	/* CONFIG_TCPC_SOURCE_VCONN */

	uint32_t tcpc_flags;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Event */
	uint8_t pd_event_count;
	uint8_t pd_event_head_index;
	uint8_t pd_msg_buffer_allocated;

	bool pd_pending_vdm_event;
	bool pd_pending_vdm_reset;
	bool pd_pending_vdm_good_crc;
	bool pd_pending_vdm_attention;
	bool pd_postpone_vdm_timeout;

	struct pd_msg pd_attention_vdm_msg;
	struct pd_event pd_vdm_event;

	struct pd_msg pd_msg_buffer[PD_MSG_BUF_SIZE];
	struct pd_event pd_event_ring_buffer[PD_EVENT_BUF_SIZE];

	uint8_t tcp_event_count;
	uint8_t tcp_event_head_index;
	struct tcp_dpm_event tcp_event_ring_buffer[TCP_EVENT_BUF_SIZE];

	bool pd_pe_running;
	bool pd_wait_pe_idle;
	bool pd_hard_reset_event_pending;
	bool pd_wait_hard_reset_complete;
	bool pd_wait_pr_swap_complete;
	bool pd_ping_event_pending;
	uint8_t pd_bist_mode;
	uint8_t pd_transmit_state;
	uint8_t pd_wait_vbus_once;

#if CONFIG_USB_PD_DIRECT_CHARGE
	bool pd_during_direct_charge;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
	bool pd_exit_attached_snk_via_cc;
#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	bool pd_discard_pending;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	uint8_t pd_retry_count;

#if CONFIG_USB_PD_DISABLE_PE
	bool disable_pe; /* typec only */
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	struct pd_port pd_port;
#if CONFIG_USB_PD_REV30
	struct notifier_block bat_nb;
	struct delayed_work bat_update_work;
	struct power_supply *bat_psy;
	uint8_t charging_status;
	int bat_soc;
#endif /* CONFIG_USB_PD_REV30 */
#if CONFIG_USB_PD_WAIT_BC12
	uint8_t pd_wait_bc12_count;
	struct power_supply *chg_psy;
#endif /* CONFIG_USB_PD_WAIT_BC12 */
	wait_queue_head_t tx_wait_que;
	atomic_t tx_pending;
	u64 tx_jiffies;
	u64 tx_jiffies_max;
	struct delayed_work tx_pending_work;
#endif /* CONFIG_USB_POWER_DELIVERY */
	u8 vbus_level:2;
	bool vbus_safe0v;
	bool vbus_present;
	u8 pd_inited_flag:1; /* MTK Only */

	/* TypeC Shield Protection */
	enum tcpc_fod_status typec_fod;
	int bootmode;
#if CONFIG_CABLE_TYPE_DETECTION
	enum tcpc_cable_type typec_cable_type;
#endif /* CONFIG_CABLE_TYPE_DETECTION */
	bool typec_otp;
	bool typec_vbus_to_cc_en;
	bool cc_hidet_en;
	int cc_hi;
};

#define to_tcpc_device(obj) container_of(obj, struct tcpc_device, dev)

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static inline uint8_t pd_get_rev(struct pd_port *pd_port, uint8_t sop_type)
{
	uint8_t pd_rev = PD_REV20;
#if CONFIG_USB_PD_REV30_SYNC_SPEC_REV
	struct pe_data *pe_data = &pd_port->pe_data;
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (sop_type == TCPC_TX_SOP) {
		pd_rev = pd_port->pd_revision[0];
	} else {
		if (pe_data->explicit_contract || pe_data->cable_rev_discovered)
			pd_rev = pd_port->pd_revision[1];
		else if (tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
			pd_rev = PD_REV30;
	}
#endif	/* CONFIG_USB_PD_REV30_SYNC_SPEC_REV */

	return pd_rev;
}

static inline bool pd_check_rev30(struct pd_port *pd_port)
{
	return pd_get_rev(pd_port, TCPC_TX_SOP) >= PD_REV30;
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_PD_DBG_INFO)
#define __RT_DBG_INFO	pd_dbg_info
#else
#define __RT_DBG_INFO	pr_info
#endif /* CONFIG_PD_DBG_INFO */

#if CONFIG_TCPC_LOG_WITH_PORT_NAME
#define RT_DBG_INFO(format, args...)	__RT_DBG_INFO(format,	\
						      tcpc->desc.name, ##args)
#else
#define RT_DBG_INFO(format, args...)	__RT_DBG_INFO(format, ##args)
#endif /* CONFIG_TCPC_LOG_WITH_PORT_NAME */

#if TYPEC_DBG_ENABLE
#define TYPEC_DBG(format, args...)		\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TYPEC:" format, ##args)
#else
#define TYPEC_DBG(format, args...)
#endif /* TYPEC_DBG_ENABLE */

#if TYPEC_INFO_ENABLE
#define TYPEC_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TYPEC:" format, ##args)
#else
#define TYPEC_INFO(format, args...)
#endif /* TYPEC_INFO_ENABLE */

#if TCPC_INFO_ENABLE
#define TCPC_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_INFO(foramt, args...)
#endif /* TCPC_INFO_ENABLE */

#if TCPC_DBG_ENABLE
#define TCPC_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_DBG(format, args...)
#endif /* TCPC_DBG_ENABLE */

#if TCPC_DBG2_ENABLE
#define TCPC_DBG2(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_DBG2(format, args...)
#endif /* TCPC_DBG2_ENABLE */

#if TCPC_TIMER_DBG_ENABLE
#define TCPC_TIMER_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TIMER:" format, ##args)
#else
#define TCPC_TIMER_DBG(format, args...)
#endif /* TCPC_TIMER_DBG_ENABLE */

#define TCPC_ERR(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC-ERR:" format, ##args)

#define DP_ERR(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP-ERR:" format, ##args)

#if DPM_INFO_ENABLE
#define DPM_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_INFO(format, args...)
#endif /* DPM_INFO_ENABLE */

#if DPM_INFO2_ENABLE
#define DPM_INFO2(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_INFO2(format, args...)
#endif /* DPM_INFO2_ENABLE */

#if DPM_DBG_ENABLE
#define DPM_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_DBG(format, args...)
#endif /* DPM_DBG_ENABLE */

#if PD_ERR_ENABLE
#define PD_ERR(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PD-ERR:" format, ##args)
#else
#define PD_ERR(format, args...)
#endif /* PD_ERR_ENABLE */

#if PE_INFO_ENABLE
#define PE_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_INFO(format, args...)
#endif /* PE_INFO_ENABLE */

#if PE_EVENT_DBG_ENABLE
#define PE_EVT_INFO(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE-EVT:" format, ##args)
#else
#define PE_EVT_INFO(format, args...)
#endif /* PE_EVENT_DBG_ENABLE */

#if PE_DBG_ENABLE
#define PE_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_DBG(format, args...)
#endif /* PE_DBG_ENABLE */

#if PE_STATE_INFO_ENABLE
#define PE_STATE_INFO(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_STATE_INFO(format, args...)
#endif /* PE_STATE_IFNO_ENABLE */

#if DP_INFO_ENABLE
#define DP_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP:" format, ##args)
#else
#define DP_INFO(format, args...)
#endif /* DP_INFO_ENABLE */

#if DP_DBG_ENABLE
#define DP_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP:" format, ##args)
#else
#define DP_DBG(format, args...)
#endif /* DP_DBG_ENABLE */

#if UVDM_INFO_ENABLE
#define UVDM_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "UVDM:" format, ##args)
#else
#define UVDM_INFO(format, args...)
#endif

#if TCPM_DBG_ENABLE
#define TCPM_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPM:" format, ##args)
#else
#define TCPM_DBG(format, args...)
#endif

void sched_set_fifo(struct task_struct *p);
#endif /* #ifndef __LINUX_RT_TCPCI_CORE_H */
