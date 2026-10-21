/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#ifndef __MT_OSAL_LOCK_H_
#define __MT_OSAL_LOCK_H_

void mt_rcu_read_lock(void);

void mt_rcu_read_unlock(void);

void mt_call_rcu(struct rcu_head *head, rcu_callback_t func);

void mt_synchronize_rcu(void);


#include <linux/spinlock.h>

typedef spinlock_t mt_os_spinlock_t;
typedef unsigned long mt_os_irq_flags_t;

#define MT_OS_SPIN_LOCK_UNLOCKED(__lock) __SPIN_LOCK_UNLOCKED(__lock)

static inline void mt_os_spin_lock_init(mt_os_spinlock_t *__lock_ptr)
{
	spin_lock_init(__lock_ptr);
}

static inline void mt_os_spin_lock_deinit(mt_os_spinlock_t *__lock_ptr)
{
}

static inline void mt_os_spin_lock_irqsave(mt_os_spinlock_t *__lock_ptr, mt_os_irq_flags_t *__irq_flags_ptr)
{
	spin_lock_irqsave(__lock_ptr, *(__irq_flags_ptr));
}

static inline void mt_os_spin_unlock_irqrestore(mt_os_spinlock_t *__lock_ptr, mt_os_irq_flags_t *__irq_flags_ptr)
{
	spin_unlock_irqrestore(__lock_ptr, *(__irq_flags_ptr));
}

static inline void mt_os_spin_lock(mt_os_spinlock_t *__lock_ptr)
{
	spin_lock(__lock_ptr);
}

static inline void mt_os_spin_unlock(mt_os_spinlock_t *__lock_ptr)
{
	spin_unlock(__lock_ptr);
}

static inline void mt_os_spin_lock_bh(mt_os_spinlock_t *__lock_ptr)
{
	spin_lock_bh(__lock_ptr);
}

static inline void mt_os_spin_unlock_bh(mt_os_spinlock_t *__lock_ptr)
{
	spin_unlock_bh(__lock_ptr);
}

static inline void mt_os_spin_lock_irq(mt_os_spinlock_t *__lock_ptr)
{
	spin_lock_irq(__lock_ptr);
}

static inline void mt_os_spin_unlock_irq(mt_os_spinlock_t *__lock_ptr)
{
	spin_unlock_irq(__lock_ptr);
}

struct mt_os_lockless_data {
	struct rcu_head rcu;
};

#define mt_os_lockless_data_get(_data) container_of(_data, struct mt_os_lockless_data, rcu)

int mt_os_lockless_ctx_init(const char *name, void *ctx);
void mt_os_lockless_ctx_cleanup(void *ctx);
void mt_os_lockless_enter(void *ctx);
void mt_os_lockless_exit(void *ctx);
void mt_os_lockless_barrier_update(void *ctx, void (*update_func)(void *), void *arg);
void mt_os_lockless_barrier(void *ctx);
void mt_os_lockless_call(void *ctx, void (*callback)(void *), void *arg);

#define mt_os_lockless_access_pointer(ctx, ptr) \
	rcu_dereference(ptr)

#define mt_os_lockless_update_pointer(ctx, ptr, new_ptr) \
	rcu_assign_pointer(ptr, new_ptr)

#define mt_os_lockless_hlist_for_each_entry(pos, head, member) \
	hlist_for_each_entry_rcu(pos, head, member)

#define mt_os_lockless_hlist_del(node) \
	hlist_del_rcu(node)

#define mt_os_lockless_hlist_add_head(node, head) \
	hlist_add_head_rcu(node, head)

#define mt_os_lockless_hlist_add_tail(node, head) \
	hlist_add_tail_rcu(node, head)

#define mt_os_lockless_hlist_replace(old, new) \
	hlist_replace_rcu(old, new)

#endif /* __MT_OSAL_LOCK_H_ */
