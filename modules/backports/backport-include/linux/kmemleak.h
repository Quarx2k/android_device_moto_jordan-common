#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
#include_next <linux/kmemleak.h>
#else
/*
 * kmemleak was introduced on 2.6.31, since older kernels do not have
 * we simply ignore its tuning.
 */
static inline void kmemleak_ignore(const void *ptr)
{
	return;
}

static inline void kmemleak_not_leak(const void *ptr)
{
	return;
}

static inline void kmemleak_no_scan(const void *ptr)
{
	return;
}
#endif
