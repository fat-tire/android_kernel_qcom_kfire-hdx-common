/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __Q6_ADM_V2_H__
#define __Q6_ADM_V2_H__


#define ADM_PATH_PLAYBACK 0x1
#define ADM_PATH_LIVE_REC 0x2
#define ADM_PATH_NONLIVE_REC 0x3
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>


/* multiple copp per stream. */
struct route_payload {
	unsigned int copp_ids[AFE_MAX_PORTS];
	unsigned short num_copps;
	unsigned int session_id;
};

int srs_trumedia_open(int port_id, int srs_tech_id, void *srs_params);

int adm_open(int port, int path, int rate, int mode, int topology,
				bool perf_mode, uint16_t bits_per_sample);

int adm_dolby_dap_get_params(int port_id, uint32_t module_id, uint32_t param_id,
			uint32_t params_length, char *params);

int adm_dolby_dap_send_params(int port_id, char *params,
				 uint32_t params_length);

int adm_multi_ch_copp_open(int port, int path, int rate, int mode,
			int topology, bool perf_mode, uint16_t bits_per_sample);

int adm_memory_map_regions(int port_id, uint32_t *buf_add, uint32_t mempool_id,
				uint32_t *bufsz, uint32_t bufcnt);

int adm_memory_unmap_regions(int port_id, uint32_t *buf_add, uint32_t *bufsz,
						uint32_t bufcnt);

int adm_close(int port, bool perf_mode);

int adm_matrix_map(int session_id, int path, int num_copps,
		unsigned int *port_id, int copp_id, bool perf_mode);

int adm_connect_afe_port(int mode, int session_id, int port_id);

void adm_ec_ref_rx_id(int  port_id);

int adm_get_copp_id(int port_id);

void adm_set_multi_ch_map(char *channel_map);

void adm_get_multi_ch_map(char *channel_map);

int adm_set_stereo_to_custom_stereo(int port_id, unsigned int session_id,
				    char *params, uint32_t params_length);

#endif /* __Q6_ADM_V2_H__ */
