/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#ifndef __MT_OSAL_MEM_H_
#define __MT_OSAL_MEM_H_

static inline int mt_os_mem_alloc(void **mem, size_t size, bool allow_suspend)
{
	if (!mem)
		return -EINVAL;

	(*mem) = kmalloc(size, allow_suspend? GFP_KERNEL: GFP_ATOMIC);
	if ((*mem) == NULL)
		return -ENOMEM;

	return 0;
}

static inline int mt_os_mem_zalloc(void **mem, size_t size, bool allow_suspend)
{
	if (!mem)
		return -EINVAL;

	(*mem) = kzalloc(size, allow_suspend? GFP_KERNEL: GFP_ATOMIC);
	if ((*mem) == NULL)
		return -ENOMEM;
	return 0;
}

static inline int mt_os_vmem_alloc(void **mem, size_t size)
{
        if (!mem)
                return -EINVAL;

        (*mem) = vmalloc(size);
        if ((*mem) == NULL)
                return -ENOMEM;
        return 0;
}

static inline void mt_os_vmem_free(void *mem)
{
	if (mem)
		vfree(mem);
}

static inline void mt_os_mem_free(void *mem)
{
	if (mem)
		kfree(mem);
}

static inline void *mt_os_mem_copy(void *dst_addr, void* src_addr, size_t num_bytes)
{
	return memcpy(dst_addr, src_addr, num_bytes);
}

static inline void mt_os_zero_mem(
	void * ptr,
	size_t length)
{
	memset(ptr, 0, length);
}

static inline void mt_os_move_mem(
	void * pDest,
	void * pSrc,
	size_t length)
{
	memmove(pDest, pSrc, length);
}

static inline void mt_os_fill_mem(
	void * pBuf,
	unsigned long Length,
	unsigned char Fill)
{
	memset(pBuf, Fill, Length);
}

static inline int mt_os_cmp_mem(
	void * Destination,
	void * Source,
	unsigned long Length)
{
	return memcmp(Destination, Source, Length);
}

static inline int mt_os_equal_mem(
	void * pBuf1,
	void * pBuf2,
	size_t length)
{
	return !memcmp(pBuf1, pBuf2, length);
}

#endif /* __MT_OSAL_MEM_H_ */
