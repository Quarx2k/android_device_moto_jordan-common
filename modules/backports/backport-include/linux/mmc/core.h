#ifndef __BACKPORT_MMC_CORE_H
#define __BACKPORT_MMC_CORE_H
#include <linux/version.h>
#include_next <linux/mmc/core.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define mmc_align_data_size LINUX_BACKPORT(mmc_align_data_size)
extern unsigned int mmc_align_data_size(struct mmc_card *, unsigned int);
#endif /* 2.6.24 - 2.6.26 */

#endif /* __BACKPORT_MMC_CORE_H */
