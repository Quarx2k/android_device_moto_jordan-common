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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nvs.h"
#include "nvs_conf.h"
#define WL1271_NVS_MAP_SIZE     332
void write_nvs(char *nvs, char *out)
{
	FILE *fp = fopen(out, "wb");
	if(fp == NULL) {
		fprintf(stderr, "Failed to open output file! %s\n",out);
		exit(1);
	}

	FILE *ifp = fopen(nvs, "rb");
	if(ifp == NULL) {
		fprintf(stderr, "Failed to open input nvs map!\n");
		fclose(fp);
		exit(1);
	}

	struct wl1271_nvs_file nvs_file;
	memset(&nvs_file, 0, sizeof(struct wl1271_nvs_file));

	if(fread(nvs_file.nvs, WL1271_NVS_MAP_SIZE, 1, ifp) != 1) {
		fprintf(stderr, "Failed to read %d bytes from nvs map!\n", WL1271_NVS_MAP_SIZE);
		fclose(ifp);
		fclose(fp);
		exit(1);
	}

	memcpy(&nvs_file.general_params, &conf_general_parms, sizeof(conf_general_parms));
	memcpy(&nvs_file.stat_radio_params, &conf_radio_parms_stat, sizeof(conf_radio_parms_stat));
	memcpy(&nvs_file.dyn_radio_params, &conf_radio_parms_dyn, sizeof(conf_radio_parms_dyn));

	fwrite(&nvs_file, sizeof(struct wl1271_nvs_file), 1, fp);

	fclose(ifp);
	fclose(fp);
}

int main(int argc, char *argv[])
{
	if(argc < 2) {
		fprintf(stderr, "Usage: mknvs <nvs_map.bin> <output dir wl1271-nvs.bin>\n");
		exit(1);
	}

	printf("Creating wl1271-nvs.bin from %s %s\n", argv[1], argv[2]);
	write_nvs(argv[1], argv[2]);

	return 0;
}
