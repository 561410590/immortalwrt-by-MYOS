/*
 * Copyright (c) [2020], MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws.
 * The information contained herein is confidential and proprietary to
 * MediaTek Inc. and/or its licensors.
 * Except as otherwise provided in the applicable licensing terms with
 * MediaTek Inc. and/or its licensors, any reproduction, modification, use or
 * disclosure of MediaTek Software, and information contained herein, in whole
 * or in part, shall be strictly prohibited.
*/
/****************************************************************************
 ****************************************************************************

    Module Name:
	rtmp_os.h

    Abstract:

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------
 */

#ifndef __RTMP_OS_H__
#define __RTMP_OS_H__

/* Driver Operators */
typedef int (*RTMP_PRINTK)(const char *ftm, ...);
typedef int (*RTMP_SNPRINTF)(char *, ULONG, const char *ftm, ...);

typedef struct _RTMP_OS_ABL_OPS {
	int (*ra_printk)(const char *ftm, ...);
	int (*ra_snprintf)(char *, ULONG, const char *ftm, ...);
} RTMP_OS_ABL_OPS;

extern RTMP_OS_ABL_OPS *pRaOsOps;

#define GNU_DISABLE_STACK_CHECK_START
#define GNU_DISABLE_STACK_CHECK_END
#define DO_PRAGMA(x)

#ifndef CONFIG_RTPCI_AP_RF_OFFSET
#define CONFIG_RTPCI_AP_RF_OFFSET 0
#endif

#ifdef LINUX
#include "mt_osal.h"
#include "os/rt_linux.h"
#endif /* LINUX */





#ifdef NONE
#include "os/rt_none.h"
#endif /* NONE */

#ifndef INLINE
#define INLINE inline
#endif

/* TODO: shiang, temporary put it here! */
#include "os/pkt_meta.h"

typedef enum _RTMP_TASK_STATUS_ {
	RTMP_TASK_STAT_UNKNOWN = 0,
	RTMP_TASK_STAT_INITED = 1,
	RTMP_TASK_STAT_RUNNING = 2,
	RTMP_TASK_STAT_STOPED = 4,
} RTMP_TASK_STATUS;
#define RTMP_TASK_CAN_DO_INSERT		(RTMP_TASK_STAT_INITED | RTMP_TASK_STAT_RUNNING)

#define RTMP_OS_TASK_NAME_LEN	16

#define RTMP_MATOpsInit(__pAd)
#define RTMP_MATPktRxNeedConvert(__pAd, __pDev)				\
	MATPktRxNeedConvert(__pAd, __pDev)
#define RTMP_MATEngineRxHandle(__pAd, __pPkt, __InfIdx)		\
	MATEngineRxHandle(__pAd, __pPkt, __InfIdx)

#ifndef OS_NDIS_SPIN_LOCK
#define OS_NDIS_SPIN_LOCK mt_os_spinlock_t
#endif

#ifndef OS_SPIN_LOCK_INIT
#define OS_SPIN_LOCK_INIT(__pLock)				\
	mt_os_spin_lock_init(__pLock)
#endif

#ifndef OS_SPIN_LOCK_DEINIT
#define OS_SPIN_LOCK_DEINIT(__pLock)				\
	mt_os_spin_lock_deinit(__pLock)
#endif

#ifndef OS_NdisAllocateSpinLock
#define OS_NdisAllocateSpinLock(__pLock)			\
	mt_os_spin_lock_init(__pLock)
#endif

#ifndef OS_NdisFreeSpinLock
#define OS_NdisFreeSpinLock(__pLock)				\
	mt_os_spin_lock_deinit(__pLock)
#endif

#ifndef OS_SPIN_LOCK_IRQSAVE
#define OS_SPIN_LOCK_IRQSAVE(__pLock, __pFlags)			\
	mt_os_spin_lock_irqsave(__pLock, __pFlags)
#endif

#ifndef OS_SPIN_UNLOCK_IRQRESTORE
#define OS_SPIN_UNLOCK_IRQRESTORE(__pLock, __pFlags)		\
	mt_os_spin_unlock_irqrestore(__pLock, __pFlags)
#endif

#ifndef OS_SPIN_LOCK
#define OS_SPIN_LOCK(__pLock)					\
	mt_os_spin_lock(__pLock)
#endif

#ifndef OS_SPIN_UNLOCK
#define OS_SPIN_UNLOCK(__pLock)					\
	mt_os_spin_unlock(__pLock)
#endif

#ifndef OS_SPIN_LOCK_BH
#define OS_SPIN_LOCK_BH(__pLock)				\
	mt_os_spin_lock_bh(__pLock)
#endif

#ifndef OS_SPIN_UNLOCK_BH
#define OS_SPIN_UNLOCK_BH(__pLock)				\
	mt_os_spin_unlock_bh(__pLock)
#endif

#ifndef OS_SPIN_LOCK_IRQ
#define OS_SPIN_LOCK_IRQ(__pLock)				\
	mt_os_spin_lock_irq(__pLock)
#endif

#ifndef OS_SPIN_UNLOCK_IRQ
#define OS_SPIN_UNLOCK_IRQ(__pLock)				\
	mt_os_spin_unlock_irq(__pLock)
#endif

#ifndef OS_IRQ_LOCK
#define OS_IRQ_LOCK(__pLock, __pFlags)				\
	{							\
		__pFlags = 0;					\
		mt_os_spin_lock_irqsave(__pLock, &__pFlags);	\
	}
#endif

#ifndef OS_IRQ_UNLOCK
#define OS_IRQ_UNLOCK(__pLock, __irqflag)			\
	mt_os_spin_unlock_irqrestore(__pLock, &__irqflag)
#endif

#ifndef OS_INT_LOCK
#define OS_INT_LOCK(__pLock, __pFlags)				\
	mt_os_spin_lock_irqsave(__pLock, &__pFlags)
#endif

#ifndef OS_INT_UNLOCK
#define OS_INT_UNLOCK(__pLock, __irqflag)			\
	mt_os_spin_unlock_irqrestore(__pLock, &__irqflag)
#endif

#ifndef OS_SEM_LOCK
#define OS_SEM_LOCK(__pLock)					\
	mt_os_spin_lock_bh(__pLock)
#endif

#ifndef OS_SEM_UNLOCK
#define OS_SEM_UNLOCK(__pLock)					\
	mt_os_spin_unlock_bh(__pLock)
#endif

#ifndef RTMP_SPIN_LOCK
#define RTMP_SPIN_LOCK(__pLock)				OS_SPIN_LOCK(__pLock)
#endif

#ifndef RTMP_SPIN_UNLOCK
#define RTMP_SPIN_UNLOCK(__pLock)			OS_SPIN_UNLOCK(__pLock)
#endif

#ifndef RTMP_SPIN_LOCK_IRQ
#define RTMP_SPIN_LOCK_IRQ(__pLock)			OS_SPIN_LOCK_IRQ(__pLock)
#endif

#ifndef RTMP_SPIN_UNLOCK_IRQ
#define RTMP_SPIN_UNLOCK_IRQ(__pLock)			OS_SPIN_UNLOCK_IRQ(__pLock)
#endif

#ifndef RTMP_SPIN_LOCK_IRQSAVE
#define RTMP_SPIN_LOCK_IRQSAVE(__pLock, __pFlags)	OS_SPIN_LOCK_IRQSAVE(__pLock, __pFlags)
#endif

#ifndef RTMP_SPIN_UNLOCK_IRQRESTORE
#define RTMP_SPIN_UNLOCK_IRQRESTORE(__pLock, __pFlags)	OS_SPIN_UNLOCK_IRQRESTORE(__pLock, __pFlags)
#endif

#ifndef RTMP_IRQ_LOCK
#define RTMP_IRQ_LOCK(__pLock, __Flags)			OS_IRQ_LOCK(__pLock, __Flags)
#endif

#ifndef RTMP_IRQ_UNLOCK
#define RTMP_IRQ_UNLOCK(__pLock, __Flags)		OS_IRQ_UNLOCK(__pLock, __Flags)
#endif

#ifndef RTMP_INT_LOCK
#define RTMP_INT_LOCK(__pLock)				OS_INT_LOCK(__pLock)
#endif

#ifndef RTMP_INT_UNLOCK
#define RTMP_INT_UNLOCK(__pLock)			OS_INT_UNLOCK(__pLock)
#endif

#ifndef NDIS_SPIN_LOCK
#define NDIS_SPIN_LOCK					OS_NDIS_SPIN_LOCK
#endif

#ifndef NdisAllocateSpinLock
#define NdisAllocateSpinLock(__pReserved, __pLock)	OS_NdisAllocateSpinLock(__pLock)
#endif

#ifndef NdisFreeSpinLock
#define NdisFreeSpinLock(__pLock)			OS_NdisFreeSpinLock(__pLock)
#endif

#ifndef NdisAcquireSpinLock
#define NdisAcquireSpinLock(__pLock)			OS_SEM_LOCK(__pLock)
#endif

#ifndef NdisReleaseSpinLock
#define NdisReleaseSpinLock(__pLock)			OS_SEM_UNLOCK(__pLock)
#endif

#ifndef LINUX
typedef struct _os_kref {
	NDIS_SPIN_LOCK lock;
    UCHAR refcount;
} os_kref;
#endif

void os_kref_init(os_kref *kref);
void os_kref_get(os_kref *kref);
int os_kref_put(os_kref *kref, void (*release)(os_kref *kref));




#endif /* __RMTP_OS_H__ */
