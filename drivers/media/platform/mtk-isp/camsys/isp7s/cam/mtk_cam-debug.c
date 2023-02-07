// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/proc_fs.h>

#include "mtk_cam.h"
#include "mtk_cam-debug.h"
#include "mtk_cam-debug_dump_header.h"

static int mtk_cam_debug_exp_open(struct inode *inode, struct file *file)
{
	struct mtk_cam_exception *exp = pde_data(inode);

	if (WARN_ON(!exp))
		return -EFAULT;

	file->private_data = exp;
	return 0;
}

static ssize_t mtk_cam_debug_exp_read(struct file *file, char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct mtk_cam_exception *exp = file->private_data;
	size_t read_count;

	if (WARN_ON(!exp))
		return 0;

	if (!atomic_read(&exp->ready)) {
		pr_info("%s: exp dump is not ready\n", __func__);
		return 0;
	}

	pr_debug("%s: read buf request: %zu bytes\n", __func__, count);

	mutex_lock(&exp->lock);
	read_count = simple_read_from_buffer(user_buf, count, ppos,
					     exp->buf,
					     exp->buf_size);
	mutex_unlock(&exp->lock);

	return read_count;
}

static int mtk_cam_debug_exp_release(struct inode *inode, struct file *file)
{
	struct mtk_cam_exception *exp = file->private_data;

	if (WARN_ON(!exp))
		return 0;

	atomic_set(&exp->ready, 0);
	pr_info("%s: reset exp dump ready\n", __func__);
	return 0;
}

static const struct proc_ops exp_fops = {
	.proc_open	= mtk_cam_debug_exp_open,
	.proc_read	= mtk_cam_debug_exp_read,
	.proc_release	= mtk_cam_debug_exp_release,
};

static int debug_exception_dump_init(struct mtk_cam_exception *exp)
{
	struct mtk_cam_debug *dbg =
		container_of(exp, struct mtk_cam_debug, exp);

	/* proc file system */
	exp->dump_entry = proc_create_data("mtk_cam_exp_dump", 0644, NULL,
					   &exp_fops, exp);
	if (!exp->dump_entry) {
		dev_info(dbg->cam->dev, "Can't create proc fs\n");
		return -ENOMEM;
	}

	atomic_set(&exp->ready, 0);
	mutex_init(&exp->lock);
	exp->buf_size = 0;
	exp->buf = NULL;
	return 0;
}

static void debug_exception_dump_deinit(struct mtk_cam_exception *exp)
{
	/* TODO: free if all users exit */
	if (exp->buf)
		vfree(exp->buf);

	if (exp->dump_entry)
		proc_remove(exp->dump_entry);
}

int mtk_cam_debug_init(struct mtk_cam_debug *dbg, struct mtk_cam_device *cam)
{
	int ret;

	memset(dbg, 0, sizeof(*dbg));

	dbg->cam = cam;

	ret = debug_exception_dump_init(&dbg->exp);
	if (ret)
		return ret;

	dev_info(cam->dev, "[%s] success\n", __func__);
	return ret;
}

void mtk_cam_debug_deinit(struct mtk_cam_debug *dbg)
{
	debug_exception_dump_deinit(&dbg->exp);
}

static size_t required_buffer_size(struct mtk_cam_dump_param *p)
{
	return sizeof(struct mtk_cam_dump_header)
		+ p->cq_size
		+ p->meta_in_dump_buf_size
		+ p->meta_out_0_dump_buf_size
		+ p->meta_out_1_dump_buf_size
		+ p->frame_param_size
		+ p->config_param_size;
}

static int mtk_cam_write_header(struct mtk_cam_dump_param *p,
				struct mtk_cam_dump_header *hdr,
				size_t buf_size)
{
	strncpy(hdr->desc, p->desc, sizeof(hdr->desc) - 1);

	hdr->request_fd = p->request_fd;
	hdr->stream_id = p->stream_id;
	hdr->timestamp = p->timestamp;
	hdr->sequence = p->sequence;
	hdr->header_size = sizeof(*hdr);
	hdr->payload_offset = hdr->header_size;
	hdr->payload_size = buf_size - hdr->header_size;

	hdr->meta_version_major = GET_PLAT_V4L2(meta_major);
	hdr->meta_version_minor = GET_PLAT_V4L2(meta_minor);

	/* CQ dump */
	hdr->cq_dump_buf_offset = hdr->payload_offset;
	hdr->cq_size = p->cq_size;
	hdr->cq_iova = p->cq_iova;
	hdr->cq_desc_offset = p->cq_desc_offset;
	hdr->cq_desc_size = p->cq_desc_size;
	hdr->sub_cq_desc_offset = p->sub_cq_desc_offset;
	hdr->sub_cq_desc_size = p->sub_cq_desc_size;

	/* meta in */
	hdr->meta_in_dump_buf_offset = hdr->cq_dump_buf_offset +
		hdr->cq_size;
	hdr->meta_in_dump_buf_size = p->meta_in_dump_buf_size;
	hdr->meta_in_iova = p->meta_in_iova;

	/* meta out 0 */
	hdr->meta_out_0_dump_buf_offset = hdr->meta_in_dump_buf_offset +
		hdr->meta_in_dump_buf_size;
	hdr->meta_out_0_dump_buf_size = p->meta_out_0_dump_buf_size;
	hdr->meta_out_0_iova = p->meta_out_0_iova;

	/* meta out 1 */
	hdr->meta_out_1_dump_buf_offset =
		hdr->meta_out_0_dump_buf_offset +
		hdr->meta_out_0_dump_buf_size;
	hdr->meta_out_1_dump_buf_size = p->meta_out_1_dump_buf_size;
	hdr->meta_out_1_iova = p->meta_out_1_iova;

	/* meta out 2 */
	hdr->meta_out_2_dump_buf_offset =
		hdr->meta_out_1_dump_buf_offset +
		hdr->meta_out_1_dump_buf_size;
	hdr->meta_out_2_dump_buf_size = p->meta_out_2_dump_buf_size;
	hdr->meta_out_2_iova = p->meta_out_2_iova;

	/* ipi frame param */
	hdr->frame_dump_offset =
		hdr->meta_out_2_dump_buf_offset +
		hdr->meta_out_2_dump_buf_size;
	hdr->frame_dump_size = p->frame_param_size;

	/* ipi config param */
	hdr->config_dump_offset =
		hdr->frame_dump_offset +
		hdr->frame_dump_size;
	hdr->config_dump_size = p->config_param_size;
	hdr->used_stream_num = 1;

	if (hdr->config_dump_offset + hdr->config_dump_size > buf_size) {
		pr_info("[%s] buf is not enough\n", __func__);
		return -1;
	}

	return 0;
}

static int mtk_cam_dump_content_to_buf(void *buf, size_t buf_size,
				       void *src, size_t size,
				       size_t offset)
{
	if (!size)
		return 0;

	if (!buf || offset + size > buf_size)
		return -1;

	memcpy(buf + offset, src, size);
	return 0;
}

static int mtk_cam_dump_to_buf(struct mtk_cam_dump_param *p,
			       void *buf, size_t buf_size)
{
	struct mtk_cam_dump_header *hdr;
	int ret;

	if (buf_size < sizeof(*hdr))
		return -1;

	hdr = (struct mtk_cam_dump_header *)buf;
	memset(hdr, 0, sizeof(*hdr));

	if (mtk_cam_write_header(p, hdr, buf_size))
		return -1;

	ret = mtk_cam_dump_content_to_buf(buf, buf_size,
					  p->cq_cpu_addr,
					  hdr->cq_size,
					  hdr->cq_dump_buf_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->meta_in_cpu_addr,
					    hdr->meta_in_dump_buf_size,
					    hdr->meta_in_dump_buf_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->meta_out_0_cpu_addr,
					    hdr->meta_out_0_dump_buf_size,
					    hdr->meta_out_0_dump_buf_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->meta_out_1_cpu_addr,
					    hdr->meta_out_1_dump_buf_size,
					    hdr->meta_out_1_dump_buf_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->meta_out_2_cpu_addr,
					    hdr->meta_out_2_dump_buf_size,
					    hdr->meta_out_2_dump_buf_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->frame_params,
					    hdr->frame_dump_size,
					    hdr->frame_dump_offset);

	ret = ret ||
		mtk_cam_dump_content_to_buf(buf, buf_size,
					    p->config_params,
					    hdr->config_dump_size,
					    hdr->config_dump_offset);
	return ret;
}

static void set_exp_buf_size_locked(struct mtk_cam_exception *exp,
				    size_t new_size)
{
	if (!new_size) {
		if (exp->buf)
			vfree(exp->buf);

		exp->buf_size = 0;
		exp->buf = 0;
		return;
	}

	if (new_size <= exp->buf_size && exp->buf)
		return;

	if (exp->buf)
		vfree(exp->buf);

	exp->buf_size = max(exp->buf_size, new_size);
	exp->buf = vmalloc(exp->buf_size);
}

void mtk_cam_debug_exp_reset(struct mtk_cam_debug *dbg)
{
	struct mtk_cam_exception *exp = &dbg->exp;

	pr_info("%s: previous ready %d\n", __func__, atomic_read(&exp->ready));

	mutex_lock(&exp->lock);
	set_exp_buf_size_locked(exp, 0);
	mutex_unlock(&exp->lock);

	atomic_set(&exp->ready, 0);
}

int mtk_cam_debug_exp_dump(struct mtk_cam_debug *dbg,
			   struct mtk_cam_dump_param *p)
{
	struct mtk_cam_exception *exp = &dbg->exp;
	struct device *dev = dbg->cam->dev;
	size_t new_size;
	int ret;

	if (!exp->dump_entry)
		return -1;

	if (atomic_read(&exp->ready)) {
		dev_info_ratelimited(dev,
				     "%s: skip due to unread dump\n", __func__);
		return -1;
	}

	mutex_lock(&exp->lock);

	new_size = required_buffer_size(p);
	set_exp_buf_size_locked(exp, new_size);

	if (!exp->buf) {
		dev_info(dev, "%s: no buf to dump, size %zu\n", __func__,
			 exp->buf_size);

		mutex_unlock(&exp->lock);
		return -1;
	}

	ret = mtk_cam_dump_to_buf(p, exp->buf, exp->buf_size);

	mutex_unlock(&exp->lock);

	if (!ret)
		atomic_set(&exp->ready, 1);

	dev_info(dev, "%s: ctx %d req_fd %d seq %d, buf_size %zu ret = %d\n",
		 __func__,
		 p->stream_id, p->request_fd, p->sequence,
		 exp->buf_size, ret);

	return ret;
}
