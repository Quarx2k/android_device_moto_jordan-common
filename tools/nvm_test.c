#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

void ReadFile(char *name, char *addr, int lenght_addr);

int netmux_fd;

int main() {

	printf("NVM Daemon Init!!!\n");

	netmux_fd = open("/dev/netmux/nvm_proxy", O_RDWR | O_NONBLOCK);

	if (netmux_fd <= 0) {
		printf("%s: failed, errno=%d\n", __func__, errno);
		return errno;
	}

// Do not change order of loading!

	printf("Load File_Logger\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Logger","\x0f\xbb\x06\x00\x00\x00\x00\x0f\xb2", 9);
	printf("Load File_PDS1\n");
	ReadFile("/pds/bp_nvm/File_PDS1","\x0c\x86\x06\x01\x00\x01\x00\x0c\x7d", 9);
	printf("Load File_PDS2\n");
	ReadFile("/pds/bp_nvm/File_PDS2","\x08\xed\x06\x02\x00\x02\x00\x08\xe4\x02", 10);
	printf("Load File_PDS3\n");
	ReadFile("/pds/bp_nvm/File_PDS3","\x11\x2c\x06\x03\x00\x03\x00\x11\x23\x01", 10);
	printf("Load File_PDS4\n");
	ReadFile("/pds/bp_nvm/File_PDS4","\x0f\xc9\x06\x04\x00\x04\x00\x0f\xc0\x01", 10);
	printf("Load File_GSM\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_GSM","\x06\x23\x06\x05\x00\x05\x00\x06\x1a\x01", 10);
	printf("Load File_GPRS\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_GPRS","\x02\x7a\x06\x06\x00\x06\x00\x02\x71", 9);
	printf("Load File_Audio\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio","\x0c\xc0\x06\x07\x00\x07\x00\x0c\xb7", 9);
	printf("Load File_Audio2\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio2","\x13\x36\x06\x08\x00\x08\x00\x13\x2d", 9);
	printf("Load File_PDS_SBCM\n");
	ReadFile("/pds/bp_nvm/File_PDS_SBCM","\x01\x49\x06\x09\x00\x09\x00\x01\x40", 9);
	printf("Load File_PDS_Subsidy_Lock\n");
	ReadFile("/pds/bp_nvm/File_PDS_Subsidy_Lock","\x05\x3b\x06\x0a\x00\x0a\x00\x05\x32", 9);
	printf("Load File_Seem_Flex_Tables\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Seem_Flex_Tables","\x0e\x60\x06\x0b\x00\x0b\x00\x0e\x57", 9);

// XXX To Check.  We cant ignore this files.
	printf("Load sim_lock_init\n");
	//ReadFile("/system/etc/motorola/bp_nvm_default/sim_lock_init"," ", 9);
	write(netmux_fd, "\x00\x09\x06\x0c\x00\x0c\x01\x00\x00", 9);

	printf("Load generic_pds_init\n");
	//ReadFile("/system/etc/motorola/bp_nvm_default/generic_pds_init"," ", 9);
	write(netmux_fd, "\x00\x09\x06\x0d\x00\x0d\x01\x00\x00", 9);
/// XXX END

	printf("Load File_UMA\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_UMA","\x0f\x48\x06\x0e\x00\x0e\x00\x0f\x3f", 9);
	printf("Load File_PDS_IMEI\n");
	ReadFile("/pds/bp_nvm/File_PDS_IMEI","\x0a\x33\x06\x0f\x00\x0f\x00\x0a\x2a", 9);
	printf("Load FILE_PDS_SUBSIDY_LOCK_DBK_1\n");
	ReadFile("/pds/bp_nvm/FILE_PDS_SUBSIDY_LOCK_DBK_1","\x07\xbb\x06\x10\x00\x10\x00\x07\xb2", 9);
	printf("Load FILE_PDS_SUBSIDY_LOCK_DBK_2\n");
	ReadFile("/pds/bp_nvm/FILE_PDS_SUBSIDY_LOCK_DBK_2","\x09\x6f\x06\x11\x00\x11\x00\x09\x66", 9);
	printf("Load FILE_PDS5\n");
	ReadFile("/pds/bp_nvm/FILE_PDS5","\x0e\x1d\x06\x12\x00\x12\x00\x0e\x14", 9);
	printf("Load File_Audio3\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio3","\x0b\x63\x06\x13\x00\x13\x00\x0b\x5a", 9);
	printf("Load File_Audio1_AMR_WB\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio1_AMR_WB","\x0c\xbf\x06\x14\x00\x14\x00\x0c\xb6", 9);
	printf("Load File_Audio2_AMR_WB\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio2_AMR_WB","\x13\x35\x06\x15\x00\x15\x00\x13\x2c", 9);
	printf("Load File_Audio3_AMR_WB\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio3_AMR_WB","\x0b\x63\x06\x16\x00\x16\x00\x0b\x5a", 9);
	printf("Load File_Audio4\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio4","\x09\x5f\x06\x17\x00\x17\x00\x09\x56", 9);
	printf("Load File_Audio5\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio5","\x09\x5f\x06\x18\x00\x18\x00\x09\x56", 9);
	printf("Load File_Audio6\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio6","\x13\x4b\x06\x19\x00\x19\x00\x13\x42", 9);
	printf("Load File_Audio4_AMR_WB\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio4_AMR_WB","\x13\x4b\x06\x1a\x00\x1a\x00\x13\x42", 9);
	printf("Load File_Audio5_AMR_WB\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio5_AMR_WB","\x0a\x1b\x06\x1b\x00\x1b\x00\x0a\x12", 9);
	printf("Load File_Audio7\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio7","\x0a\x1b\x06\x1c\x00\x1c\x00\x0a\x12", 9);
	printf("Load File_Audio8\n");
	ReadFile("/system/etc/motorola/bp_nvm_default/File_Audio8","\x0b\x20\x06\x1d\x00\x1d\x00\x0b\x17", 9);
	printf("Load File_PDS_LTE1\n");
	ReadFile("/pds/bp_nvm/File_PDS_LTE1","\x11\x8b\x06\x1e\x00\x1e\x00\x11\x82", 9);
	printf("Load File_PDS_LTE2\n");
	ReadFile("/pds/bp_nvm/File_PDS_LTE2","\x1a\x4b\x06\x1f\x00\x1f\x00\x1a\x42", 9);

// XXX To Check.  We cant ignore this files.
	printf("Load unknown info 1\n");
	write(netmux_fd, "\x00\x05\x0a\x20\x00", 5);
	printf("Load unknown info 2\n");
	write(netmux_fd, "\x00\x08\x09\x21\x00\x00\x00\x00", 8);
// XXX end

	return 0;
}


void ReadFile(char *name, char *addr, int lenght_addr)
{
	FILE *file;
	char *buffer;
	unsigned long fileLen;

	int i;


	//Open file
	file = fopen(name, "rb");
	if (!file) {
		fprintf(stderr, "Unable to open file %s", name);
		return;
	}
	
	//Get file length
	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	fseek(file, 0, SEEK_SET);

	//Allocate memory
	buffer=(char *)malloc(fileLen+1+lenght_addr);

	if (!buffer) {
		fprintf(stderr, "Memory error!");
		fclose(file);
		return;
	}

	//Read file contents into buffer
	fread(buffer + lenght_addr, fileLen, 1, file);

	memcpy(buffer, addr, 9);
/*
	for (i = 0; i < fileLen+lenght_addr; i++)
	{
 	   printf("%02X", buffer[i]);
	}
	printf("\n");
*/
	// Write file to netmux
	write(netmux_fd, buffer, fileLen+lenght_addr);

	fclose(file);
	free(buffer);
}


