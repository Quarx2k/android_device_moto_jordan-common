/**
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __NVS_H__
#define __NVS_H__

#define u8		unsigned char
#define s8		unsigned char
#define __le16	short

/*
 * Taken from linux-2.6/drivers/net/wireless/wl12xx/wl1271.h
 */

/* NVS data structure */
#define WL1271_NVS_SECTION_SIZE                  468

#define WL1271_NVS_GENERAL_PARAMS_SIZE            57
#define WL1271_NVS_GENERAL_PARAMS_SIZE_PADDED \
	(WL1271_NVS_GENERAL_PARAMS_SIZE + 1)

#define WL1271_NVS_STAT_RADIO_PARAMS_SIZE         17
#define WL1271_NVS_STAT_RADIO_PARAMS_SIZE_PADDED \
	(WL1271_NVS_STAT_RADIO_PARAMS_SIZE + 1)

#define WL1271_NVS_DYN_RADIO_PARAMS_SIZE          65
#define WL1271_NVS_DYN_RADIO_PARAMS_SIZE_PADDED \
	(WL1271_NVS_DYN_RADIO_PARAMS_SIZE + 1)

#define WL1271_NVS_FEM_COUNT                       2
#define WL1271_NVS_INI_SPARE_SIZE                124

struct wl1271_nvs_file {
	/* NVS section */
	u8 nvs[WL1271_NVS_SECTION_SIZE];

	/* INI section */
	u8 general_params[WL1271_NVS_GENERAL_PARAMS_SIZE_PADDED];
	u8 stat_radio_params[WL1271_NVS_STAT_RADIO_PARAMS_SIZE_PADDED];
	u8 dyn_radio_params[WL1271_NVS_FEM_COUNT]
			   [WL1271_NVS_DYN_RADIO_PARAMS_SIZE_PADDED];
	u8 ini_spare[WL1271_NVS_INI_SPARE_SIZE];
} __attribute__ ((packed));


#define CONF_MAX_SMART_REFLEX_PARAMS 16
#define CONF_RSSI_AND_PROCESS_COMPENSATION_SIZE 15
#define CONF_NUMBER_OF_SUB_BANDS_5  7
#define CONF_NUMBER_OF_RATE_GROUPS  6
#define CONF_NUMBER_OF_CHANNELS_2_4 14
#define CONF_NUMBER_OF_CHANNELS_5   35

/*
 * Adapted from wireless-2.6.git
 * drivers/net/wireless/wl12xx/wl1271_cmd.h 
 * ba433f423c8bdc8cfdebf0d387e4cd7d29061a3b
 */

struct wl1271_general_parms {
	u8 ref_clk;
	u8 settling_time;
	u8 clk_valid_on_wakeup;
	u8 dc2dcmode;
	u8 single_dual_band;

	u8 tx_bip_fem_autodetect;
	u8 tx_bip_fem_manufacturer;
	u8 settings;

	u8 sr_state;

	s8 srf1[CONF_MAX_SMART_REFLEX_PARAMS];
	s8 srf2[CONF_MAX_SMART_REFLEX_PARAMS];
	s8 srf3[CONF_MAX_SMART_REFLEX_PARAMS];

	/* unsused from here */
	/*
	s8 sr_debug_table[CONF_MAX_SMART_REFLEX_PARAMS];

	u8 sr_sen_n_p;
	u8 sr_sen_n_p_gain;
	u8 sr_sen_nrn;
	u8 sr_sen_prn;

	u8 padding[3];
	*/
} __attribute__ ((packed));

struct wl1271_radio_parms_stat {
	/* 2.4GHz */
	u8 rx_trace_loss;
	u8 tx_trace_loss;
	s8 rx_rssi_and_proc_compens[CONF_RSSI_AND_PROCESS_COMPENSATION_SIZE];
} __attribute__ ((packed));

struct wl1271_radio_parms_dyn {
	/* Dynamic radio parameters */
	/* 2.4GHz */
	__le16 tx_ref_pd_voltage;
	u8  tx_ref_power;
	s8  tx_offset_db;

	s8  tx_rate_limits_normal[CONF_NUMBER_OF_RATE_GROUPS];
	s8  tx_rate_limits_degraded[CONF_NUMBER_OF_RATE_GROUPS];
	s8  tx_rate_limits_extreme[CONF_NUMBER_OF_RATE_GROUPS];

	s8  tx_channel_limits_11b[CONF_NUMBER_OF_CHANNELS_2_4];
	s8  tx_channel_limits_ofdm[CONF_NUMBER_OF_CHANNELS_2_4];
	s8  tx_pdv_rate_offsets[CONF_NUMBER_OF_RATE_GROUPS];

	u8  tx_ibias[CONF_NUMBER_OF_RATE_GROUPS];
	u8  rx_fem_insertion_loss;

	u8  degraded_low_to_normal_threshold;
	u8  degraded_normal_to_high_threshold;
} __attribute__ ((packed));

#endif

