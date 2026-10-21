/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/proc_fs.h>

#define SUBMODULE "mt_osal_proc"
#include <mt_osal_debug.h>
#include <mt_osal_net.h>
#include <mt_osal_wifi.h>
#include <mt_osal_proc.h>
#include <mt_osal_module.h>

static int mt_procDbgInfoShow(struct seq_file *seq, void *v)
{
	struct mt_proc_priv *procpriv = (struct mt_proc_priv *)seq->private;
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;

	if (!procpriv) {
		MT_DEBUG_ERROR("proc pointer get fail!!!\n");
		return -1;
	}

	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)procpriv->dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = procpriv->dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(procpriv->dev);
	pIoctlConfig->cmd = MT_IO_PROC;
	pIoctlConfig->subcmd = proc_file_show;
	pIoctlConfig->subid = procpriv->subid;
	pIoctlConfig->data = (void *)seq;
	net_priv_info->ioctl(procpriv->dev, pIoctlConfig);
	return 0;
}

int mt_procDbgOpen(struct inode *inode, struct file *file)
{
#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	return single_open(file, mt_procDbgInfoShow, pde_data(file_inode(file)));
#else
	return single_open(file, mt_procDbgInfoShow, PDE_DATA(file_inode(file)));
#endif
}

int mt_procFileOpen(struct inode *n, struct file *f)
{
	struct mt_proc_priv *procpriv = NULL;
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(pde_data(file_inode(f)));
#elif (KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(PDE_DATA(file_inode(f)));
#else
	procpriv = (struct mt_proc_priv *)(PDE(file_inode(f))->data);
#endif

	if (!procpriv) {
		MT_DEBUG_ERROR("proc pointer get fail!!!\n");
		return -1;
	}

	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)procpriv->dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = procpriv->dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(procpriv->dev);
	pIoctlConfig->cmd = MT_IO_PROC;
	pIoctlConfig->subcmd = proc_file_open;
	pIoctlConfig->subid = procpriv->subid;
	net_priv_info->ioctl(procpriv->dev, pIoctlConfig);

	return 0;
}

int mt_procFileRelease(struct inode *n, struct file *f)
{
	struct mt_proc_priv *procpriv = NULL;
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(pde_data(file_inode(f)));
#elif (KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(PDE_DATA(file_inode(f)));
#else
	procpriv = (struct mt_proc_priv *)(PDE(file_inode(f))->data);
#endif

	if (!procpriv) {
		MT_DEBUG_ERROR("proc pointer get fail!!!\n");
		return -1;
	}

	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)procpriv->dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = procpriv->dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(procpriv->dev);
	pIoctlConfig->cmd = MT_IO_PROC;
	pIoctlConfig->subcmd = proc_file_release;
	pIoctlConfig->subid = procpriv->subid;
	net_priv_info->ioctl(procpriv->dev, pIoctlConfig);

	return 0;
}

ssize_t mt_procFileRead(struct file *filp,
	char __user *buf, size_t count, loff_t *f_pos)
{
	struct mt_proc_priv *procpriv = NULL;
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	ssize_t u4CopySize = 0;

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(pde_data(file_inode(filp)));
#elif (KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE)
	procpriv = (struct mt_proc_priv *)(PDE_DATA(file_inode(filp)));
#else
	procpriv = (struct mt_proc_priv *)(PDE(file_inode(filp))->data);
#endif

	if (!procpriv) {
		MT_DEBUG_ERROR("proc pointer get fail!!!\n");
		return -1;
	}

	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)procpriv->dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = procpriv->dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(procpriv->dev);
	pIoctlConfig->cmd = MT_IO_PROC;
	pIoctlConfig->subcmd = proc_file_read;
	pIoctlConfig->subid = procpriv->subid;
	pIoctlConfig->data = (void *)buf;
	pIoctlConfig->data_len = (unsigned int)count;
	u4CopySize = net_priv_info->ioctl(procpriv->dev, pIoctlConfig);

	if (u4CopySize > 0)
		*f_pos += u4CopySize;

	return u4CopySize;
}

int mt_proc_remove(void *proc)
{
	if (proc) {
		proc_remove((struct proc_dir_entry *)proc);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(mt_proc_remove);

void *mt_proc_mkdir(char *name)
{
	struct proc_dir_entry *proc = NULL;

	if (name)
		proc = proc_mkdir(name, init_net.proc_net);
	return (void *)proc;
}
EXPORT_SYMBOL(mt_proc_mkdir);

int mt_proc_show(MT_OS_SEQFD seq, const char *pFmt, ...)
{
	va_list args;
	char strBuf[100];

	va_start(args, pFmt);
	if (vsnprintf(strBuf, 100, pFmt, args) > 0)
		seq_printf(seq, strBuf);
	va_end(args);

    return 0;
}
EXPORT_SYMBOL(mt_proc_show);


DECLARE_SUB_MODULE_INIT(mt_osal_proc_init)
{
	MT_DEBUG_ERROR("mt_osal_proc_init loaded");

	return 0;
}
sub_module_init(mt_osal_proc_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_proc_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_proc_exit);
