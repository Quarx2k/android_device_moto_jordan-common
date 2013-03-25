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

#include "nvs.h"

#define WL1271_NVS_MAP_SIZE     332

static struct wl1271_general_parms conf_general_parms = {
	.ref_clk = 1,					/* 26 Mhz */
	.settling_time = 5,
	.clk_valid_on_wakeup = 0,
	.dc2dcmode = 0,					/* btSPI is not used */
	.single_dual_band = 0,			/* Single band */

	.tx_bip_fem_autodetect = 0,		/* Manual mode */
	.tx_bip_fem_manufacturer = 1,	/* Triquint */
	.settings = 1,					/* NBI on, Telec channel 14 off */

	.sr_state = 1,					/* Enabled */

	.srf1 = {0x07, 0x03, 0x18, 0x10, 0x05, 0xfb, 0xf0, 0xe8,},
	.srf2 = {0x07, 0x03, 0x18, 0x10, 0x05, 0xf6, 0xf0, 0xe8,},
	.srf3 = {0x07, 0x03, 0x18, 0x10, 0x05, 0xfb, 0xf0, 0xe8,},
};

static struct wl1271_radio_parms_stat conf_radio_parms_stat = {
	.rx_trace_loss = 0,
	.tx_trace_loss = 0,
	.rx_rssi_and_proc_compens =	{0xec, 0xf6,00, 0x0c, 0x18, 0xf8, 0xfc, 0x00, 0x08, 0x10, 0xf0, 0xf8, 0x00, 0x0a, 0x14},
};

static struct wl1271_radio_parms_dyn conf_radio_parms_dyn = {
	.tx_ref_pd_voltage = 0x177,
	.tx_ref_power = 0x80,
	.tx_offset_db = 0,

	.tx_rate_limits_normal = {0x1c, 0x1f, 0x22, 0x24, 0x28, 0x29},
	.tx_rate_limits_degraded = {0x19, 0x1f, 0x22, 0x23, 0x27, 0x28},

	/* Copied from wl1271 reference */
	.tx_rate_limits_extreme = {0x19, 0x1c, 0x1e, 0x20, 0x24, 0x25},

	.tx_channel_limits_11b = {0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50},
	.tx_channel_limits_ofdm = {0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50},
	.tx_pdv_rate_offsets = {0x01, 0x02, 0x02, 0x02, 0x02, 0x00},

	.tx_ibias = {0x11, 0x11, 0x15, 0x11, 0x15, 0x0f},
	.rx_fem_insertion_loss = 0x0e,

	/* Copied from wl1271 reference */
	.degraded_low_to_normal_threshold = 0x1e,

	/* Copied from wl1271 reference */
	.degraded_normal_to_high_threshold = 0x2d,
};

