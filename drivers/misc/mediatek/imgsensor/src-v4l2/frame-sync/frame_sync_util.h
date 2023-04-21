/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_UTIL_H
#define _FRAME_SYNC_UTIL_H


/*******************************************************************************
 * Frame Sync Util functions.
 ******************************************************************************/
long long calc_mod_64(const long long a, const long long b);


/* sensor related APIs */
unsigned int calcLineTimeInNs(
	const unsigned int pclk, const unsigned int linelength);

unsigned int convert2TotalTime(
	const unsigned int lineTimeInus, const unsigned int time);
unsigned int convert2LineCount(
	const unsigned int lineTimeInNs, const unsigned int val);
/* *** */


/* timestamp/tick related APIs */
unsigned long long convert_timestamp_2_tick(
	const unsigned long long timestamp, const unsigned int tick_factor);
unsigned long long convert_tick_2_timestamp(
	const unsigned long long tick, const unsigned int tick_factor);

unsigned long long calc_time_after_sof(
	const unsigned long long timestamp,
	const unsigned long long tick, const unsigned int tick_factor);

unsigned int check_tick_b_after_a(
	const unsigned long long tick_a_, const unsigned long long tick_b_);

unsigned int check_timestamp_b_after_a(
	const unsigned long long ts_a, const unsigned long long ts_b,
	const unsigned int tick_factor);

unsigned long long get_two_timestamp_diff(
	const unsigned long long ts_a, const unsigned long long ts_b,
	const unsigned int tick_factor);

void get_array_data_from_new_to_old(
	const unsigned long long *in_arr,
	const unsigned int newest_idx, const unsigned int len,
	unsigned long long *res_data);

int get_ts_diff_table_idx(const unsigned int idx_a, const unsigned int idx_b);

long long find_two_sensor_timestamp_diff(
	const unsigned long long *ts_a_arr, const unsigned long long *ts_b_arr,
	const unsigned int ts_arr_len, const unsigned int tick_factor);

int check_sync_result(
	const long long *ts_diff_table_arr, const unsigned int mask,
	const unsigned int len, const unsigned int threshold);
/* *** */


/******************************************************************************/

#endif
