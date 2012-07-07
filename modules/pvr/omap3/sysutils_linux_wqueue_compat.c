/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#include <linux/version.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <plat/omap-pm.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "sysinfo.h"
#include "sgxapi_km.h"
#include "sysconfig.h"
#include "sgxinfokm.h"
#include "syslocal.h"

#if !defined(PVR_LINUX_USING_WORKQUEUES)
#error "PVR_LINUX_USING_WORKQUEUES must be defined"
#endif

#define	ONE_MHZ	1000000
#define	HZ_TO_MHZ(m) ((m) / ONE_MHZ)

#if defined(SUPPORT_OMAP3430_SGXFCLK_96M)
#define SGX_PARENT_CLOCK "cm_96m_fck"
#else
#define SGX_PARENT_CLOCK "core_ck"
#endif

extern struct platform_device *gpsPVRLDMDev;
#if defined(SGX530) && (SGX_CORE_REV == 125)
#define OMAP_MEMORY_BUS_CLOCK_MAX 800000
#else
#define OMAP_MEMORY_BUS_CLOCK_MAX 664000
#endif
static IMG_VOID PowerLockWrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		BUG_ON(in_atomic());
		mutex_lock(&psSysSpecData->sPowerLock);
	}
}

static IMG_VOID PowerLockUnwrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		BUG_ON(in_atomic());
		mutex_unlock(&psSysSpecData->sPowerLock);
	}
}

PVRSRV_ERROR SysPowerLockWrap(IMG_BOOL bTryLock)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockWrap(psSysData->pvSysSpecificData);

	return PVRSRV_OK;
}

IMG_VOID SysPowerLockUnwrap(IMG_VOID)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockUnwrap(psSysData->pvSysSpecificData);
}

IMG_BOOL WrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	return IMG_TRUE;
}

IMG_VOID UnwrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
}

static inline IMG_UINT32 scale_by_rate(IMG_UINT32 val, IMG_UINT32 rate1, IMG_UINT32 rate2)
{
	if (rate1 >= rate2)
	{
		return val * (rate1 / rate2);
	}

	return val / (rate2 / rate1);
}

static inline IMG_UINT32 scale_prop_to_SGX_clock(IMG_UINT32 val, IMG_UINT32 rate)
{
	return scale_by_rate(val, rate, SYS_SGX_CLOCK_SPEED);
}

static inline IMG_UINT32 scale_inv_prop_to_SGX_clock(IMG_UINT32 val, IMG_UINT32 rate)
{
	return scale_by_rate(val, SYS_SGX_CLOCK_SPEED, rate);
}

IMG_VOID SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psTimingInfo)
{
	IMG_UINT32 rate;

#if defined(NO_HARDWARE)
	rate = SYS_SGX_CLOCK_SPEED;
#else
	PVR_ASSERT(atomic_read(&gpsSysSpecificData->sSGXClocksEnabled) != 0);

	rate = clk_get_rate(gpsSysSpecificData->psSGX_FCK);
	PVR_ASSERT(rate != 0);
#endif
	psTimingInfo->ui32CoreClockSpeed = rate;
	psTimingInfo->ui32HWRecoveryFreq = scale_prop_to_SGX_clock(SYS_SGX_HWRECOVERY_TIMEOUT_FREQ, rate);
	psTimingInfo->ui32uKernelFreq = scale_prop_to_SGX_clock(SYS_SGX_PDS_TIMER_FREQ, rate);
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = IMG_TRUE;
#else
	psTimingInfo->bEnableActivePM = IMG_FALSE;
#endif
	psTimingInfo->ui32ActivePowManLatencyms = SYS_SGX_ACTIVE_POWER_LATENCY_MS;
}

PVRSRV_ERROR EnableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	long lNewRate;
	long lRate;
	IMG_INT res;

	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) != 0)
	{
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: Enabling SGX Clocks"));

#if defined(DEBUG)
	{
		IMG_UINT32 rate = clk_get_rate(psSysSpecData->psMPU_CK);
		PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: CPU Clock is %dMhz", HZ_TO_MHZ(rate)));
	}
#endif

	res = clk_enable(psSysSpecData->psSGX_FCK);
	if (res < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: Couldn't enable SGX functional clock (%d)", res));
		return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
	}

	res = clk_enable(psSysSpecData->psSGX_ICK);
	if (res < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: Couldn't enable SGX interface clock (%d)", res));

		clk_disable(psSysSpecData->psSGX_FCK);
		return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
	}

	lNewRate = clk_round_rate(psSysSpecData->psSGX_FCK, SYS_SGX_CLOCK_SPEED + ONE_MHZ);
	if (lNewRate <= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: Couldn't round SGX functional clock rate"));
		return PVRSRV_ERROR_UNABLE_TO_ROUND_CLOCK_RATE;
	}

	lRate = clk_get_rate(psSysSpecData->psSGX_FCK);
	if (lRate != lNewRate)
	{
		res = clk_set_rate(psSysSpecData->psSGX_FCK, lNewRate);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_WARNING, "EnableSGXClocks: Couldn't set SGX functional clock rate (%d)", res));
		}
	}

#if defined(DEBUG)
	{
		IMG_UINT32 rate = clk_get_rate(psSysSpecData->psSGX_FCK);
		PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: SGX Functional Clock is %dMhz", HZ_TO_MHZ(rate)));
	}
#endif

#if defined(SYS_OMAP3430_PIN_MEMORY_BUS_CLOCK)
	omap_pm_set_min_bus_tput(&gpsPVRLDMDev->dev, OCP_INITIATOR_AGENT, OMAP_MEMORY_BUS_CLOCK_MAX);
#endif


	atomic_set(&psSysSpecData->sSGXClocksEnabled, 1);

#else	/* !defined(NO_HARDWARE) */
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	/* !defined(NO_HARDWARE) */
	return PVRSRV_OK;
}


IMG_VOID DisableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) == 0)
	{
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "DisableSGXClocks: Disabling SGX Clocks"));

	if (psSysSpecData->psSGX_ICK)
	{
		clk_disable(psSysSpecData->psSGX_ICK);
	}

	if (psSysSpecData->psSGX_FCK)
	{
		clk_disable(psSysSpecData->psSGX_FCK);
	}

#if defined(SYS_OMAP3430_PIN_MEMORY_BUS_CLOCK)
	omap_pm_set_min_bus_tput(&gpsPVRLDMDev->dev, OCP_INITIATOR_AGENT, 0);
#endif

	atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

#else
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif
}

PVRSRV_ERROR EnableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	struct clk *psCLK;
	IMG_INT res;
	PVRSRV_ERROR eError;

#if defined(DEBUG) || defined(TIMING)
	IMG_INT rate;
	struct clk *sys_ck;
	IMG_CPU_PHYADDR     TimerRegPhysBase;
	IMG_HANDLE hTimerEnable;
	IMG_UINT32 *pui32TimerEnable;

#endif

	PVR_TRACE(("EnableSystemClocks: Enabling System Clocks"));

	if (!psSysSpecData->bSysClocksOneTimeInit)
	{
		mutex_init(&psSysSpecData->sPowerLock);

		atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

		psCLK = clk_get(NULL, SGX_PARENT_CLOCK);
		if (IS_ERR(psCLK))
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSsystemClocks: Couldn't get Core Clock"));
			goto ExitError;
		}
		psSysSpecData->psCORE_CK = psCLK;

		psCLK = clk_get(NULL, "sgx_fck");
		if (IS_ERR(psCLK))
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSsystemClocks: Couldn't get SGX Functional Clock"));
			goto ExitError;
		}
		psSysSpecData->psSGX_FCK = psCLK;

		psCLK = clk_get(NULL, "sgx_ick");
		if (IS_ERR(psCLK))
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't get SGX Interface Clock"));
			goto ExitError;
		}
		psSysSpecData->psSGX_ICK = psCLK;

#if defined(DEBUG)
		psCLK = clk_get(NULL, "mpu_ck");
		if (IS_ERR(psCLK))
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't get MPU Clock"));
			goto ExitError;
		}
		psSysSpecData->psMPU_CK = psCLK;
#endif
		res = clk_set_parent(psSysSpecData->psSGX_FCK, psSysSpecData->psCORE_CK);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't set SGX parent clock (%d)", res));
			goto ExitError;
		}

		psSysSpecData->bSysClocksOneTimeInit = IMG_TRUE;
	}

#if defined(DEBUG) || defined(TIMING)

	psCLK = clk_get(NULL, "gpt11_fck");
	if (IS_ERR(psCLK))
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't get GPTIMER11 functional clock"));
		goto ExitUnRegisterConstraintNotifications;
	}
	psSysSpecData->psGPT11_FCK = psCLK;

	psCLK = clk_get(NULL, "gpt11_ick");
	if (IS_ERR(psCLK))
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't get GPTIMER11 interface clock"));
		goto ExitUnRegisterConstraintNotifications;
	}
	psSysSpecData->psGPT11_ICK = psCLK;

	sys_ck = clk_get(NULL, "sys_ck");
	if (IS_ERR(sys_ck))
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't get System clock"));
		goto ExitUnRegisterConstraintNotifications;
	}

	if(clk_get_parent(psSysSpecData->psGPT11_FCK) != sys_ck)
	{
		PVR_TRACE(("Setting GPTIMER11 parent to System Clock"));
		res = clk_set_parent(psSysSpecData->psGPT11_FCK, sys_ck);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't set GPTIMER11 parent clock (%d)", res));
		goto ExitUnRegisterConstraintNotifications;
		}
	}

	rate = clk_get_rate(psSysSpecData->psGPT11_FCK);
	PVR_TRACE(("GPTIMER11 clock is %dMHz", HZ_TO_MHZ(rate)));

	res = clk_enable(psSysSpecData->psGPT11_FCK);
	if (res < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't enable GPTIMER11 functional clock (%d)", res));
		goto ExitUnRegisterConstraintNotifications;
	}

	res = clk_enable(psSysSpecData->psGPT11_ICK);
	if (res < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: Couldn't enable GPTIMER11 interface clock (%d)", res));
		goto ExitDisableGPT11FCK;
	}


	TimerRegPhysBase.uiAddr = SYS_OMAP3430_GP11TIMER_TSICR_SYS_PHYS_BASE;
	pui32TimerEnable = OSMapPhysToLin(TimerRegPhysBase,
                  4,
                  PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
                  &hTimerEnable);

	if (pui32TimerEnable == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: OSMapPhysToLin failed"));
		goto ExitDisableGPT11ICK;
	}

	rate = *pui32TimerEnable;
	if(!(rate & 4))
	{
		PVR_TRACE(("Setting GPTIMER11 mode to posted (currently is non-posted)"));


		*pui32TimerEnable = rate | 4;
	}

	OSUnMapPhysToLin(pui32TimerEnable,
		    4,
		    PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
		    hTimerEnable);


	TimerRegPhysBase.uiAddr = SYS_OMAP3430_GP11TIMER_ENABLE_SYS_PHYS_BASE;
	pui32TimerEnable = OSMapPhysToLin(TimerRegPhysBase,
                  4,
                  PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
                  &hTimerEnable);

	if (pui32TimerEnable == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSystemClocks: OSMapPhysToLin failed"));
		goto ExitDisableGPT11ICK;
	}


	*pui32TimerEnable = 3;

	OSUnMapPhysToLin(pui32TimerEnable,
		    4,
		    PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
		    hTimerEnable);

#endif

	eError = PVRSRV_OK;
	goto Exit;

#if defined(DEBUG) || defined(TIMING)
ExitDisableGPT11ICK:
	clk_disable(psSysSpecData->psGPT11_ICK);
ExitDisableGPT11FCK:
	clk_disable(psSysSpecData->psGPT11_FCK);
ExitUnRegisterConstraintNotifications:
#endif
ExitError:
	eError = PVRSRV_ERROR_DISABLE_CLOCK_FAILURE;
Exit:
	return eError;
}

IMG_VOID DisableSystemClocks(SYS_DATA *psSysData)
{
#if defined(DEBUG) || defined(TIMING)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	IMG_CPU_PHYADDR TimerRegPhysBase;
	IMG_HANDLE hTimerDisable;
	IMG_UINT32 *pui32TimerDisable;
#endif

	PVR_TRACE(("DisableSystemClocks: Disabling System Clocks"));


	DisableSGXClocks(psSysData);

#if defined(DEBUG) || defined(TIMING)

	TimerRegPhysBase.uiAddr = SYS_OMAP3430_GP11TIMER_ENABLE_SYS_PHYS_BASE;
	pui32TimerDisable = OSMapPhysToLin(TimerRegPhysBase,
				4,
				PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
				&hTimerDisable);

	if (pui32TimerDisable == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "DisableSystemClocks: OSMapPhysToLin failed"));
	}
	else
	{
		*pui32TimerDisable = 0;

		OSUnMapPhysToLin(pui32TimerDisable,
				4,
				PVRSRV_HAP_KERNEL_ONLY|PVRSRV_HAP_UNCACHED,
				hTimerDisable);
	}

	clk_disable(psSysSpecData->psGPT11_ICK);

	clk_disable(psSysSpecData->psGPT11_FCK);

#endif
}
