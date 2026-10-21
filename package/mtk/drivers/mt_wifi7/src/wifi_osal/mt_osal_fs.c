/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>

#define SUBMODULE "mt_osal_fs"
#include <mt_osal_debug.h>
#include <mt_osal_fs.h>
#include <mt_osal_module.h>

void mt_debugfs_remove(struct dentry *dentry)
{
	debugfs_remove(dentry);
}
EXPORT_SYMBOL(mt_debugfs_remove);

void mt_debugfs_remove_recursive(struct dentry *dentry)
{
	debugfs_remove_recursive(dentry);
}
EXPORT_SYMBOL(mt_debugfs_remove_recursive);

struct dentry *mt_debugfs_create_file(const char *name, umode_t mode,
				      struct dentry *parent, void *data,
				      const struct file_operations *fops)
{
	return debugfs_create_file(name, mode, parent, data, fops);
}
EXPORT_SYMBOL(mt_debugfs_create_file);

struct dentry *mt_debugfs_create_dir(const char *name, struct dentry *parent)
{
	return debugfs_create_dir(name, parent);
}
EXPORT_SYMBOL(mt_debugfs_create_dir);

#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
struct dentry *mt_debugfs_create_devm_seqfile(struct device *dev, const char *name,
				 struct dentry *parent,
				 int (*read_fn)(struct seq_file *s, void *data))
{
	return debugfs_create_devm_seqfile(dev, name, parent, read_fn);
}
#else
void mt_debugfs_create_devm_seqfile(struct device *dev, const char *name,
				 struct dentry *parent,
				 int (*read_fn)(struct seq_file *s, void *data))
{
	debugfs_create_devm_seqfile(dev, name, parent, read_fn);
}
#endif
EXPORT_SYMBOL(mt_debugfs_create_devm_seqfile);

struct dentry *mt_debugfs_lookup(const char *name, struct dentry *parent)
{
	return debugfs_lookup(name, parent);
}
EXPORT_SYMBOL(mt_debugfs_lookup);

int mt_sysfs_create_group(
        struct kobject *kobj, const struct attribute_group *grp)
{
	return sysfs_create_group(kobj, grp);
}
EXPORT_SYMBOL(mt_sysfs_create_group);

/*******************************************************************************
 *	File open/close related functions.
 *******************************************************************************/
MT_OS_FD mt_os_file_open(char *pPath, int flag, int mode)
{
	struct file *filePtr;

	if (flag == MT_OS_FILE_RDONLY)
		flag = O_RDONLY;
	else if (flag == MT_OS_FILE_WRONLY)
		flag = O_WRONLY;
	else if (flag == MT_OS_FILE_CREAT)
		flag = O_CREAT;
	else if (flag == MT_OS_FILE_TRUNC)
		flag = O_TRUNC;

	filePtr = filp_open(pPath, flag, 0);

	if (IS_ERR(filePtr)) {
		MT_DEBUG_TRACE("Error %ld opening %s\n", -PTR_ERR(filePtr), pPath);
	}

	return (MT_OS_FD) filePtr;
}
EXPORT_SYMBOL(mt_os_file_open);

int mt_os_file_close(MT_OS_FD osfd)
{
	filp_close(osfd, NULL);
	return 0;
}
EXPORT_SYMBOL(mt_os_file_close);


void mt_os_file_seek(MT_OS_FD osfd, int offset)
{
	osfd->f_pos = offset;
}
EXPORT_SYMBOL(mt_os_file_seek);


int mt_os_file_read(MT_OS_FD osfd, char *pDataPtr, int readLen)
{
	/* The object must have a read method */
#if (KERNEL_VERSION(3, 19, 0) > LINUX_VERSION_CODE)
	if (osfd->f_op && osfd->f_op->read) {
		return osfd->f_op->read(osfd, pDataPtr, readLen, &osfd->f_pos);
	}
#elif (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE)
	if (osfd->f_mode & FMODE_CAN_READ) {
		return kernel_read(osfd, pDataPtr, readLen, &osfd->f_pos);
	}
#else
	if (osfd->f_mode & FMODE_CAN_READ) {
		return __vfs_read(osfd, pDataPtr, readLen, &osfd->f_pos);
	}
#endif
	else {
		MT_DEBUG_TRACE("no file read method\n");
		return -1;
	}
}
EXPORT_SYMBOL(mt_os_file_read);

int mt_os_file_write(MT_OS_FD osfd, char *pDataPtr, int writeLen)
{
#if (KERNEL_VERSION(4, 1, 0) > LINUX_VERSION_CODE)
	return osfd->f_op->write(osfd, pDataPtr, (size_t) writeLen, &osfd->f_pos);
#elif (KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE && KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE)
	return __kernel_write(osfd, pDataPtr, (size_t) writeLen, &osfd->f_pos);
#elif (KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE)
	return kernel_write(osfd, pDataPtr, (size_t) writeLen, &osfd->f_pos);
#else
	return __vfs_write(osfd, pDataPtr, (size_t) writeLen, &osfd->f_pos);
#endif
}
EXPORT_SYMBOL(mt_os_file_write);

void mt_os_fs_info_change(MT_OS_FS_INFO *pOSFSInfo, unsigned char bSet)
{
	if (bSet) {
		/* Save uid and gid used for filesystem access. */
		/* Set user and group to 0 (root) */
#if (KERNEL_VERSION(2, 6, 29) > LINUX_VERSION_CODE)
		pOSFSInfo->fsuid = current->fsuid;
		pOSFSInfo->fsgid = current->fsgid;
		current->fsuid = current->fsgid = 0;
#else
#ifdef CONFIG_UIDGID_STRICT_TYPE_CHECKS
		kuid_t uid;
		kgid_t gid;

		uid = current_fsuid();
		gid = current_fsgid();
		pOSFSInfo->fsuid = (int)uid.val;
		pOSFSInfo->fsgid = (int)gid.val;
#else
		/* pOSFSInfo->fsuid = (int)(current_fsuid()); */
		/* pOSFSInfo->fsgid = (int)(current_fsgid()); */
#endif
#endif
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
		pOSFSInfo->fs = get_fs();
		set_fs(KERNEL_DS);
#endif
	} else {
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
		set_fs(pOSFSInfo->fs);
#endif
#if (KERNEL_VERSION(2, 6, 29) > LINUX_VERSION_CODE)
		current->fsuid = pOSFSInfo->fsuid;
		current->fsgid = pOSFSInfo->fsgid;
#endif
	}
}
EXPORT_SYMBOL(mt_os_fs_info_change);

void mt_os_file_open_ext(
	char * pPath,
	MT_OS_FD_EXT *fd,
	int flag, /* CreateDisposition */
	int file_mode)
{
	memset(fd, 0, sizeof(MT_OS_FD_EXT));
	fd->fsFd = mt_os_file_open(pPath, flag, file_mode);
	fd->Status = IS_FILE_OPEN_ERR(fd->fsFd);

	if (!fd->Status) {
#ifdef LINUX
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
		fd->fsize = (unsigned long)fd->fsFd->f_dentry->d_inode->i_size;
#else
		fd->fsize = (unsigned long)fd->fsFd->f_path.dentry->d_inode->i_size;
#endif
#endif /* LINUX */
		mt_os_fs_info_change(&fd->fsInfo, true);
	}
}
EXPORT_SYMBOL(mt_os_file_open_ext);

#define MT_OS_BIN_MAX_PARTS 8

static int mt_os_load_one_bin(MT_OS_DEV *dev, char *name,
			      unsigned char **image, unsigned int *code_len)
{
	const struct firmware *fw_entry;
	char *fw_name = name;
	struct file *fp;
	unsigned char *buf;
	loff_t size;
	ssize_t n;

	*image = NULL;
	*code_len = 0;

	/* Absolute paths (e.g. /etc/wireless/...) are not firmware names. */
	if (name[0] == '/') {
		fp = filp_open(name, O_RDONLY, 0);
		if (!IS_ERR(fp)) {
			size = i_size_read(file_inode(fp));
			if (size <= 0) {
				filp_close(fp, NULL);
				return -EINVAL;
			}
			buf = kmalloc(size + 1, GFP_KERNEL);
			if (!buf) {
				filp_close(fp, NULL);
				return -ENOMEM;
			}
			n = kernel_read(fp, buf, size, &fp->f_pos);
			filp_close(fp, NULL);
			if (n < 0) {
				kfree(buf);
				return (int)n;
			}
			buf[n] = '\0';
			*image = buf;
			*code_len = (unsigned int)n;
			return 0;
		}
		if ((strncasecmp(name, "/etc/wireless/", 14) == 0) ||
		    (strncasecmp(name, "/lib/firmware/", 14) == 0))
			fw_name = name + 14;
	}

	if (request_firmware(&fw_entry, fw_name, dev) != 0)
		return -ENOENT;

	buf = kmalloc(fw_entry->size + 1, GFP_KERNEL);
	if (!buf) {
		release_firmware(fw_entry);
		return -ENOMEM;
	}
	memcpy(buf, fw_entry->data, fw_entry->size);
	buf[fw_entry->size] = '\0';
	*image = buf;
	*code_len = fw_entry->size;
	release_firmware(fw_entry);
	return 0;
}

void mt_os_load_code_from_bin(
	MT_OS_DEV *dev, unsigned char **image, char *bin_name, unsigned int *code_len)
{
	char *copy, *p, *tok;
	unsigned char *parts[MT_OS_BIN_MAX_PARTS];
	unsigned int lens[MT_OS_BIN_MAX_PARTS];
	unsigned int total = 0;
	int n = 0, i;
	unsigned char *buf;

	*image = NULL;
	if (code_len)
		*code_len = 0;

	if (!bin_name)
		return;

	copy = kstrdup(bin_name, GFP_KERNEL);
	if (!copy)
		return;

	p = copy;
	while ((tok = strsep(&p, ";")) != NULL) {
		while (*tok == ' ' || *tok == '\t')
			tok++;
		if (*tok == '\0')
			continue;
		if (n >= MT_OS_BIN_MAX_PARTS)
			break;
		if (mt_os_load_one_bin(dev, tok, &parts[n], &lens[n])) {
			MT_DEBUG_TRACE("fw not available(%s)\n", tok);
			goto err_parts;
		}
		total += lens[n];
		n++;
	}
	kfree(copy);
	copy = NULL;

	if (!n)
		return;

	buf = kmalloc(total + 1, GFP_KERNEL);
	if (!buf)
		goto err_parts;

	total = 0;
	for (i = 0; i < n; i++) {
		memcpy(buf + total, parts[i], lens[i]);
		total += lens[i];
		kfree(parts[i]);
	}
	buf[total] = '\0';
	*image = buf;
	if (code_len)
		*code_len = total;
	return;

err_parts:
	while (n--)
		kfree(parts[n]);
	kfree(copy);
}
EXPORT_SYMBOL(mt_os_load_code_from_bin);

DECLARE_SUB_MODULE_INIT(mt_osal_fs_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_fs_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_fs_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_fs_exit);
