// Copyright ESWIN 2024

//#ifdef ESWIN_PRODUCT_TEST

#include "eswin_eph861x_types.h"
#include "eswin_eph861x_comms.h"
#include "eswin_eph861x_eswin.h"
#include "eswin_eph861x_bootloader.h"
#include "eswin_eph861x_tlv_report.h"
#include "eswin_eph861x.h"
#include "eswin_eph861x_testing.h"
#include "eswin_eph861x_tlv_command.h"

static int eph_write(struct eph_data *ephdata, u8 write_type, u16 comp_id,
        u16 offset, u8 *data, u32 data_len)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;

    u8 *tlv = NULL;
    u16 payload_len = 0;
    u32 tlv_len = 0;

    if (!ephdata) {
        dev_err(dev, "wrong param\n");
        return -1;
    }
    if ((write_type != TLV_CONFIG_DATA_WRITE) &&
        (write_type != TLV_CONTROL_DATA_WRITE)) {
        dev_err(dev, "wrong param\n");
        return -1;
    }

    payload_len = sizeof(comp_id) + sizeof(offset) + data_len;
    tlv_len = sizeof(write_type) + sizeof(payload_len) + payload_len;

    tlv = devm_kzalloc(dev, tlv_len, GFP_KERNEL);
    if (!tlv) {
        dev_err(dev, "malloc mem fail\n");
        return -ENOMEM;
    }

    tlv[0] = write_type;
    tlv[1] = payload_len & 0xFF;
    tlv[2] = (payload_len >> 8) & 0xFF;
    tlv[3] = comp_id & 0xFF;
    tlv[4] = (comp_id >> 8) & 0xFF;
    tlv[5] = offset & 0xFF;
    tlv[6] = (offset >> 8) & 0xFF;
    if (data_len > 0)
        memcpy(&tlv[7], data, data_len);

    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config(ephdata, tlv_len, tlv);
    mutex_unlock(&ephdata->comms_mutex);

    return ret_val;
}

/* enable_flags: BIT0 mct singal
 *               BIT1 sct singal
 *               BIT2 mct delta
 *               BIT3 sct delta
 */
static int eph_testing_preprocess(struct eph_data *ephdata, unsigned int enable_flags)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;

    u8 type = 0;
    u16 comp_id = 0;
    u16 offset = 0;
    u8 enable = 0;
    unsigned int debug_post_flags = 0;
    //u8 mct_freq_ctrl = 0;

    dev_info(dev, "%s\n", __func__);

    disable_irq(ephdata->chg_irq);

    // tic into active state
    type = TLV_CONTROL_DATA_WRITE;
    comp_id = 0; //control
    offset = 0x16; // param 'force active'
    enable = 1; // disable force active
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &enable, sizeof(enable));
    if (ret_val) {
        dev_err(dev, "eph write tic into active state fail %x\n", ret_val);
    }

    // disable report event
# if 0
    type = TLV_CONFIG_DATA_WRITE;
    comp_id = 500; //comp reporting
    offset = 0; // param 'enable_event_report'
    enable = 0; // disable event report
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &enable, sizeof(enable));
    if (ret_val) {
        dev_err(dev, "eph write disable report event fail %x\n", ret_val);
        goto error;
    }
#endif
#if 0 // this make returned wrong eng data
    // fixed freq at index 0
    type = TLV_CONTROL_DATA_WRITE;
    comp_id = 270; // comp sampling
    offset = 12; // param 'host_mct_freq_control'
    /* BIT0 - BIT3: freq index (0, 1, 2, 3)
     * BIT7: enable_manual
     */
    mct_freq_ctrl = BIT(7);
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &mct_freq_ctrl, sizeof(mct_freq_ctrl));
    if (ret_val) {
        dev_err(dev, "eph write fail %x\n", ret_val);
        goto error;
    }
#endif
    // enable singals
    if (enable_flags & 0x3) {
        type = TLV_CONTROL_DATA_WRITE;
        comp_id = 140; // comp sensing
        offset = 24; // param 'debug_post_flags'
        /* BIT0: mct singals enable
         * BIT1: sct singals enable
         */
        debug_post_flags = (0x1 & enable_flags) | (0x2 & enable_flags);
        ret_val = eph_write(ephdata, type, comp_id, offset,
                (u8 *)&debug_post_flags, sizeof(debug_post_flags));
        if (ret_val) {
            dev_err(dev, "eph write enable singals fail %x\n", ret_val);
            goto error;
        }
    }

    // enable deltas
    if (enable_flags & 0xC) {
        type = TLV_CONTROL_DATA_WRITE;
        comp_id = 270; // comp preprocessing
        offset = 13; // param 'debug_post_flags'
        /* BIT0: mct delta enable
         * BIT1: sct delta enable
         */
        debug_post_flags = (0x1 & (enable_flags >> 2)) | (0x2 & (enable_flags >> 2));
        ret_val = eph_write(ephdata, type, comp_id, offset,
                (u8 *)&debug_post_flags, sizeof(debug_post_flags));
        if (ret_val) {
            dev_err(dev, "eph write enable deltas fail %x\n", ret_val);
            goto error;
        }
    }

    return ret_val;

error:
    enable_irq(ephdata->chg_irq);
    return ret_val;
}

/* disable_flags: BIT0 mct singal
 *               BIT1 sct singal
 *               BIT2 mct delta
 *               BIT3 sct delta
 */
static int eph_testing_postprocess(struct eph_data *ephdata, unsigned int disable_flags)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;

    u8 type = TLV_CONFIG_DATA_WRITE;
    u16 comp_id = 0;
    u16 offset = 0;
    u8 enable = 0;
    unsigned int debug_post_flags = 0;
    //u8 mct_freq_ctrl = 0;

    dev_info(dev, "%s\n", __func__);

    // disable singals
    if (disable_flags & 0x3) {
        type = TLV_CONTROL_DATA_WRITE;
        comp_id = 140; // comp sensing
        offset = 24; // param 'debug_post_flags'
        /* BIT0: mct singals enable
         * BIT1: sct singals enable
         */
        debug_post_flags = 0;
        ret_val = eph_write(ephdata, type, comp_id, offset,
                (u8 *)&debug_post_flags, sizeof(debug_post_flags));
        if (ret_val) {
            dev_err(dev, "eph write fail %x\n", ret_val);
        }
    }

    // disable deltas
    if (disable_flags & 0xC) {
        type = TLV_CONTROL_DATA_WRITE;
        comp_id = 270; // comp preprocessing
        offset = 13; // param 'debug_post_flags'
        /* BIT0: mct delta enable
         * BIT1: sct delta enable
         */
        debug_post_flags = 0;
        ret_val = eph_write(ephdata, type, comp_id, offset,
                (u8 *)&debug_post_flags, sizeof(debug_post_flags));
        if (ret_val) {
            dev_err(dev, "eph write fail %x\n", ret_val);
        }
    }

#if 0
    // restore freq
    type = TLV_CONTROL_DATA_WRITE;
    comp_id = 270; // comp sampling
    offset = 12; // param 'host_mct_freq_control'
    /* BIT0 - BIT3: freq index (0, 1, 2, 3)
     * BIT7: enable_manual
     */
    mct_freq_ctrl = 0;
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &mct_freq_ctrl, sizeof(mct_freq_ctrl));
    if (ret_val) {
        dev_err(dev, "eph write fail %x\n", ret_val);
    }
#endif

    // enable report data
# if 0
    type = TLV_CONFIG_DATA_WRITE;
    comp_id = 500; //comp reporting
    offset = 0; // param 'enable_event_report'
    enable = 3; // disable event report
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &enable, sizeof(enable));
    if (ret_val) {
        dev_err(dev, "eph write fail %x\n", ret_val);
    }
#endif
    // tic into normal state
    type = TLV_CONTROL_DATA_WRITE;
    comp_id = 0; //control
    offset = 0x16; // param 'force active'
    enable = 0; // disable force active
    ret_val = eph_write(ephdata, type, comp_id, offset,
            &enable, sizeof(enable));
    if (ret_val) {
        dev_err(dev, "eph write fail %x\n", ret_val);
    }

    enable_irq(ephdata->chg_irq);

    return ret_val;
}

/* check open and short */
static int eph_testing_pt01_process(struct eph_data *ephdata, u8 *msg)
{
    int ret_val = 0;
    int i = 0;
    struct device *dev = NULL;
    u8 type = 0;
    u16 length = 0;
    u16 comp_id = 0;
    u8 data_id = 0;
    int16_t *data_ptr = NULL;
    int16_t adjust_range = ephdata->pt_threshold_add;

    if (!msg)
        return -1;

    dev = (struct device *)&ephdata->commsdevice->dev;
    if (!dev)
        return -1;

    type = msg[0];
    length = *(u16 *)&msg[1];
    comp_id = *(u16 *)&msg[3];
    data_id = msg[5];

    dev_info(dev, "%s, t[%d] len[%d] comp_id[%d] data_id [%d]", __func__, type, length, comp_id, data_id);

    if (length <= 0) {
        dev_err(dev, "Not enough len\n");
        return -1;
    }

    data_ptr = (int16_t *)&msg[7];

    if (sizeof(pt_max_ref_data) > (length - 4)) {
        dev_err(dev, "Received data size not match the ref data\n");
        return -1;
    }

    for (i = 0; i < sizeof(pt_max_ref_data)/2; i++) {
        if (data_ptr[i] > (pt_max_ref_data[i] + adjust_range)) {
            ret_val = -1;
            dev_err(dev, "node[%d]: data[%d] max_data[%d] adjust_range[%d]\n", i, data_ptr[i],
                    pt_max_ref_data[i], adjust_range);
            break;
        }
    }

    for (i = 0; i < sizeof(pt_min_ref_data)/2; i++) {
        if (data_ptr[i] < (pt_min_ref_data[i] - adjust_range)) {
            ret_val = -1;
            dev_err(dev, "node[%d]: data[%d] min_data[%d] adjust_range[%d]\n", i, data_ptr[i],
                    pt_min_ref_data[i], adjust_range);
            break;
        }
    }

    return ret_val;
}

/* check rawdata */
static int eph_testing_pt02_process(struct eph_data *ephdata, u8 *msg)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val = 0;
    int i = 0, j = 0;
    u8 type = 0;
    u16 length = 0;
    u16 comp_id = 0;
    u8 data_id = 0;
    u16 tx_num = 18;
    u16 rx_num = 40;
    int16_t *data_ptr = NULL;
    int16_t max_diff = ephdata->pt_flatness_max;
    int16_t diff = 0;

    if (!msg)
        return -1;

    type = msg[0];
    length = *(u16 *)&msg[1];
    comp_id = *(u16 *)&msg[3];
    data_id = msg[5];

    dev_info(dev, "%s, t[%d] len[%d] comp_id[%d] data_id [%d]", __func__, type, length, comp_id, data_id);

    data_ptr = (int16_t *)&msg[7];

    /* PT spec need to ignore the top two rows,
     * the bottom two rows and the first cols,
     * the end cols
    */
    /* col check */
    for (i = 1; i < (tx_num-1); i++) {
        for (j = 2; j < (rx_num-2); j++) {
            diff = data_ptr[i * tx_num + j] - data_ptr[i * tx_num + j - 1];
            if (diff < 0)
                diff = -diff;
            if (diff > max_diff) {
                ret_val = -1;
                dev_err(dev, "col node[%d]:diff[%d] max_diff[%d]\n", i, diff, max_diff);
                goto out;
            }
        }
    }

    /* row check */
    for (i = 2; i < (rx_num-2); i++) {
        for (j = 2; j < (tx_num-1); j++) {
            diff = data_ptr[i * rx_num + j] - data_ptr[i * rx_num + j - 1];
            if (diff < 0)
                diff = -diff;
            if (diff > max_diff) {
                ret_val = -1;
                dev_err(dev, "row node[%d]:diff[%d] max_diff[%d]\n", i, diff, max_diff);
                goto out;
            }
        }
    }

out:
    return ret_val;
}

/* check max delta (noise part1) */
static int eph_testing_pt03_process(struct eph_data *ephdata, u8 *msg)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val = 0;
    int i = 0;
    u8 type = 0;
    u16 length = 0;
    u16 comp_id = 0;
    u8 data_id = 0;
    int16_t *data_ptr = NULL;
    int16_t delta = 0;
    int16_t max_delta = ephdata->pt_max_delta;

    if (!msg)
        return -1;

    type = msg[0];
    length = *(u16 *)&msg[1];
    comp_id = *(u16 *)&msg[3];
    data_id = msg[5];

    dev_info(dev, "%s, t[%d] len[%d] comp_id[%d] data_id [%d]", __func__, type, length, comp_id, data_id);

    data_ptr = (int16_t *)&msg[7];

    for (i = 0; i < (length - 4); i++)  {
        if (data_ptr[i] < 0)
            delta = -data_ptr[i];
        delta = -data_ptr[i];

        if (delta > max_delta) {
            ret_val = -1;
            dev_err(dev, "node[%d]:delta[%d] max_delta[%d]\n", i, delta, max_delta);
            break;
        }
    }

    return ret_val;
}

/* check the range between max and min (noise part2) */
static int eph_testing_pt04_process(struct eph_data *ephdata, u8 *msg)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val = 0;
    int i = 0;
    u8 type = 0;
    u16 length = 0;
    u16 comp_id = 0;
    u8 data_id = 0;
    int16_t *data_ptr = NULL;

    int16_t max_diff = ephdata->pt_max_diff_delta;

    static int16_t max_delta[40*18] = { 0 };
    static int16_t min_delta[40*18] = { 0 };

    if (!msg)
        return -1;

    type = msg[0];
    length = *(u16 *)&msg[1];
    comp_id = *(u16 *)&msg[3];
    data_id = msg[5];

    dev_info(dev, "%s, t[%d] len[%d] comp_id[%d] data_id [%d]", __func__, type, length, comp_id, data_id);

    data_ptr = (int16_t *)&msg[7];

    // update max/min delta
    for (i = 0; i < sizeof(max_delta)/2; i++) {
        if (data_ptr[i] > max_delta[i])
            max_delta[i] = data_ptr[i];
        if (data_ptr[i] < min_delta[i])
            min_delta[i] = data_ptr[i];
    }

    for (i = 0; i < sizeof(max_delta)/2; i++) {
        if ((max_delta[i] - min_delta[i]) > max_diff) {
            ret_val = -1;
            dev_err(dev, "node[%d]:max_delta[%d] min_delta[%d] max_diff[%d]\n", i, max_delta[i], min_delta[i],
                    max_diff);
            break;
        }
    }

    return ret_val;
}

static int eph_testing_pt(struct eph_data *ephdata, int pt_index, u8 *buf)
{
    int ret_val = 0;
    int retry_cnt = 2;
    struct tlv_header tlvheader;
    struct device *dev = &ephdata->commsdevice->dev;
    unsigned int enable_flags = 0;
    int need_datasets = 1;

    if (pt_index <= 0)
        return -1;

    if (pt_index == 1) {
        enable_flags = BIT(0);
    } else if (pt_index == 2) {
        enable_flags = BIT(0);
    } else if (pt_index == 3) {
        enable_flags = BIT(2);
    } else if (pt_index == 4) {
        enable_flags = BIT(2);
        need_datasets = 5;
    }

    dev_info(dev, "%s\n", __func__);

    ret_val = eph_testing_preprocess(ephdata, enable_flags);
    if (ret_val) {
        dev_err(dev, "testing preprocess fail %x\n", ret_val);
        return ret_val;
    }

    /* tic need at least 100ms to generate the first out */
    msleep(1000);

retry:
    msleep(200);
    memset(buf, 0x0, PAGE_SIZE);
    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_comms_two_stage_read(ephdata, buf);
    mutex_unlock(&ephdata->comms_mutex);

    if (unlikely(ret_val)) {
        if (retry_cnt) {
            retry_cnt--;
            goto retry;
        }
        dev_err(dev, "read fail %x\n", ret_val);
        goto exit;
    }

    tlvheader = eph_get_tl_header_info(ephdata, buf);
    if (tlvheader.type != TLV_ENG_DEBUG_DATA) {
        if (retry_cnt) {
            retry_cnt--;
            goto retry;
        }
        dev_err(dev, "read wrong data\n");
        ret_val = -1;
        goto exit;
    }

    need_datasets--;

    if (pt_index == 1)
        ret_val = eph_testing_pt01_process(ephdata, buf);
    else if (pt_index == 2)
        ret_val = eph_testing_pt02_process(ephdata, buf);
    else if (pt_index == 3)
        ret_val = eph_testing_pt03_process(ephdata, buf);
    else if (pt_index == 4)
        ret_val = eph_testing_pt04_process(ephdata, buf);

    if (ret_val) {
        dev_err(dev, "%s fail %x\n", __func__, ret_val);
        goto exit;
    }

    if (need_datasets > 0) {
        retry_cnt = 2;
        goto retry;
    }

exit:
    if (eph_testing_postprocess(ephdata, enable_flags))
        dev_err(dev, "testing postprocess fail\n");

    return ret_val;
}

int eph_testing_pt_data_malloc(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
    if (!ephdata->pt1_buffer) {
        ephdata->pt1_buffer = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
        if (!ephdata->pt1_buffer) {
            return -ENOMEM;
        }
    } else {
        dev_info(dev, "already malloc pt1 buffer\n");
    }
    if (!ephdata->pt2_buffer) {
        ephdata->pt2_buffer = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
        if (!ephdata->pt2_buffer) {
            return -ENOMEM;
        }
    } else {
        dev_info(dev, "already malloc pt2 buffer\n");
    }
    if (!ephdata->pt3_buffer) {
        ephdata->pt3_buffer = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
        if (!ephdata->pt3_buffer) {
            return -ENOMEM;
        }
    } else {
        dev_info(dev, "already malloc pt3 buffer\n");
    }
    if (!ephdata->pt4_buffer) {
        ephdata->pt4_buffer = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
        if (!ephdata->pt4_buffer) {
            return -ENOMEM;
        }
    } else {
        dev_info(dev, "already malloc pt4 buffer\n");
    }
    return 0;
}

void eph_testing_pt_data_free(struct eph_data *ephdata)
{
//    int ret = 0;
    struct device *dev = &ephdata->commsdevice->dev;
    if (ephdata->pt1_buffer)
        devm_kfree(dev, ephdata->pt1_buffer);
    if (ephdata->pt2_buffer)
        devm_kfree(dev, ephdata->pt2_buffer);
    if (ephdata->pt3_buffer)
        devm_kfree(dev, ephdata->pt3_buffer);
    if (ephdata->pt4_buffer)
        devm_kfree(dev, ephdata->pt4_buffer);
    ephdata->pt1_buffer = NULL;
    ephdata->pt2_buffer = NULL;
    ephdata->pt3_buffer = NULL;
    ephdata->pt4_buffer = NULL;
    ephdata->stored_pt_data = false;
    return;
}

/* Self test API */
int eswin_product_self_test(struct eph_data *ephdata)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;
    u8 *eng_data_buf = NULL;

    if (ephdata->suspended) {
        dev_err(dev, "TIC suspended! Wait wakeup!\n");
        goto exit;
    }

    eng_data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
    if (!eng_data_buf) {
        ret_val = -1;
        goto exit;
    }

    ephdata->stored_pt_data = false;
    if (eph_testing_pt_data_malloc(ephdata) < 0) {
        eph_testing_pt_data_free(ephdata);
        ret_val = -1;
        goto exit1;
    }
    
    memset(ephdata->pt1_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt2_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt3_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt4_buffer, 0x0, PAGE_SIZE);

    // open/short test
    ret_val = eph_testing_pt(ephdata, OPEN_SHORT, eng_data_buf);
    dev_info(dev, "eph_testing_pt open/short test result %d\n", ret_val);

    // store open/short test data
    memcpy(ephdata->pt1_buffer, eng_data_buf, PAGE_SIZE);

    memset(eng_data_buf, 0, PAGE_SIZE);

    // rawdata test
    ret_val |= eph_testing_pt(ephdata, SIGNAL_FLATNESS, eng_data_buf);
    dev_info(dev, "eph_testing_pt rawdata test result %d\n", ret_val);

    // store rawdata test data
    memcpy(ephdata->pt2_buffer, eng_data_buf, PAGE_SIZE);

    memset(eng_data_buf, 0, PAGE_SIZE);

    // noise part1
    ret_val |= eph_testing_pt(ephdata, DELTA_CASE, eng_data_buf);
    dev_info(dev, "eph_testing_pt noise part1 result %d\n", ret_val);

    // store noise part1 test data
    memcpy(ephdata->pt3_buffer, eng_data_buf, PAGE_SIZE);

    memset(eng_data_buf, 0, PAGE_SIZE);

    // noise part2
    ret_val |= eph_testing_pt(ephdata, DELTA_RANGE, eng_data_buf);
    dev_info(dev, "eph_testing_pt noise part2 result %d\n", ret_val);

    // store noise part2 test data
    memcpy(ephdata->pt4_buffer, eng_data_buf, PAGE_SIZE);

    memset(eng_data_buf, 0, PAGE_SIZE);

    ephdata->stored_pt_data = true;

exit1:
    devm_kfree(dev, eng_data_buf);
    eng_data_buf = NULL;
exit:
    return ret_val;
}

/* NOTE: create the Product test node for privide test
   specail case from user space
 */

/* TRX Short/Open Test */
ssize_t eph_devattr_test_pt01_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret_val = 0;
    unsigned int count = 0;
    unsigned int i = 0;
    struct eph_data *ephdata;
    u8 *eng_data_buf = NULL;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (ephdata->suspended) {
        dev_err(dev, "TIC suspended! Wait wakeup!\n");
        goto exit;
    }

    eng_data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
    if (!eng_data_buf)
        goto exit;

    ret_val = eph_testing_pt(ephdata, 1, eng_data_buf);

    count += scnprintf(buf, PAGE_SIZE,
            "TEST PT$01: %s\n", (ret_val < 0) ? "fail" : "pass");

    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
            eng_data_buf[i]);
    }
    count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

    devm_kfree(dev, eng_data_buf);
    eng_data_buf = NULL;
exit:
    return count;
}

ssize_t eph_devattr_test_pt01_set(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    int input = 0;
    struct eph_data *ephdata;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (kstrtoint(buf, 10, &input)) {
        dev_err(dev, "set wrong para\n");
        return -EINVAL;
    }

    dev_info(dev, "%s %d\n", __func__, input);

    if (input < 0)
        return -EINVAL;

    if (input > 0)
        ephdata->pt_threshold_add = input;

    return ephdata->pt_threshold_add;
}

/* rawdata Test */
ssize_t eph_devattr_test_pt02_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret_val = 0;
    unsigned int count = 0;
    unsigned int i = 0;
    struct eph_data *ephdata;
    u8 *eng_data_buf = NULL;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (ephdata->suspended) {
        dev_err(dev, "TIC suspended! Wait wakeup!\n");
        goto exit;
    }

    eng_data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
    if (!eng_data_buf)
        goto exit;

    ret_val = eph_testing_pt(ephdata, 2, eng_data_buf);

    count += scnprintf(buf, PAGE_SIZE,
            "TEST PT$02: %s\n", (ret_val < 0) ? "fail" : "pass");

    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
            eng_data_buf[i]);
    }
    count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

    devm_kfree(dev, eng_data_buf);
    eng_data_buf = NULL;
exit:
    return count;
}

ssize_t eph_devattr_test_pt02_set(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    int input = 0;
    struct eph_data *ephdata;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (kstrtoint(buf, 10, &input)) {
        dev_err(dev, "set wrong para\n");
        return -EINVAL;
    }

    if (input < 0)
        return -EINVAL;

    if (input > 0)
        ephdata->pt_flatness_max = input;

    return ephdata->pt_flatness_max;
}

ssize_t eph_devattr_test_pt03_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret_val = 0;
    unsigned int count = 0;
    unsigned int i = 0;
    struct eph_data *ephdata;
    u8 *eng_data_buf = NULL;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (ephdata->suspended) {
        dev_err(dev, "TIC suspended! Wait wakeup!\n");
        goto exit;
    }

    eng_data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
    if (!eng_data_buf)
        goto exit;

    ret_val = eph_testing_pt(ephdata, 3, eng_data_buf);

    count += scnprintf(buf, PAGE_SIZE,
            "TEST PT$03: %s\n", (ret_val < 0) ? "fail" : "pass");

    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
            eng_data_buf[i]);
    }
    count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

    devm_kfree(dev, eng_data_buf);
    eng_data_buf = NULL;
exit:
    return count;

}

ssize_t eph_devattr_test_pt03_set(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    int input = 0;
    struct eph_data *ephdata;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (kstrtoint(buf, 10, &input)) {
        dev_err(dev, "set wrong para\n");
        return -EINVAL;
    }

    if (input < 0)
        return -EINVAL;

    if (input > 0)
        ephdata->pt_max_delta = input;

    return ephdata->pt_max_delta;
}

ssize_t eph_devattr_test_pt04_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret_val = 0;
    unsigned int count = 0;
    unsigned int i = 0;
    struct eph_data *ephdata;
    u8 *eng_data_buf = NULL;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (ephdata->suspended) {
        dev_err(dev, "TIC suspended! Wait wakeup!\n");
        goto exit;
    }

    eng_data_buf = devm_kzalloc(dev, PAGE_SIZE, GFP_KERNEL);
    if (!eng_data_buf)
        goto exit;

    ret_val = eph_testing_pt(ephdata, 4, eng_data_buf);

    count += scnprintf(buf, PAGE_SIZE,
            "TEST PT$04: %s\n", (ret_val < 0) ? "fail" : "pass");

    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, PAGE_SIZE - count, "%d ",
            eng_data_buf[i]);
    }
    count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

    devm_kfree(dev, eng_data_buf);
    eng_data_buf = NULL;
exit:
    return count;
}

ssize_t eph_devattr_test_pt04_set(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    int input = 0;
    struct eph_data *ephdata;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (kstrtoint(buf, 10, &input)) {
        dev_err(dev, "set wrong para\n");
        return -EINVAL;
    }

    if (input < 0)
        return -EINVAL;

    if (input > 0)
        ephdata->pt_max_diff_delta = input;

    return ephdata->pt_max_diff_delta;
}

//#else

//#endif // ESWIN_PRODUCT_TEST
