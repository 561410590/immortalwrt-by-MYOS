/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_PROC_H_
#define __MT_OSAL_PROC_H_

enum mt_proc_cmd_attr_type {
	proc_file_open = 1,
	proc_file_read,
	proc_file_release,
	proc_file_show,
};

struct mt_proc_priv{
	struct net_device *dev;
	unsigned short subid;
};

typedef struct seq_file *MT_OS_SEQFD;

int mt_proc_remove(void *proc);
void *mt_proc_mkdir(char *name);
int mt_procDbgOpen(struct inode *inode, struct file *file);
int mt_procFileOpen(struct inode *n, struct file *f);
int mt_procFileRelease(struct inode *n, struct file *f);
ssize_t mt_procFileRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
int mt_proc_show(MT_OS_SEQFD seq, const char *pFmt,	...);

#endif /* __MT_OSAL_PROC_H_ */
