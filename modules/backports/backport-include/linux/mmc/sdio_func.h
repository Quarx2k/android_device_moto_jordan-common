#ifndef __BACKPORT_MMC_SDIO_FUNC_H
#define __BACKPORT_MMC_SDIO_FUNC_H
#include <linux/version.h>
#include_next <linux/mmc/sdio_func.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
#define sdio_writeb_readb(func, write_byte, addr, err_ret) sdio_readb(func, addr, err_ret)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
/*
 * Backports da68c4eb25
 * sdio: introduce API for special power management features
 *
 * We simply carry around the data structures and flags, and
 * make the host return no flags set by the driver.
 *
 * This is declared in mmc/pm.h upstream, but that files
 * didn't exist before this commit and isn't included directly.
 */
//typedef unsigned int mmc_pm_flag_t;

#define MMC_PM_KEEP_POWER      (1 << 0)        /* preserve card power during suspend */
#define MMC_PM_WAKE_SDIO_IRQ   (1 << 1)        /* wake up host system on SDIO IRQ assertion */
/*
extern mmc_pm_flag_t sdio_get_host_pm_caps(struct sdio_func *func);
extern int sdio_set_host_pm_flags(struct sdio_func *func, mmc_pm_flag_t flags);
*/
#endif

#ifndef dev_to_sdio_func
#define dev_to_sdio_func(d)	container_of(d, struct sdio_func, dev)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define sdio_align_size LINUX_BACKPORT(sdio_align_size)
extern unsigned int sdio_align_size(struct sdio_func *func, unsigned int sz);
#endif /* 2.6.24 - 2.6.26 */

#endif /* __BACKPORT_MMC_SDIO_FUNC_H */
