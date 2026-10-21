/****************************************************************************

    Module Name:
	mt_os_util.h

	Abstract:
	All function prototypes are provided from UTIL modules.

	Note:
	But can not use any OS key word and compile option here.
	All functions are provided from UTIL modules.

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------

***************************************************************************/

#ifndef __MT_OS_UTIL_H__
#define __MT_OS_UTIL_H__

INT os_alloc_mem(
	VOID * pAd,
	UCHAR **mem,
	ULONG size
);

VOID os_free_mem(
	PVOID mem
);

INT os_alloc_vmem(
	VOID *pAd,
	UCHAR **vmem,
	ULONG size
);

VOID os_free_vmem(
	PVOID vmem
);

#define os_zero_mem(ptr, length) mt_os_zero_mem(ptr, length)
#define os_move_mem(pDest, pSrc, length) mt_os_move_mem(pDest, pSrc, length)
#define os_fill_mem(pBuf, Length, Fill) mt_os_fill_mem(pBuf, Length, Fill)
#define os_cmp_mem(Destination, Source, Length) mt_os_cmp_mem(Destination, Source, Length)
#define os_equal_mem(pBuf1, pBuf2, length) mt_os_equal_mem(pBuf1, pBuf2, length)

MT_OS_FD os_file_open(
	char *pPath, int flag, int mode
);

INT os_file_close(
	MT_OS_FD osfd
);

VOID os_file_seek(
	MT_OS_FD osfd,
	int offset
);

INT os_file_write(
	MT_OS_FD osfd, char *pDataPtr, int writeLen
);

INT os_file_read(
	MT_OS_FD osfd, char *pDataPtr, int readLen
);

void os_fs_info_change(
	MT_OS_FS_INFO *pOSFSInfo, unsigned char bSet
);

VOID os_file_open_ext(
	CHAR * pPath,
	MT_OS_FD_EXT *fd,
	INT32  flag, /* CreateDisposition */
	INT32 file_mode
);

INT os_file_close_ext(
	MT_OS_FD_EXT osfd
);

INT os_file_write_ext(
	MT_OS_FD_EXT osfd,
	CHAR *pDataPtr,
	INT32 writeLen
);

INT os_file_read_ext(
	MT_OS_FD_EXT osfd,
	CHAR *pDataPtr,
	INT32 readLen
);

VOID os_msec_delay(UINT msec);
VOID os_usec_delay(UINT usec);
VOID os_load_code_from_bin(void *pAd, unsigned char **image, char *bin_name, UINT32 *code_len);
CHAR *os_str_chr(CHAR *str, INT32 character);
#define os_str_chr(str, character) mt_os_str_chr(str, character)
#define os_str_spn(str1, str2) mt_os_str_spn(str1, str2)
#define os_str_pbrk(str1, str2) mt_os_str_pbrk(str1, str2)
#define os_str_tol(str, endptr, base) mt_os_str_tol(str, endptr, base)
#define os_str_toul(str, endptr, base) mt_os_str_toul(str, endptr, base)
#define os_str_str(str1, str2) mt_os_str_str(str1, str2)

void *os_net_dev_create(
	int MC_RowID,
	unsigned int *pIoctlIF,
	int devType,
	int devNum,
	int privMemSize,
	char *pNamePrefix,
	BOOLEAN autoSuffix);
void *os_net_dev_init(
	struct _RTMP_ADAPTER *pAd, struct mt_os_net_dev_op_hook *pNetDevHook);
int os_net_dev_attach(
	struct _RTMP_ADAPTER *mac_ad,
	void *net_dev,
	struct mt_os_net_dev_op_hook *pDevOpHook);
VOID os_free_mac_ad(VOID *pAdSrc);
INT os_ioctl(void *net_dev, void *ioctl_cfg_);

#if !defined(COMPOS_WIN) && !defined(COMPOS_TESTMODE_WIN)
VOID
os_alloc_dma_buf(
	IN VOID *pHandler,
	IN ULONG Length,
	IN BOOLEAN Cached,
	OUT VOID **VirtualAddress,
	OUT OS_PHYSICAL_ADDRESS * phy_addr);

VOID
os_free_dma_buf(
	IN VOID *pHandler,
	IN ULONG Length,
	IN PVOID VirtualAddress,
	IN OS_PHYSICAL_ADDRESS PhysicalAddress);

OS_PACKET
os_alloc_packet(
	IN VOID *pHandler,
	IN ULONG Length);

VOID
os_free_packet(
	IN VOID *pHandler,
	IN OS_PACKET *pPacket);

VOID os_event_init(OS_EVENT *pEvent);
VOID os_event_wait(OS_EVENT *pEvent, UINT32 TimeOut);
VOID os_event_set(OS_EVENT *pEvent);
#endif /*#if !defined(COMPOS_WIN) && !defined(COMPOS_TESTMODE_WIN)*/
#endif
