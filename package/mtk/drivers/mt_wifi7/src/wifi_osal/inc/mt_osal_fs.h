/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_FS_H_
#define __MT_OSAL_FS_H_

#include <linux/version.h>

/*****************************************************************************
 *	OS file operation related data structure definitions
 ******************************************************************************/
/* if you add any new type, please also modify mt_os_file_open() */
#define MT_OS_FILE_RDONLY	0x0F01
#define MT_OS_FILE_WRONLY	0x0F02
#define MT_OS_FILE_CREAT	0x0F03
#define MT_OS_FILE_TRUNC	0x0F04

/***********************************************************************************
 *	OS file operation related data structure definitions
 ***********************************************************************************/
typedef struct file *MT_OS_FD;
typedef struct device MT_OS_DEV;

typedef struct _MT_OS_FS_INFO_ {
	int				fsuid;
	int				fsgid;
#if KERNEL_VERSION(5, 18, 0) > LINUX_VERSION_CODE
	mm_segment_t	fs;
#endif
} MT_OS_FS_INFO;

typedef struct _MT_OS_FD_EXT {
	MT_OS_FD fsFd;
	MT_OS_FS_INFO fsInfo;
	int Status;
	unsigned int fsize;
} MT_OS_FD_EXT;

#define IS_FILE_OPEN_ERR(_fd)	((_fd == NULL) || IS_ERR((_fd)))

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

int mt_sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);

/* OS File */
MT_OS_FD mt_os_file_open(char *pPath,  int flag, int mode);
int mt_os_file_close(MT_OS_FD osfd);
void mt_os_file_seek(MT_OS_FD osfd, int offset);
int mt_os_file_read(MT_OS_FD osfd, char *pDataPtr, int readLen);
int mt_os_file_write(MT_OS_FD osfd, char *pDataPtr, int writeLen);
void mt_os_fs_info_change(MT_OS_FS_INFO *pOSFSInfoOrg, unsigned char bSet);
void mt_os_file_open_ext(
	char * pPath,
	MT_OS_FD_EXT *fd,
	int flag, /* CreateDisposition */
	int file_mode);
void mt_os_load_code_from_bin(
	MT_OS_DEV *dev, unsigned char **image, char *bin_name, unsigned int *code_len);

#endif /* __MT_OSAL_FS_H_ */
