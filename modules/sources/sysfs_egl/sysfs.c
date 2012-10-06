
#define TAG "pvrsysfs"

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <asm/page.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "services_headers.h"
#include "pdump_km.h"
#include "pvr_debug.h"
#include "sgxdefs.h"

static struct kobject* pvr_sysfs_object = NULL;

/* sysfs structures */
struct pvrsrv_attribute {
	struct attribute attr;
	int sgx_version;
	int sgx_revision;
};

static struct pvrsrv_attribute PVRSRVAttr = {
	.attr.name = "egl.cfg",
	.attr.mode = S_IRUGO,
	.sgx_version = 530,
	.sgx_revision = SGX_CORE_REV,
};

/* sysfs read function */
static ssize_t PVRSRVEglCfgShow(struct kobject *kobj, struct attribute *attr,
								char *buffer) {
	struct pvrsrv_attribute *pvrsrv_attr;

	pvrsrv_attr = container_of(attr, struct pvrsrv_attribute, attr);
#ifdef SGX_CORE_FRIENDLY_NAME
	return snprintf(buffer, PAGE_SIZE, "0 0 android\n0 1 POWERVR_" SGX_CORE_FRIENDLY_NAME "_%d\n",
			pvrsrv_attr->sgx_revision);
#else
	return snprintf(buffer, PAGE_SIZE, "0 0 android\n0 1 POWERVR_SGX%d_%d\n",
			pvrsrv_attr->sgx_version, pvrsrv_attr->sgx_revision);
#endif
}

/* sysfs write function unsupported*/
static ssize_t PVRSRVEglCfgStore(struct kobject *kobj, struct attribute *attr,
					const char *buffer, size_t size) {
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVEglCfgStore not implemented"));
	return 0;
}

static struct attribute *pvrsrv_sysfs_attrs[] = {
	&PVRSRVAttr.attr,
	NULL
};

static struct sysfs_ops pvrsrv_sysfs_ops = {
	.show = PVRSRVEglCfgShow,
	.store = PVRSRVEglCfgStore,
};

static struct kobj_type pvrsrv_ktype = {
	.sysfs_ops = &pvrsrv_sysfs_ops,
	.default_attrs = pvrsrv_sysfs_attrs,
};

/* create sysfs entry /sys/egl/egl.cfg to determine
   which gfx libraries to load */

int PVRSRVCreateSysfsEntry(void)
{
	struct kobject *egl_cfg_kobject;
	int r;

	egl_cfg_kobject = kzalloc(sizeof(*egl_cfg_kobject), GFP_KERNEL);
	r = kobject_init_and_add(egl_cfg_kobject, &pvrsrv_ktype, NULL, "egl");

	if (r) {
		PVR_DPF((PVR_DBG_ERROR,
			"Failed to create egl.cfg sysfs entry"));
		return PVRSRV_ERROR_INIT_FAILURE;
	}
	pvr_sysfs_object = egl_cfg_kobject;

	return PVRSRV_OK;
}

static int __init pvrsysfs_init(void) {
	int ret = PVRSRVCreateSysfsEntry();
	return ret;
}

static void __exit pvrsysfs_exit(void) {
	if (pvr_sysfs_object != NULL) {
		kobject_put(pvr_sysfs_object);
		pvr_sysfs_object = NULL;
	}
}

module_init(pvrsysfs_init);
module_exit(pvrsysfs_exit);

MODULE_ALIAS(TAG);
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Export egl.cfg in sysfs (omapzoom backport)");
MODULE_AUTHOR("Tanguy Pruvot, omapzoom");
MODULE_LICENSE("GPL");

