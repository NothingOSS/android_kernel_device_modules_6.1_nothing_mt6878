// SPDX-License-Identifier: GPL-2.0
/*
 * MTK xhci quirk driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Denis Hsu <denis.hsu@mediatek.com>
 */

#include <linux/usb/quirks.h>
#include <trace/hooks/audio_usboffload.h>
#include "quirks.h"
#include "xhci-mtk.h"
#include "xhci-trace.h"

struct usb_audio_quirk_flags_table {
	u32 id;
	u32 flags;
};

#define DEVICE_FLG(vid, pid, _flags) \
	{ .id = USB_ID(vid, pid), .flags = (_flags) }
#define VENDOR_FLG(vid, _flags) DEVICE_FLG(vid, 0, _flags)

/* quirk list in usbcore */
static const struct usb_device_id mtk_usb_quirk_list[] = {
	/* HUAWEI AM33/CM33 HeadSet */
	{USB_DEVICE(0x12d1, 0x3a07), .driver_info = USB_QUIRK_IGNORE_REMOTE_WAKEUP|USB_QUIRK_RESET},
	/* TTGK */
	{USB_DEVICE(0x2d13, 0xa001), .driver_info = USB_QUIRK_NO_LPM},

	{ }  /* terminating entry must be last */
};

/* quirk list in /sound/usb */
static const struct usb_audio_quirk_flags_table mtk_snd_quirk_flags_table[] = {
		DEVICE_FLG(0x2d99, 0xa026, /* EDIFIER H180 Plus */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x12d1, 0x3a07,	/* HUAWEI AM33/CM33 HeadSet */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x04e8, 0xa051,      /* SS USBC Headset (AKG) */
		   QUIRK_FLAG_CTL_MSG_DELAY),

		{} /* terminator */
};

static int usb_match_device(struct usb_device *dev, const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
		return 0;

	/* No need to test id->bcdDevice_lo != 0, since 0 is never
	   greater than any unsigned number. */
	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		(id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		(id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		(id->bDeviceClass != dev->descriptor.bDeviceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		(id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		(id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
		return 0;

	return 1;
}

static u32 usb_detect_static_quirks(struct usb_device *udev,
					const struct usb_device_id *id)
{
	u32 quirks = 0;

	for (; id->match_flags; id++) {
		if (!usb_match_device(udev, id))
			continue;

		quirks |= (u32)(id->driver_info);
		dev_info(&udev->dev,
			  "Set usbcore quirk_flags 0x%x for device %04x:%04x\n",
			  (u32)id->driver_info, id->idVendor,
			  id->idProduct);
	}

	return quirks;
}

static void snd_usb_init_quirk_flags(struct snd_usb_audio *chip)
{
	const struct usb_audio_quirk_flags_table *p;

	for (p = mtk_snd_quirk_flags_table; p->id; p++) {
		if (chip->usb_id == p->id ||
			(!USB_ID_PRODUCT(p->id) &&
			 USB_ID_VENDOR(chip->usb_id) == USB_ID_VENDOR(p->id))) {
			dev_info(&chip->dev->dev,
					  "Set audio quirk_flags 0x%x for device %04x:%04x\n",
					  p->flags, USB_ID_VENDOR(chip->usb_id),
					  USB_ID_PRODUCT(chip->usb_id));
			chip->quirk_flags |= p->flags;
			return;
		}
	}
}

/* update mtk usbcore quirk */
void xhci_mtk_apply_quirk(struct usb_device *udev)
{
	if (!udev)
		return;

	udev->quirks = usb_detect_static_quirks(udev, mtk_usb_quirk_list);
}

/* update mtk usb audio quirk */
void xhci_mtk_sound_usb_connect(void *unused, struct usb_interface *intf, struct snd_usb_audio *chip)
{
	if (!chip)
		return;

	snd_usb_init_quirk_flags(chip);
}

static void xhci_mtk_usb_asap_quirk(struct urb *urb)
{
	urb->transfer_flags |= URB_ISO_ASAP;
}

static void xhci_mtk_usb_set_interface_quirk(struct urb *urb)
{
	struct device *dev = &urb->dev->dev;
	struct usb_ctrlrequest *ctrl = NULL;
	struct usb_interface *iface = NULL;
	struct usb_host_interface *alt = NULL;

	ctrl = (struct usb_ctrlrequest *)urb->setup_packet;
	if (ctrl->bRequest != USB_REQ_SET_INTERFACE || ctrl->wValue == 0)
		return;

	iface = usb_ifnum_to_if(urb->dev, ctrl->wIndex);
	if (!iface)
		return;

	alt = usb_altnum_to_altsetting(iface, ctrl->wValue);
	if (!alt)
		return;

	if (alt->desc.bInterfaceClass != USB_CLASS_AUDIO)
		return;

	dev_dbg(dev, "delay 5ms for UAC device\n");
	mdelay(5);
}

static bool xhci_mtk_is_usb_audio(struct urb *urb)
{
	struct usb_host_config *config = NULL;
	struct usb_interface_descriptor *intf_desc = NULL;
	int config_num, i;

	config = urb->dev->config;
	if (!config)
		return false;
	config_num = urb->dev->descriptor.bNumConfigurations;

	for (i = 0; i < config_num; i++, config++) {
		if (config && config->desc.bNumInterfaces > 0)
			intf_desc = &config->intf_cache[0]->altsetting->desc;
		if (intf_desc && intf_desc->bInterfaceClass == USB_CLASS_AUDIO)
			return true;
	}

	return false;
}

static void xhci_trace_ep_urb_enqueue(void *data, struct urb *urb)
{
	u32 ep_type;

	if (!urb || !urb->dev)
		return;

	ep_type = usb_endpoint_type(&urb->ep->desc);

	if (ep_type == USB_ENDPOINT_XFER_CONTROL) {
		if (!urb->setup_packet)
			return;

		if (xhci_mtk_is_usb_audio(urb)) {
			/* apply set interface face delay */
			xhci_mtk_usb_set_interface_quirk(urb);
		}
	} else if (ep_type == USB_ENDPOINT_XFER_ISOC) {
		if (xhci_mtk_is_usb_audio(urb)) {
			/* add URB_ISO_ASAP flag */
			xhci_mtk_usb_asap_quirk(urb);
		}
	}
}

void xhci_mtk_trace_init(struct device *dev)
{
	WARN_ON(register_trace_xhci_urb_enqueue_(xhci_trace_ep_urb_enqueue, dev));
	WARN_ON(register_trace_android_vh_audio_usb_offload_connect(
		xhci_mtk_sound_usb_connect, NULL));
}

void xhci_mtk_trace_deinit(struct device *dev)
{
	WARN_ON(unregister_trace_xhci_urb_enqueue_(xhci_trace_ep_urb_enqueue, dev));
	WARN_ON(unregister_trace_android_vh_audio_usb_offload_connect(
		xhci_mtk_sound_usb_connect, NULL));
}
