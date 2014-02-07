#ifndef __BACKPORT_MMC_HOST_H
#define __BACKPORT_MMC_HOST_H
#include <linux/version.h>
#include_next <linux/mmc/host.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
/* rename member in struct mmc_host */
#define max_segs	max_hw_segs
#endif

#endif /* __BACKPORT_MMC_HOST_H */
