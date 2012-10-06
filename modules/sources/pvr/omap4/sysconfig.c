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

#include "services_headers.h"
#include "kerneldisplay.h"
#include "oemfuncs.h"
#include "sgxinfo.h"
#include "sgxinfokm.h"
#include "syslocal.h"
#include "sysconfig.h"

#include "ocpdefs.h"

#if !defined(NO_HARDWARE) && \
     defined(SYS_USING_INTERRUPTS) && \
     defined(SGX540)
#define SGX_OCP_REGS_ENABLED
#endif

SYS_DATA* gpsSysData = (SYS_DATA*)IMG_NULL;
SYS_DATA  gsSysData;

static SYS_SPECIFIC_DATA gsSysSpecificData;
SYS_SPECIFIC_DATA *gpsSysSpecificData;

static IMG_UINT32	gui32SGXDeviceID;
static SGX_DEVICE_MAP	gsSGXDeviceMap;
static PVRSRV_DEVICE_NODE *gpsSGXDevNode;

#define DEVICE_SGX_INTERRUPT (1 << 0)

#if defined(NO_HARDWARE)
static IMG_CPU_VIRTADDR gsSGXRegsCPUVAddr;
#endif

IMG_UINT32 PVRSRV_BridgeDispatchKM(IMG_UINT32	Ioctl,
								   IMG_BYTE		*pInBuf,
								   IMG_UINT32	InBufLen,
								   IMG_BYTE		*pOutBuf,
								   IMG_UINT32	OutBufLen,
								   IMG_UINT32	*pdwBytesTransferred);

#if defined(DEBUG) && defined(DUMP_OMAP34xx_CLOCKS) && defined(__linux__)

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <mach/clock.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
#include <../mach-omap2/clock_34xx.h>
#define ONCHIP_CLKS onchip_clks
#else
#include <../mach-omap2/clock34xx.h>
#define ONCHIP_CLKS onchip_34xx_clks
#endif

static void omap3_clk_recalc(struct clk *clk) {}
static void omap3_followparent_recalc(struct clk *clk) {}
static void omap3_propagate_rate(struct clk *clk) {}
static void omap3_table_recalc(struct clk *clk) {}
static long omap3_round_to_table_rate(struct clk *clk, unsigned long rate) { return 0; }
static int omap3_select_table_rate(struct clk *clk, unsigned long rate) { return 0; }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
static void omap3_dpll_recalc(struct clk *clk, unsigned long parent_rate,
							  u8 rate_storage) {}
static void omap3_clkoutx2_recalc(struct clk *clk, unsigned long parent_rate,
								  u8 rate_storage) {}
static void omap3_dpll_allow_idle(struct clk *clk) {}
static void omap3_dpll_deny_idle(struct clk *clk) {}
static u32 omap3_dpll_autoidle_read(struct clk *clk) { return 0; }
static int omap3_noncore_dpll_enable(struct clk *clk) { return 0; }
static void omap3_noncore_dpll_disable(struct clk *clk) {}
static int omap3_noncore_dpll_set_rate(struct clk *clk, unsigned long rate) { return 0; }
static int omap3_core_dpll_m2_set_rate(struct clk *clk, unsigned long rate) { return 0; }
void followparent_recalc(struct clk *clk, unsigned long new_parent_rate,
								u8 rate_storage) {}
long omap2_dpll_round_rate(struct clk *clk, unsigned long target_rate) { return 0; }
void omap2_clksel_recalc(struct clk *clk, unsigned long new_parent_rate,
								u8 rate_storage) {}
long omap2_clksel_round_rate(struct clk *clk, unsigned long target_rate) { return 0; }
int omap2_clksel_set_rate(struct clk *clk, unsigned long rate) { return 0; }
void omap2_fixed_divisor_recalc(struct clk *clk, unsigned long new_parent_rate,
									   u8 rate_storage) {}
void omap2_init_clksel_parent(struct clk *clk) {}
#endif

static void dump_omap34xx_clocks(void)
{
	struct clk **c;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
	struct vdd_prcm_config *t1 = vdd1_rate_table;
	struct vdd_prcm_config *t2 = vdd2_rate_table;

	t1 = t1;
	t2 = t2;
#else

	omap3_dpll_allow_idle(0);
	omap3_dpll_deny_idle(0);
	omap3_dpll_autoidle_read(0);
	omap3_clk_recalc(0);
	omap3_followparent_recalc(0);
	omap3_propagate_rate(0);
	omap3_table_recalc(0);
	omap3_round_to_table_rate(0, 0);
	omap3_select_table_rate(0, 0);
#endif

	for(c = ONCHIP_CLKS; c < ONCHIP_CLKS + ARRAY_SIZE(ONCHIP_CLKS); c++)
	{
		struct clk *cp = *c, *copy;
		unsigned long rate;
		copy = clk_get(NULL, cp->name);
		if(!copy)
			continue;
		rate = clk_get_rate(copy);
		if (rate < 1000000)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: clock %s is %lu KHz (%lu Hz)", __func__, cp->name, rate/1000, rate));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: clock %s is %lu MHz (%lu Hz)", __func__, cp->name, rate/1000000, rate));
		}
	}
}

#else

static INLINE void dump_omap34xx_clocks(void) {}

#endif

#if defined(SGX_OCP_REGS_ENABLED)

#define SYS_OMAP4430_OCP_REGS_SYS_PHYS_BASE		(SYS_OMAP4430_SGX_REGS_SYS_PHYS_BASE + EUR_CR_OCP_REVISION)
#define SYS_OMAP4430_OCP_REGS_SIZE				0x110

static IMG_CPU_VIRTADDR gpvOCPRegsLinAddr;

static PVRSRV_ERROR EnableSGXClocksWrap(SYS_DATA *psSysData)
{
	PVRSRV_ERROR eError = EnableSGXClocks(psSysData);

	if(eError == PVRSRV_OK)
	{
		OSWriteHWReg(gpvOCPRegsLinAddr,
					 EUR_CR_OCP_DEBUG_CONFIG - EUR_CR_OCP_REVISION,
					 EUR_CR_OCP_DEBUG_CONFIG_THALIA_INT_BYPASS_MASK);
	}

	return eError;
}

#else

static INLINE PVRSRV_ERROR EnableSGXClocksWrap(SYS_DATA *psSysData)
{
	return EnableSGXClocks(psSysData);
}

#endif

static INLINE PVRSRV_ERROR EnableSystemClocksWrap(SYS_DATA *psSysData)
{
	PVRSRV_ERROR eError = EnableSystemClocks(psSysData);

#if !defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	if(eError == PVRSRV_OK)
	{

		EnableSGXClocksWrap(psSysData);
	}
#endif

	return eError;
}

static PVRSRV_ERROR SysLocateDevices(SYS_DATA *psSysData)
{
#if defined(NO_HARDWARE)
	PVRSRV_ERROR eError;
	IMG_CPU_PHYADDR sCpuPAddr;
#endif

	PVR_UNREFERENCED_PARAMETER(psSysData);


	gsSGXDeviceMap.ui32Flags = 0x0;

#if defined(NO_HARDWARE)


	eError = OSBaseAllocContigMemory(SYS_OMAP4430_SGX_REGS_SIZE,
									 &gsSGXRegsCPUVAddr,
									 &sCpuPAddr);
	if(eError != PVRSRV_OK)
	{
		return eError;
	}
	gsSGXDeviceMap.sRegsCpuPBase = sCpuPAddr;
	gsSGXDeviceMap.sRegsSysPBase = SysCpuPAddrToSysPAddr(gsSGXDeviceMap.sRegsCpuPBase);
	gsSGXDeviceMap.ui32RegsSize = SYS_OMAP4430_SGX_REGS_SIZE;
#if defined(__linux__)

	gsSGXDeviceMap.pvRegsCpuVBase = gsSGXRegsCPUVAddr;
#else

	gsSGXDeviceMap.pvRegsCpuVBase = IMG_NULL;
#endif

	OSMemSet(gsSGXRegsCPUVAddr, 0, SYS_OMAP4430_SGX_REGS_SIZE);




	gsSGXDeviceMap.ui32IRQ = 0;

#else

	gsSGXDeviceMap.sRegsSysPBase.uiAddr = SYS_OMAP4430_SGX_REGS_SYS_PHYS_BASE;
	gsSGXDeviceMap.sRegsCpuPBase = SysSysPAddrToCpuPAddr(gsSGXDeviceMap.sRegsSysPBase);
	gsSGXDeviceMap.ui32RegsSize = SYS_OMAP4430_SGX_REGS_SIZE;

	gsSGXDeviceMap.ui32IRQ = SYS_OMAP4430_SGX_IRQ;

#endif 

#if defined(PDUMP)
	{
		
		static IMG_CHAR pszPDumpDevName[] = "SGXMEM";
		gsSGXDeviceMap.pszPDumpDevName = pszPDumpDevName;
	}
#endif





	return PVRSRV_OK;
}


IMG_CHAR *SysCreateVersionString(IMG_CPU_PHYADDR sRegRegion)
{
	static IMG_CHAR aszVersionString[100];
	SYS_DATA	*psSysData;
	IMG_UINT32	ui32SGXRevision;
	IMG_INT32	i32Count;
#if !defined(NO_HARDWARE)
	IMG_VOID	*pvRegsLinAddr;

	pvRegsLinAddr = OSMapPhysToLin(sRegRegion,
								   SYS_OMAP4430_SGX_REGS_SIZE,
								   PVRSRV_HAP_UNCACHED|PVRSRV_HAP_KERNEL_ONLY,
								   IMG_NULL);
	if(!pvRegsLinAddr)
	{
		return IMG_NULL;
	}

	ui32SGXRevision = OSReadHWReg((IMG_PVOID)((IMG_PBYTE)pvRegsLinAddr),
								  EUR_CR_CORE_REVISION);
#else
	ui32SGXRevision = 0;
#endif

	SysAcquireData(&psSysData);

	i32Count = OSSNPrintf(aszVersionString, 100,
						  "SGX revision = %u.%u.%u",
						  (IMG_UINT)((ui32SGXRevision & EUR_CR_CORE_REVISION_MAJOR_MASK)
							>> EUR_CR_CORE_REVISION_MAJOR_SHIFT),
						  (IMG_UINT)((ui32SGXRevision & EUR_CR_CORE_REVISION_MINOR_MASK)
							>> EUR_CR_CORE_REVISION_MINOR_SHIFT),
						  (IMG_UINT)((ui32SGXRevision & EUR_CR_CORE_REVISION_MAINTENANCE_MASK)
							>> EUR_CR_CORE_REVISION_MAINTENANCE_SHIFT)
						 );

#if !defined(NO_HARDWARE)
	OSUnMapPhysToLin(pvRegsLinAddr,
					 SYS_OMAP4430_SGX_REGS_SIZE,
					 PVRSRV_HAP_UNCACHED|PVRSRV_HAP_KERNEL_ONLY,
					 IMG_NULL);
#endif

	if(i32Count == -1)
	{
		return IMG_NULL;
	}

	return aszVersionString;
}


PVRSRV_ERROR SysInitialise(IMG_VOID)
{
	IMG_UINT32			i;
	PVRSRV_ERROR 		eError;
	PVRSRV_DEVICE_NODE	*psDeviceNode;
#if !defined(NO_OMAP_TIMER)
	IMG_CPU_PHYADDR		TimerRegPhysBase;
#endif
#if !defined(SGX_DYNAMIC_TIMING_INFO)
	SGX_TIMING_INFORMATION*	psTimingInfo;
#endif
	gpsSysData = &gsSysData;
	OSMemSet(gpsSysData, 0, sizeof(SYS_DATA));

	gpsSysSpecificData =  &gsSysSpecificData;
	OSMemSet(gpsSysSpecificData, 0, sizeof(SYS_SPECIFIC_DATA));

	gpsSysData->pvSysSpecificData = gpsSysSpecificData;

	eError = OSInitEnvData(&gpsSysData->pvEnvSpecificData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to setup env structure"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_ENVDATA);

	gpsSysData->ui32NumDevices = SYS_DEVICE_COUNT;


	for(i=0; i<SYS_DEVICE_COUNT; i++)
	{
		gpsSysData->sDeviceID[i].uiID = i;
		gpsSysData->sDeviceID[i].bInUse = IMG_FALSE;
	}

	gpsSysData->psDeviceNodeList = IMG_NULL;
	gpsSysData->psQueueList = IMG_NULL;

	eError = SysInitialiseCommon(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed in SysInitialiseCommon"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}

#if !defined(NO_OMAP_TIMER)
	TimerRegPhysBase.uiAddr = SYS_OMAP3430_GP11TIMER_REGS_SYS_PHYS_BASE;
	gpsSysData->pvSOCTimerRegisterKM = IMG_NULL;
	gpsSysData->hSOCTimerRegisterOSMemHandle = 0;
	OSReservePhys(TimerRegPhysBase,
				  4,
				  PVRSRV_HAP_MULTI_PROCESS|PVRSRV_HAP_UNCACHED,
				  (IMG_VOID **)&gpsSysData->pvSOCTimerRegisterKM,
				  &gpsSysData->hSOCTimerRegisterOSMemHandle);
#endif

#if !defined(SGX_DYNAMIC_TIMING_INFO)

	psTimingInfo = &gsSGXDeviceMap.sTimingInfo;
	psTimingInfo->ui32CoreClockSpeed = SYS_SGX_CLOCK_SPEED;
	psTimingInfo->ui32HWRecoveryFreq = SYS_SGX_HWRECOVERY_TIMEOUT_FREQ;
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = IMG_TRUE;
#else
	psTimingInfo->bEnableActivePM = IMG_FALSE;
#endif
	psTimingInfo->ui32ActivePowManLatencyms = SYS_SGX_ACTIVE_POWER_LATENCY_MS;
	psTimingInfo->ui32uKernelFreq = SYS_SGX_PDS_TIMER_FREQ;
#endif



	gpsSysSpecificData->ui32SrcClockDiv = 3;





	eError = SysLocateDevices(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to locate devices"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LOCATEDEV);

#if defined(SGX_OCP_REGS_ENABLED)
	{
		IMG_SYS_PHYADDR sOCPRegsSysPBase;
		IMG_CPU_PHYADDR sOCPRegsCpuPBase;

		sOCPRegsSysPBase.uiAddr	= SYS_OMAP4430_OCP_REGS_SYS_PHYS_BASE;
		sOCPRegsCpuPBase		= SysSysPAddrToCpuPAddr(sOCPRegsSysPBase);

		gpvOCPRegsLinAddr		= OSMapPhysToLin(sOCPRegsCpuPBase,
												 SYS_OMAP4430_OCP_REGS_SIZE,
												 PVRSRV_HAP_UNCACHED|PVRSRV_HAP_KERNEL_ONLY,
												 IMG_NULL);

		if (gpvOCPRegsLinAddr == IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to map OCP registers"));
			return PVRSRV_ERROR_BAD_MAPPING;
		}
	}
#endif




	eError = PVRSRVRegisterDevice(gpsSysData, SGXRegisterDevice,
								  DEVICE_SGX_INTERRUPT, &gui32SGXDeviceID);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to register device!"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_REGDEV);




	psDeviceNode = gpsSysData->psDeviceNodeList;
	while(psDeviceNode)
	{

		switch(psDeviceNode->sDevId.eDeviceType)
		{
			case PVRSRV_DEVICE_TYPE_SGX:
			{
				DEVICE_MEMORY_INFO *psDevMemoryInfo;
				DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;




				psDeviceNode->psLocalDevMemArena = IMG_NULL;


				psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
				psDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;


				for(i=0; i<psDevMemoryInfo->ui32HeapCount; i++)
				{
					psDeviceMemoryHeap[i].ui32Attribs |= PVRSRV_BACKINGSTORE_SYSMEM_NONCONTIG;
				}

				gpsSGXDevNode = psDeviceNode;
				gsSysSpecificData.psSGXDevNode = psDeviceNode;

				break;
			}
			default:
				PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to find SGX device node!"));
				return PVRSRV_ERROR_INIT_FAILURE;
		}


		psDeviceNode = psDeviceNode->psNext;
	}

	eError = EnableSystemClocksWrap(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to Enable system clocks (%d)", eError));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS);
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	eError = EnableSGXClocksWrap(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to Enable SGX clocks (%d)", eError));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
#endif

	dump_omap34xx_clocks();

	eError = PVRSRVInitialiseDevice(gui32SGXDeviceID);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to initialise device!"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_INITDEV);

#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)

	DisableSGXClocks(gpsSysData);
#endif

	return PVRSRV_OK;
}


PVRSRV_ERROR SysFinalise(IMG_VOID)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	eError = EnableSGXClocksWrap(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysInitialise: Failed to Enable SGX clocks (%d)", eError));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
#endif

#if defined(SYS_USING_INTERRUPTS)

	eError = OSInstallMISR(gpsSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysFinalise: Failed to install MISR"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_MISR);


	eError = OSInstallDeviceLISR(gpsSysData, gsSGXDeviceMap.ui32IRQ, "SGX ISR", gpsSGXDevNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysFinalise: Failed to install ISR"));
		(IMG_VOID)SysDeinitialise(gpsSysData);
		gpsSysData = IMG_NULL;
		return eError;
	}
	SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LISR);
#endif


	gpsSysData->pszVersionString = SysCreateVersionString(gsSGXDeviceMap.sRegsCpuPBase);
	if (!gpsSysData->pszVersionString)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysFinalise: Failed to create a system version string"));
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING, "SysFinalise: Version string: %s", gpsSysData->pszVersionString));
	}

#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)

	DisableSGXClocks(gpsSysData);
#endif

	gpsSysSpecificData->bSGXInitComplete = IMG_TRUE;

	return eError;
}


PVRSRV_ERROR SysDeinitialise (SYS_DATA *psSysData)
{
	PVRSRV_ERROR eError;

#if defined(SYS_USING_INTERRUPTS)
	if (SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LISR))
	{
		eError = OSUninstallDeviceLISR(psSysData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysDeinitialise: OSUninstallDeviceLISR failed"));
			return eError;
		}
	}

	if (SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_MISR))
	{
		eError = OSUninstallMISR(psSysData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysDeinitialise: OSUninstallMISR failed"));
			return eError;
		}
	}
#else
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif

	if (SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_INITDEV))
	{
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
		PVR_ASSERT(SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS));
		
		eError = EnableSGXClocksWrap(gpsSysData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysDeinitialise: EnableSGXClocks failed"));
			return eError;
		}
#endif	

		
		eError = PVRSRVDeinitialiseDevice (gui32SGXDeviceID);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysDeinitialise: failed to de-init the device"));
			return eError;
		}
	}
	
#if defined(SGX_OCP_REGS_ENABLED)
	OSUnMapPhysToLin(gpvOCPRegsLinAddr,
					 SYS_OMAP4430_OCP_REGS_SIZE,
					 PVRSRV_HAP_UNCACHED|PVRSRV_HAP_KERNEL_ONLY,
					 IMG_NULL);
#endif

	

	if (SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS))
	{
		DisableSystemClocks(gpsSysData);
	}

	if (SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_ENVDATA))
	{
		eError = OSDeInitEnvData(gpsSysData->pvEnvSpecificData);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"SysDeinitialise: failed to de-init env structure"));
			return eError;
		}
	}

#if !defined(NO_OMAP_TIMER)
	if(gpsSysData->pvSOCTimerRegisterKM)
	{
		OSUnReservePhys(gpsSysData->pvSOCTimerRegisterKM,
						4,
						PVRSRV_HAP_MULTI_PROCESS|PVRSRV_HAP_UNCACHED,
						gpsSysData->hSOCTimerRegisterOSMemHandle);
	}
#endif

	SysDeinitialiseCommon(gpsSysData);

#if defined(NO_HARDWARE)
	if(SYS_SPECIFIC_DATA_TEST(gpsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LOCATEDEV))
	{

		OSBaseFreeContigMemory(SYS_OMAP4430_SGX_REGS_SIZE, gsSGXRegsCPUVAddr, gsSGXDeviceMap.sRegsCpuPBase);
	}
#endif

	gpsSysSpecificData->ui32SysSpecificData = 0;
	gpsSysSpecificData->bSGXInitComplete = IMG_FALSE;

	gpsSysData = IMG_NULL;

	return PVRSRV_OK;
}


PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE	eDeviceType,
								   IMG_VOID				**ppvDeviceMap)
{

	switch(eDeviceType)
	{
		case PVRSRV_DEVICE_TYPE_SGX:
		{

			*ppvDeviceMap = (IMG_VOID*)&gsSGXDeviceMap;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SysGetDeviceMemoryMap: unsupported device type"));
		}
	}
	return PVRSRV_OK;
}


IMG_DEV_PHYADDR SysCpuPAddrToDevPAddr(PVRSRV_DEVICE_TYPE	eDeviceType,
									  IMG_CPU_PHYADDR		CpuPAddr)
{
	IMG_DEV_PHYADDR DevPAddr;

	PVR_UNREFERENCED_PARAMETER(eDeviceType);


	DevPAddr.uiAddr = CpuPAddr.uiAddr;

	return DevPAddr;
}

IMG_CPU_PHYADDR SysSysPAddrToCpuPAddr (IMG_SYS_PHYADDR sys_paddr)
{
	IMG_CPU_PHYADDR cpu_paddr;


	cpu_paddr.uiAddr = sys_paddr.uiAddr;
	return cpu_paddr;
}

IMG_SYS_PHYADDR SysCpuPAddrToSysPAddr (IMG_CPU_PHYADDR cpu_paddr)
{
	IMG_SYS_PHYADDR sys_paddr;


	sys_paddr.uiAddr = cpu_paddr.uiAddr;
	return sys_paddr;
}


IMG_DEV_PHYADDR SysSysPAddrToDevPAddr(PVRSRV_DEVICE_TYPE eDeviceType, IMG_SYS_PHYADDR SysPAddr)
{
	IMG_DEV_PHYADDR DevPAddr;

	PVR_UNREFERENCED_PARAMETER(eDeviceType);


	DevPAddr.uiAddr = SysPAddr.uiAddr;

	return DevPAddr;
}


IMG_SYS_PHYADDR SysDevPAddrToSysPAddr(PVRSRV_DEVICE_TYPE eDeviceType, IMG_DEV_PHYADDR DevPAddr)
{
	IMG_SYS_PHYADDR SysPAddr;

	PVR_UNREFERENCED_PARAMETER(eDeviceType);


	SysPAddr.uiAddr = DevPAddr.uiAddr;

	return SysPAddr;
}


IMG_VOID SysRegisterExternalDevice(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
}


IMG_VOID SysRemoveExternalDevice(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
}


IMG_UINT32 SysGetInterruptSource(SYS_DATA			*psSysData,
								 PVRSRV_DEVICE_NODE	*psDeviceNode)
{
	PVR_UNREFERENCED_PARAMETER(psSysData);
#if defined(NO_HARDWARE)

	return 0xFFFFFFFF;
#else

	return psDeviceNode->ui32SOCInterruptBit;
#endif
}


IMG_VOID SysClearInterrupts(SYS_DATA* psSysData, IMG_UINT32 ui32ClearBits)
{
	PVR_UNREFERENCED_PARAMETER(psSysData);
	PVR_UNREFERENCED_PARAMETER(ui32ClearBits);


	OSReadHWReg(((PVRSRV_SGXDEV_INFO *)gpsSGXDevNode->pvDevice)->pvRegsBaseKM,
										EUR_CR_EVENT_HOST_CLEAR);
}


PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_D3)
	{
		PVR_TRACE(("SysSystemPrePowerState: Entering state D3"));

#if defined(SYS_USING_INTERRUPTS)
		if (SYS_SPECIFIC_DATA_TEST(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LISR))
		{
#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
			IMG_BOOL bWrapped = WrapSystemPowerChange(&gsSysSpecificData);
#endif
			eError = OSUninstallDeviceLISR(gpsSysData);
#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
			if (bWrapped)
			{
				UnwrapSystemPowerChange(&gsSysSpecificData);
			}
#endif
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"SysSystemPrePowerState: OSUninstallDeviceLISR failed (%d)", eError));
				return eError;
			}
			SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR);
			SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LISR);
		}
#endif

		if (SYS_SPECIFIC_DATA_TEST(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS))
		{
			DisableSystemClocks(gpsSysData);

			SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_DISABLE_SYSCLOCKS);
			SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS);
		}
	}

	return eError;
}


PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_D0)
	{
		PVR_TRACE(("SysSystemPostPowerState: Entering state D0"));

		if (SYS_SPECIFIC_DATA_TEST(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_DISABLE_SYSCLOCKS))
		{
			eError = EnableSystemClocksWrap(gpsSysData);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"SysSystemPostPowerState: EnableSystemClocksWrap failed (%d)", eError));
				return eError;
			}
			SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_SYSCLOCKS);
			SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_DISABLE_SYSCLOCKS);
		}

#if defined(SYS_USING_INTERRUPTS)
		if (SYS_SPECIFIC_DATA_TEST(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR))
		{
#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
			IMG_BOOL bWrapped = WrapSystemPowerChange(&gsSysSpecificData);
#endif

			eError = OSInstallDeviceLISR(gpsSysData, gsSGXDeviceMap.ui32IRQ, "SGX ISR", gpsSGXDevNode);
#if defined(SYS_CUSTOM_POWERLOCK_WRAP)
			if (bWrapped)
			{
				UnwrapSystemPowerChange(&gsSysSpecificData);
			}
#endif
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"SysSystemPostPowerState: OSInstallDeviceLISR failed to install ISR (%d)", eError));
				return eError;
			}
			SYS_SPECIFIC_DATA_SET(&gsSysSpecificData, SYS_SPECIFIC_DATA_ENABLE_LISR);
			SYS_SPECIFIC_DATA_CLEAR(&gsSysSpecificData, SYS_SPECIFIC_DATA_PM_UNINSTALL_LISR);
		}
#endif
	}
	return eError;
}


PVRSRV_ERROR SysDevicePrePowerState(IMG_UINT32				ui32DeviceIndex,
									PVRSRV_DEV_POWER_STATE	eNewPowerState,
									PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVR_UNREFERENCED_PARAMETER(eCurrentPowerState);

	if (ui32DeviceIndex != gui32SGXDeviceID)
	{
		return PVRSRV_OK;
	}

#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "SysDevicePrePowerState: SGX Entering state D3"));
		DisableSGXClocks(gpsSysData);
	}
#else
	PVR_UNREFERENCED_PARAMETER(eNewPowerState );
#endif
	return PVRSRV_OK;
}


PVRSRV_ERROR SysDevicePostPowerState(IMG_UINT32				ui32DeviceIndex,
									 PVRSRV_DEV_POWER_STATE	eNewPowerState,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_UNREFERENCED_PARAMETER(eNewPowerState);

	if (ui32DeviceIndex != gui32SGXDeviceID)
	{
		return eError;
	}

#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "SysDevicePostPowerState: SGX Leaving state D3"));
		eError = EnableSGXClocksWrap(gpsSysData);
	}
#else
	PVR_UNREFERENCED_PARAMETER(eCurrentPowerState);
#endif

	return eError;
}


PVRSRV_ERROR SysOEMFunction (	IMG_UINT32	ui32ID,
								IMG_VOID	*pvIn,
								IMG_UINT32	ulInSize,
								IMG_VOID	*pvOut,
								IMG_UINT32	ulOutSize)
{
	PVR_UNREFERENCED_PARAMETER(ui32ID);
	PVR_UNREFERENCED_PARAMETER(pvIn);
	PVR_UNREFERENCED_PARAMETER(ulInSize);
	PVR_UNREFERENCED_PARAMETER(pvOut);
	PVR_UNREFERENCED_PARAMETER(ulOutSize);

	if ((ui32ID == OEM_GET_EXT_FUNCS) &&
		(ulOutSize == sizeof(PVRSRV_DC_OEM_JTABLE)))
	{

		PVRSRV_DC_OEM_JTABLE *psOEMJTable = (PVRSRV_DC_OEM_JTABLE*) pvOut;
		psOEMJTable->pfnOEMBridgeDispatch = &PVRSRV_BridgeDispatchKM;
		return PVRSRV_OK;
	}

	return PVRSRV_ERROR_INVALID_PARAMS;
}
