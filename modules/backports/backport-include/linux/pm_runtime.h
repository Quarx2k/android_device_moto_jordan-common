#include <linux/version.h>

#ifndef __COMPAT_LINUX_PM_RUNTIME_H
#define __COMPAT_LINUX_PM_RUNTIME_H

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
#include_next <linux/pm_runtime.h>
#else

static inline void pm_runtime_enable(struct device *dev) {}

#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
/*
 * Backports 5e928f77a09a07f9dd595bb8a489965d69a83458
 * run-time power management cannot really be backported
 * given that the implementation added bus specific
 * callbacks that we won't have on older kernels. If
 * you really want run-time power management or good
 * power management upgrade your kernel. We'll just
 * compile this out as if run-time power management was
 * disabled just as the kernel disables run-time power management
 * when CONFIG_PM_RUNTIME is disabled.
 */
static inline void pm_runtime_init(struct device *dev) {}
static inline void pm_runtime_remove(struct device *dev) {}
static inline int pm_runtime_get(struct device *dev)
{
	return 0;
}

static inline int pm_runtime_get_sync(struct device *dev)
{
	return 0;
}

static inline int pm_runtime_put(struct device *dev)
{
	return 0;
}

static inline int pm_runtime_put_sync(struct device *dev)
{
	return 0;
}

static inline int pm_runtime_set_active(struct device *dev)
{
	return 0;
}

static inline void pm_runtime_set_suspended(struct device *dev)
{
}

static inline void pm_runtime_disable(struct device *dev)
{
}

static inline void pm_runtime_put_noidle(struct device *dev) {}
static inline void pm_runtime_get_noresume(struct device *dev) {}
#endif

#endif
