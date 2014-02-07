/*
 * Copyright 2012-2013  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Backport functionality introduced in Linux 3.5.
 */

#include <linux/module.h>
#include <linux/highuid.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
#include <linux/device.h>

/**
 * devres_release - Find a device resource and destroy it, calling release
 * @dev: Device to find resource from
 * @release: Look for resources associated with this release function
 * @match: Match function (optional)
 * @match_data: Data for the match function
 *
 * Find the latest devres of @dev associated with @release and for
 * which @match returns 1.  If @match is NULL, it's considered to
 * match all.  If found, the resource is removed atomically, the
 * release function called and the resource freed.
 *
 * RETURNS:
 * 0 if devres is found and freed, -ENOENT if not found.
 */
int devres_release(struct device *dev, dr_release_t release,
		   dr_match_t match, void *match_data)
{
	void *res;

	res = devres_remove(dev, release, match, match_data);
	if (unlikely(!res))
		return -ENOENT;

	(*release)(dev, res);
	devres_free(res);
	return 0;
}
EXPORT_SYMBOL_GPL(devres_release);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) */

/*
 * Commit 7a4e7408c5cadb240e068a662251754a562355e3
 * exported overflowuid and overflowgid for all
 * kernel configurations, prior to that we only
 * had it exported when CONFIG_UID16 was enabled.
 * We are technically redefining it here but
 * nothing seems to be changing it, except
 * kernel/ code does epose it via sysctl and
 * proc... if required later we can add that here.
 */
#ifndef CONFIG_UID16
int overflowuid = DEFAULT_OVERFLOWUID;
int overflowgid = DEFAULT_OVERFLOWGID;

EXPORT_SYMBOL_GPL(overflowuid);
EXPORT_SYMBOL_GPL(overflowgid);
#endif
