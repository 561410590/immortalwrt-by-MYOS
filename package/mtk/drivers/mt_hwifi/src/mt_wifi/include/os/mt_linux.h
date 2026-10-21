/*
 * Copyright (c) [2020] MediaTek Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ""AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MT_WIFI_CMN_H_
#define __MT_WIFI_CMN_H_

/* CONFIG_LINUX_CRYPTO */
#include <linux/scatterlist.h>
#include <linux/ieee80211.h>
#ifdef MT_WIFI_COMMON_SUPPORT
#include <linux/version.h>
#include <linux/rcupdate.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/oom.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/rtc.h>

int mt_unregister_oom_notifier(struct notifier_block *nb);
void mt_debugfs_remove(struct dentry *dentry);
void mt_debugfs_remove_recursive(struct dentry *dentry);
struct dentry *mt_debugfs_create_file(const char *name, umode_t mode,
				struct dentry *parent, void *data,
				const struct file_operations *fops);
struct dentry *mt_debugfs_create_dir(const char *name, struct dentry *parent);
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
struct dentry *mt_debugfs_create_devm_seqfile(struct device *dev, const char *name,
				struct dentry *parent,
				int (*read_fn)(struct seq_file *s, void *data));
#else
void mt_debugfs_create_devm_seqfile(struct device *dev, const char *name,
				struct dentry *parent,
				int (*read_fn)(struct seq_file *s, void *data));
#endif
struct dentry *mt_debugfs_lookup(const char *name, struct dentry *parent);
int mt_init_dummy_netdev(struct net_device *dev);
void mt_get_online_cpus(void);
void mt_put_online_cpus(void);
void *mt_idr_find(struct idr *idr, unsigned long id);
int mt_idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp);
void mt_idr_remove(struct idr *idr, unsigned long id);
void mt_call_rcu(struct rcu_head *head, rcu_callback_t func);
void mt_synchronize_rcu(void);
void mt_rcu_read_lock(void);
void mt_rcu_read_unlock(void);
struct workqueue_struct *mt_alloc_workqueue(const char *fmt, unsigned int flags, int max_active);
void mt_destroy_workqueue(struct workqueue_struct *wq);
#else /* MT_WIFI_COMMON_SUPPORT */

#define mt_register_oom_notifier(nb) register_oom_notifier(nb)
#define mt_unregister_oom_notifier(nb) unregister_oom_notifier(nb)
#define mt_debugfs_remove(dentry) debugfs_remove(dentry)
#define mt_debugfs_remove_recursive(dentry) debugfs_remove_recursive(dentry)
#define mt_debugfs_create_file(name, mode, parent, data, fops) debugfs_create_file(name, mode, parent, data, fops)
#define mt_debugfs_create_dir(name, parent) debugfs_create_dir(name, parent)
#define mt_debugfs_create_devm_seqfile(dev, name, parent, read_fn) debugfs_create_devm_seqfile(dev, name, parent, read_fn)
#define mt_debugfs_lookup(name, parent) debugfs_lookup(name, parent)
#define mt_init_dummy_netdev(dev) init_dummy_netdev(dev)
#endif /* !MT_WIFI_COMMON_SUPPORT */
struct rtc_time mt_rtc_ktime_to_tm(ktime_t ktime);
/*redefine dev_emerg*/
/* CONFIG_LINUX_CRYPTO */
typedef void (*DEV_HANDLE_FN)(struct work_struct *work);
void mt_device_work_init(
	struct workqueue_struct **dev_wq,
	struct delayed_work *dev_work,
	DEV_HANDLE_FN func);
#endif /* __MT_WIFI_CMN_H_ */
