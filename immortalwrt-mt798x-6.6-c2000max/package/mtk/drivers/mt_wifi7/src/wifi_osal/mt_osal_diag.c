/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/if_arp.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/ieee80211.h>
#if KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE
#include <linux/cred.h>
#endif

#define SUBMODULE "mt_osal_diag"
#include <mt_osal_debug.h>
#include <mt_osal_type.h>
#include <mt_osal_util.h>
#include <mt_osal_diag.h>
#include <mt_osal_module.h>
#include <mt_osal_wifi.h>
#include <mt_osal_net.h>
#include <mt_osal_fs.h>

#define MT_OS_WAIT(_time) \
	{	\
		if (in_interrupt()) \
			mt_os_usec_delay(_time * 1000);\
		else { \
			msleep(_time); \
		} \
	}

#if (KERNEL_VERSION(5, 17, 0) <= LINUX_VERSION_CODE)
#define MT_GET_PRIV_DATA_FROM_INODE(inode)		pde_data(inode)
#define MT_GET_PRIV_DATA_FROM_FILE(file)		pde_data(file_inode(file))
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
#define MT_GET_PRIV_DATA_FROM_INODE(inode)		PDE_DATA(inode)
#define MT_GET_PRIV_DATA_FROM_FILE(file)		PDE_DATA(file_inode(file))
#else
#define MT_GET_PRIV_DATA_FROM_INODE(inode)		(PDE(inode)->data)
#define MT_GET_PRIV_DATA_FROM_FILE(file)		(PDE(file->f_path.dentry->d_inode)->data)
#endif

#define MT_DIAG_IN_TIME(now, pCtrl) \
	(MT_TIME_BEFORE(now, pCtrl->diag_enable_time + ((pCtrl->diag_duration) * OS_HZ)))

static char *MT_DIAG_CONN_ERR_INFO[MT_DIAG_CONN_ERROR_MAX] = {
	"WiFi frames losing. reason code ",
	"Capability checking failed.\n",
	"Authentication failed. reason code ",
	"Rejected by blacklist.\n",
	"Rejected by STA limiation.\n",
	"De-authed. reason code ",
	"Association ignored by band steering.\n"
};

static char *MT_DIAG_PROC_FILE_LIST_DEV0[MT_DIAG_BAND_MAX][MT_DIAG_PROC_FILE_NUM] = {
	{
		MT_DIAG_PROC_PATH_DEV0"/Stat2G",
		MT_DIAG_PROC_PATH_DEV0"/diag_duration_2G",
		MT_DIAG_PROC_PATH_DEV0"/diag_enable_2G",
		MT_DIAG_PROC_PATH_DEV0"/channel_occupancy_2G",
		MT_DIAG_PROC_PATH_DEV0"/stats_2G"
	},
	{
		MT_DIAG_PROC_PATH_DEV0"/Stat5G",
		MT_DIAG_PROC_PATH_DEV0"/diag_duration_5G",
		MT_DIAG_PROC_PATH_DEV0"/diag_enable_5G",
		MT_DIAG_PROC_PATH_DEV0"/channel_occupancy_5G",
		MT_DIAG_PROC_PATH_DEV0"/stats_5G"
	},
	{
		MT_DIAG_PROC_PATH_DEV0"/Stat6G",
		MT_DIAG_PROC_PATH_DEV0"/diag_duration_6G",
		MT_DIAG_PROC_PATH_DEV0"/diag_enable_6G",
		MT_DIAG_PROC_PATH_DEV0"/channel_occupancy_6G",
		MT_DIAG_PROC_PATH_DEV0"/stats_6G"
	}
};

static char *MT_DIAG_VAR_FILE_LIST_DEV0[MT_DIAG_BAND_MAX][MT_DIAG_VAR_FILE_NUM] = {
	{
		MT_DIAG_VAR_PATH_DEV0"/association_errors_2G",
		MT_DIAG_VAR_PATH_DEV0"/diag_log_2G",
	},
	{
		MT_DIAG_VAR_PATH_DEV0"/association_errors_5G",
		MT_DIAG_VAR_PATH_DEV0"/diag_log_5G"
	},
	{
		MT_DIAG_VAR_PATH_DEV0"/association_errors_6G",
		MT_DIAG_VAR_PATH_DEV0"/diag_log_6G"
	}
};

static char *MT_DIAG_PROC_FILE_LIST_DEV1[MT_DIAG_BAND_MAX][MT_DIAG_PROC_FILE_NUM] = {
	{
		MT_DIAG_PROC_PATH_DEV1"/Stat2G",
		MT_DIAG_PROC_PATH_DEV1"/diag_duration_2G",
		MT_DIAG_PROC_PATH_DEV1"/diag_enable_2G",
		MT_DIAG_PROC_PATH_DEV1"/channel_occupancy_2G",
		MT_DIAG_PROC_PATH_DEV1"/stats_2G"
	},
	{
		MT_DIAG_PROC_PATH_DEV1"/Stat5G",
		MT_DIAG_PROC_PATH_DEV1"/diag_duration_5G",
		MT_DIAG_PROC_PATH_DEV1"/diag_enable_5G",
		MT_DIAG_PROC_PATH_DEV1"/channel_occupancy_5G",
		MT_DIAG_PROC_PATH_DEV1"/stats_5G"
	},
	{
		MT_DIAG_PROC_PATH_DEV1"/Stat6G",
		MT_DIAG_PROC_PATH_DEV1"/diag_duration_6G",
		MT_DIAG_PROC_PATH_DEV1"/diag_enable_6G",
		MT_DIAG_PROC_PATH_DEV1"/channel_occupancy_6G",
		MT_DIAG_PROC_PATH_DEV1"/stats_6G"
	}
};

static char *MT_DIAG_VAR_FILE_LIST_DEV1[MT_DIAG_BAND_MAX][MT_DIAG_VAR_FILE_NUM] = {
	{
		MT_DIAG_VAR_PATH_DEV1"/association_errors_2G",
		MT_DIAG_VAR_PATH_DEV1"/diag_log_2G",
	},
	{
		MT_DIAG_VAR_PATH_DEV1"/association_errors_5G",
		MT_DIAG_VAR_PATH_DEV1"/diag_log_5G"
	},
	{
		MT_DIAG_VAR_PATH_DEV1"/association_errors_6G",
		MT_DIAG_VAR_PATH_DEV1"/diag_log_6G"
	}
};

struct mt_diag_handle {
	u8 max_band_num;
	bool proc_init;
	bool ctrl_init;
	struct mt_diag_ctrl *diag_ctrl;
} g_mt_diag_handle[MT_MAX_DEVICE_NUM];

static int mt_diag_io_ctrl(
	struct net_device *net_dev,
	enum mt_diag_cmd_attr_type cmd_id,
	void *data,
	unsigned int data_len)
{
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	int ret;
	struct mt_net_dev_priv *net_priv_info = NULL;

	if (!net_dev) {
		MT_DEBUG_TRACE("no net_dev\n");
		return -EFAULT;
	}
	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)net_dev);
	if (!net_priv_info)
		return -EFAULT;
	if (!net_priv_info->ioctl)
		return -EFAULT;
	memset(&IoctlConfig, 0, sizeof(IoctlConfig));
	pIoctlConfig->net_dev = net_dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(net_dev);
	pIoctlConfig->cmd = MT_IO_DIAG;
	pIoctlConfig->subcmd = cmd_id;
	pIoctlConfig->data = data;
	pIoctlConfig->data_len = data_len;
	ret = net_priv_info->ioctl(net_dev, pIoctlConfig);
	return ret;
}

static char *mt_diag_get_var_file_name(
	u8 device_idx, enum MT_ENUM_DIAG_BAND band_idx, u8 file_num)
{
	if (band_idx >= MT_DIAG_BAND_MAX)
		return MT_DIAG_VAR_PATH_UNKNOWN;
	if (file_num >= MT_DIAG_VAR_FILE_NUM)
		return MT_DIAG_VAR_PATH_UNKNOWN;
	if (device_idx == 1)
		return MT_DIAG_VAR_FILE_LIST_DEV1[band_idx][file_num];
	else
		return MT_DIAG_VAR_FILE_LIST_DEV0[band_idx][file_num];
}

static char *mt_diag_get_proc_path_name(
	u8 device_idx)
{
	if (device_idx == 1)
		return MT_DIAG_PROC_PATH_DEV1;
	else
		return MT_DIAG_PROC_PATH_DEV0;
}

static int mt_diag_statistic_proc_show(
	struct seq_file *m, void *v)
{
	struct mt_diag_ctrl *diag_ctrl = (struct mt_diag_ctrl *)m->private;
	struct mt_diag_statistic_info info;

	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("diag_ctrl is NULL\n");
		return -EFAULT;
	}

	memset(&info, 0, sizeof(info));
	MT_DEBUG_TRACE("diag_band=%d\n", diag_ctrl->diag_band);

	/* To Get rxCount/txCount from core wifi module. */
	mt_diag_io_ctrl(diag_ctrl->net_dev, mt_diag_get_statistic, &info, sizeof(info));

	seq_printf(m, "tx_packets:%ld\n", info.tx_count);
	seq_printf(m, "rx_packets:%ld\n", info.rx_count);
	seq_printf(m, "tx_fails:%ld\n", info.tx_fails);

	return 0;
}

static int mt_diag_statistic_proc_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, mt_diag_statistic_proc_show, MT_GET_PRIV_DATA_FROM_INODE(inode));
}

static int mt_diag_duration_proc_show(struct seq_file *m, void *v)
{
	struct mt_diag_ctrl *diag_ctrl = (struct mt_diag_ctrl *)m->private;

	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("diag_ctrl is NULL\n");
		return -EFAULT;
	}

	seq_printf(m, "%d\n", diag_ctrl->diag_duration);
	return 0;
}

static int mt_diag_duration_proc_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, mt_diag_duration_proc_show, MT_GET_PRIV_DATA_FROM_INODE(inode));
}

static ssize_t mt_diag_duration_proc_write(
	struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_diag_ctrl *diag_ctrl = NULL;
	unsigned int val = 0;
	u8 buf[10];

	if ((file == NULL) || (buffer == NULL))
		return -EFAULT;

	diag_ctrl = (struct mt_diag_ctrl *)MT_GET_PRIV_DATA_FROM_FILE(file);
	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("diag_ctrl is NULL\n");
		return -EFAULT;
	}

	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, buffer, min(sizeof(buf), count)))
		return -EFAULT;

	val = simple_strtol(buf, NULL, 10);
	if ((val <= 300) && (val >= 60)) {
		diag_ctrl->diag_duration = val;
		MT_DEBUG_TRACE("set duraion %d, band=%d.\n", val, diag_ctrl->diag_band);
	} else {
		MT_DEBUG_TRACE("rang error, valid rang is [60,300].\n");
		return -EFAULT;
	}
	return count;
}

static int mt_diag_enable_proc_show(struct seq_file *m, void *v)
{
	struct mt_diag_ctrl *diag_ctrl = (struct mt_diag_ctrl *)m->private;

	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("diag_ctrl is NULL\n");
		return -EFAULT;
	}

	seq_printf(m, "%d\n", diag_ctrl->diag_enable);
	return 0;
}

static int mt_diag_enable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_diag_enable_proc_show, MT_GET_PRIV_DATA_FROM_INODE(inode));
}

static ssize_t mt_diag_enable_proc_write(
	struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_diag_ctrl *diag_ctrl = NULL;
	MT_OS_FD fd = NULL;
	MT_OS_FS_INFO osFSInfo;
	u8 buf[10];
	unsigned int val = 0;
	u8 ucAction = 0;
	u32 log_entry_size;
	u32 assoc_error_entry_size;

	if ((file == NULL) || (buffer == NULL))
		return -EFAULT;

	diag_ctrl = (struct mt_diag_ctrl *)MT_GET_PRIV_DATA_FROM_FILE(file);
	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("diag_ctrl is NULL\n");
		return -EFAULT;
	}

	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, buffer, min(sizeof(buf), count)))
		return -EFAULT;

	val = simple_strtol(buf, NULL, 10);
	if (val > 1) {
		MT_DEBUG_TRACE("rang error, valid rang is [0,1].\n");
		return -EFAULT;
	}

	/* stop diag_log handling */
	diag_ctrl->diag_enable = 0;
	MT_OS_WAIT(500);
	log_entry_size = MT_DIAG_LOG_BUFF_SIZE*sizeof(struct mt_diag_log_entry);
	assoc_error_entry_size =
		MT_DIAG_ASSOC_ERROR_BUFF_SIZE*sizeof(struct mt_diag_assoc_error_entry);
	if (val !=  0) {
		spin_lock_bh(&diag_ctrl->diag_log_lock);
		if (!diag_ctrl->diag_log_entry) {
			diag_ctrl->diag_log_entry = kmalloc(log_entry_size, GFP_ATOMIC);
			if (!diag_ctrl->diag_log_entry) {
				MT_DEBUG_TRACE("fail to allocate diag_log_entry memory\n");
				spin_unlock_bh(&diag_ctrl->diag_log_lock);
				return -EFAULT;
			}
		}
		memset(diag_ctrl->diag_log_entry, 0, log_entry_size);
		diag_ctrl->diag_log_read_idx = 0;
		diag_ctrl->diag_log_write_idx = 0;
		spin_unlock_bh(&diag_ctrl->diag_log_lock);
		spin_lock_bh(&diag_ctrl->assoc_error_lock);
		if (!diag_ctrl->assoc_error_entry) {
			diag_ctrl->assoc_error_entry = kmalloc(assoc_error_entry_size, GFP_ATOMIC);
			if (!diag_ctrl->assoc_error_entry) {
				MT_DEBUG_TRACE("fail to allocate assoc_error_entry memory\n");
				spin_unlock_bh(&diag_ctrl->assoc_error_lock);
				return -EFAULT;
			}
		}
		memset(diag_ctrl->assoc_error_entry, 0, assoc_error_entry_size);
		diag_ctrl->assoc_error_read_idx = 0;
		diag_ctrl->assoc_error_write_idx = 0;
		spin_unlock_bh(&diag_ctrl->assoc_error_lock);
		if (diag_ctrl->diag_band >= MT_DIAG_BAND_MAX)
			return -EFAULT;

		mt_os_fs_info_change(&osFSInfo, true);
		fd = mt_os_file_open(
			mt_diag_get_var_file_name(diag_ctrl->device_idx, diag_ctrl->diag_band, 1),
			O_CREAT | O_TRUNC,
			S_IRWXU | S_IRWXG | S_IRWXO);
		if (!IS_FILE_OPEN_ERR(fd))
			mt_os_file_close(fd);
		mt_os_fs_info_change(&osFSInfo, false);
		diag_ctrl->diag_enable_time = jiffies;
		diag_ctrl->diag_enable = val;
		/* Drop unwanted Ctrl Frame */
		ucAction |= MT_DIAG_DROP_UNWANTED_CTRL_FRAME;
	} else {
		/* Drop RTS */
		ucAction |= MT_DIAG_DROP_RTS_CTRL_FRAME;
		/* Drop CTS */
		ucAction |= MT_DIAG_DROP_CTS_CTRL_FRAME;
		/* Drop unwanted Ctrl Frame */
		ucAction |= MT_DIAG_DROP_UNWANTED_CTRL_FRAME;
		spin_lock_bh(&diag_ctrl->diag_log_lock);
		kfree(diag_ctrl->diag_log_entry);
		diag_ctrl->diag_log_entry = NULL;
		spin_unlock_bh(&diag_ctrl->diag_log_lock);
		spin_lock_bh(&diag_ctrl->assoc_error_lock);
		kfree(diag_ctrl->assoc_error_entry);
		diag_ctrl->assoc_error_entry = NULL;
		spin_unlock_bh(&diag_ctrl->assoc_error_lock);
	}
	/* Inform core wifi module to enable/disable diag feature. */
	mt_diag_io_ctrl(
		diag_ctrl->net_dev,
		mt_diag_set_enable,
		&diag_ctrl->diag_enable, sizeof(diag_ctrl->diag_enable));
	/* Inform core wifi module to perform action. */
	mt_diag_io_ctrl(
		diag_ctrl->net_dev,
		mt_diag_set_drop_action,
		&ucAction, sizeof(ucAction));
	return count;
}

static int mt_diag_ch_occ_proc_show(
	struct seq_file *m, void *v)
{
	struct mt_diag_ctrl *diag_ctrl = NULL;
	struct mt_diag_ch_occ_info ch_occ;
	char *band;

	diag_ctrl = (struct mt_diag_ctrl *)m->private;
	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("pCtrl is NULL\n");
		return -EFAULT;
	}

	/* Get ch and ch_occ from core wifi module */
	memset(&ch_occ, 0, sizeof(ch_occ));
	mt_diag_io_ctrl(diag_ctrl->net_dev, mt_diag_get_ch_occ, &ch_occ, sizeof(ch_occ));
	seq_puts(m, "Channel\tBand\tOccupancy\n");
	if (diag_ctrl->diag_band == MT_DIAG_BAND_2G)
		band = "2G";
	else if (diag_ctrl->diag_band == MT_DIAG_BAND_5G)
		band = "5G";
	else if (diag_ctrl->diag_band == MT_DIAG_BAND_6G)
		band = "6G";
	else
		band = "Unknown";

	seq_printf(m, "%-7d\t%-4s\t%-9d\n", ch_occ.channel, band, ch_occ.ch_occ);
	return 0;
}

static int mt_diag_ch_occ_proc_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, mt_diag_ch_occ_proc_show, MT_GET_PRIV_DATA_FROM_INODE(inode));
}

static int mt_diag_sta_proc_show(
	struct seq_file *m, void *v)
{
	struct mt_diag_ctrl *diag_ctrl = NULL;
	int i = 0;
	u32 pecent = 0;
	u32 deci = 0;
	u32 tx_suc;
	struct mt_diag_entry_stat_info entry_stat;
	struct mt_diag_statistic_info statistic;

	diag_ctrl = (struct mt_diag_ctrl *)m->private;
	if (diag_ctrl == NULL) {
		MT_DEBUG_TRACE("pCtrl is NULL\n");
		return -EFAULT;
	}

	seq_puts(m, "STA:             \tSNR  \tRSSI   \tTXSUCC\n");
	for (;;) {
		memset(&entry_stat, 0, sizeof(entry_stat));
		entry_stat.entry_status = MT_DIAG_ENTRY_INVALID;
		entry_stat.entry_idx = i++;
		mt_diag_io_ctrl(diag_ctrl->net_dev,
			mt_diag_get_entry_stat, &entry_stat, sizeof(entry_stat));
		if (entry_stat.entry_status == MT_DIAG_ENTRY_SKIP)
			continue;
		if (entry_stat.entry_status == MT_DIAG_ENTRY_INVALID)
			break;
		seq_printf(m, " %02x:**:**:%02x:%02x:%02x",
			entry_stat.sta_addr[0],
			entry_stat.sta_addr[3],
			entry_stat.sta_addr[4],
			entry_stat.sta_addr[5]);
		seq_printf(m, "\t%d", entry_stat.snr);
		seq_printf(m, "\t%d", entry_stat.rssi);

		if (entry_stat.total_tx_count >= entry_stat.total_tx_fail_count)
			tx_suc = entry_stat.total_tx_count - entry_stat.total_tx_fail_count;
		else
			tx_suc = 0;
		if ((entry_stat.total_tx_count > 0) && (tx_suc <= entry_stat.total_tx_count)) {
			pecent = (u32)((100 * tx_suc)/entry_stat.total_tx_count);
			deci = (u32)((100 * tx_suc)%entry_stat.total_tx_count);
			deci = (u32)((100 * deci)/entry_stat.total_tx_count);
		} else {
			pecent = 100;
			deci = 0;
		}
		MT_DEBUG_TRACE("tx_succ=%d, tx_total=%d\n", tx_suc, entry_stat.total_tx_count);
		seq_printf(m, "\t%d.%02d\n", pecent, deci);
	}

	memset(&statistic, 0, sizeof(statistic));
	mt_diag_io_ctrl(diag_ctrl->net_dev, mt_diag_get_statistic, &statistic, sizeof(statistic));
	seq_printf(m, "RXCRCERR: %d\n", statistic.rx_fcs_error_count);
	return 0;
}

static int mt_diag_sta_proc_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, mt_diag_sta_proc_show, MT_GET_PRIV_DATA_FROM_INODE(inode));
}

#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE
static const struct proc_ops mt_diag_statistic_proc_fops = {
	.proc_open	= mt_diag_statistic_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static const struct proc_ops mt_diag_duration_proc_fops = {
	.proc_open	= mt_diag_duration_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
	.proc_write	= mt_diag_duration_proc_write,
};

static const struct proc_ops mt_diag_enable_proc_fops = {
	.proc_open	= mt_diag_enable_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
	.proc_write	= mt_diag_enable_proc_write,
};

static const struct proc_ops mt_diag_ch_occ_proc_fops = {
	.proc_open	= mt_diag_ch_occ_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static const struct proc_ops mt_diag_sta_proc_fops = {
	.proc_open	= mt_diag_sta_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};
#else

static const struct file_operations mt_diag_statistic_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mt_diag_statistic_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mt_diag_duration_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mt_diag_duration_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= mt_diag_duration_proc_write,
};

static const struct file_operations mt_diag_enable_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mt_diag_enable_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= mt_diag_enable_proc_write,
};

static const struct file_operations mt_diag_ch_occ_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mt_diag_ch_occ_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mt_diag_sta_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mt_diag_sta_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int mt_diag_create_directory(
	char *name, umode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int err;

	dentry = kern_path_create(AT_FDCWD, name, &path, LOOKUP_DIRECTORY);
	if (IS_ERR(dentry)) {
		MT_DEBUG_TRACE("Error create!\n");
		return PTR_ERR(dentry);
	}

#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	err = vfs_mkdir(&nop_mnt_idmap, d_inode(path.dentry), dentry, mode);
#elif KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE
	err = vfs_mkdir(current_user_ns(), d_inode(path.dentry), dentry, mode);
#else
	err = vfs_mkdir(d_inode(path.dentry), dentry, mode);
#endif
	if (!err)
		MT_DEBUG_TRACE("create %s OK!\n", name);
	done_path_create(&path, dentry);
	return err;
}

static void mt_diag_var_dir_mk(
	u8 device_idx, enum MT_ENUM_DIAG_BAND diag_band)
{
	MT_OS_FS_INFO osFSInfo;
	MT_OS_FD fd = NULL;
	int i;

	if (diag_band >= MT_DIAG_BAND_MAX) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", diag_band);
		return;
	}

	/* create var sub directory */
	mt_os_fs_info_change(&osFSInfo, true);
	if (device_idx == 1)
		mt_diag_create_directory(MT_DIAG_VAR_PATH_DEV1, 0644);
	else
		mt_diag_create_directory(MT_DIAG_VAR_PATH_DEV0, 0644);
	for (i = 0; i < MT_DIAG_VAR_FILE_NUM; i++) {
		fd = mt_os_file_open(
			mt_diag_get_var_file_name(device_idx, diag_band, i),
			O_CREAT | O_TRUNC,
			S_IRWXU | S_IRWXG | S_IRWXO);
		if (IS_FILE_OPEN_ERR(fd))
			MT_DEBUG_TRACE("Error create %s\n",
				mt_diag_get_var_file_name(device_idx, diag_band, i));
		else
			mt_os_file_close(fd);
	}

	mt_os_fs_info_change(&osFSInfo, false);
}

static void mt_diag_proc_entry_init(
	enum MT_ENUM_DIAG_BAND diag_band, struct mt_diag_ctrl *diag_ctrl)
{
#if KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE
	const struct proc_ops *DIAG_PROC_FOPS[5] = {
#else
	const struct file_operations *DIAG_PROC_FOPS[5] = {
#endif
		&mt_diag_statistic_proc_fops,
		&mt_diag_duration_proc_fops,
		&mt_diag_enable_proc_fops,
		&mt_diag_ch_occ_proc_fops,
		&mt_diag_sta_proc_fops
	};
	int i;

	MT_DEBUG_TRACE("[Device-%d]diag_band:%d\n", diag_ctrl->device_idx, diag_band);
	if (diag_band >= MT_DIAG_BAND_MAX)
		return;
	if (diag_ctrl->device_idx == 1) {
		for (i = 0; i < MT_DIAG_PROC_FILE_NUM; i++)
			proc_create_data(MT_DIAG_PROC_FILE_LIST_DEV1[diag_band][i],
				S_IRWXU | S_IRWXG | S_IRWXO, NULL,
				DIAG_PROC_FOPS[i], diag_ctrl);
	} else {
		for (i = 0; i < MT_DIAG_PROC_FILE_NUM; i++)
			proc_create_data(MT_DIAG_PROC_FILE_LIST_DEV0[diag_band][i],
				S_IRWXU | S_IRWXG | S_IRWXO, NULL,
				DIAG_PROC_FOPS[i], diag_ctrl);
	}
}

static void mt_diag_proc_entry_deinit(
	u8 device_idx, enum MT_ENUM_DIAG_BAND diag_band)
{
	int i;

	if (diag_band >= MT_DIAG_BAND_MAX)
		return;
	if (device_idx == 1) {
		for (i = 0; i < MT_DIAG_PROC_FILE_NUM; i++)
			remove_proc_entry(MT_DIAG_PROC_FILE_LIST_DEV1[diag_band][i], 0);
	} else {
		for (i = 0; i < MT_DIAG_PROC_FILE_NUM; i++)
			remove_proc_entry(MT_DIAG_PROC_FILE_LIST_DEV0[diag_band][i], 0);
	}
}

static void mt_diag_entry_free(
	struct mt_diag_ctrl *diag_ctrl)
{
	if (!diag_ctrl)
		return;
	spin_lock_bh(&diag_ctrl->diag_log_lock);
	kfree(diag_ctrl->diag_log_entry);
	diag_ctrl->diag_log_entry = 0;
	spin_unlock_bh(&diag_ctrl->diag_log_lock);
	spin_lock_bh(&diag_ctrl->assoc_error_lock);
	kfree(diag_ctrl->assoc_error_entry);
	diag_ctrl->assoc_error_entry = 0;
	spin_unlock_bh(&diag_ctrl->assoc_error_lock);
}

static void mt_diag_get_frame_info(
	void *frame, u8 isTx, u8 **pp_frame_type, u8 **pp_sta_addr)
{
	struct ieee80211_hdr *wifi_hdr = frame;
	__le16 frame_control = 0;

	if (!frame || !pp_frame_type || !pp_sta_addr)
		return;

	*pp_frame_type = NULL;
	*pp_sta_addr = NULL;

	frame_control = wifi_hdr->frame_control;
	if (ieee80211_is_mgmt(frame_control)) {
		if (isTx)
			*pp_sta_addr = wifi_hdr->addr1;
		else
			*pp_sta_addr = wifi_hdr->addr2;

		if (ieee80211_is_beacon(frame_control)) {
			/* just record our beacon */
			if (isTx)
				*pp_frame_type = "Beacon";
		} else if (ieee80211_is_probe_req(frame_control)) {
			/* just record peer's probe request */
			if (!isTx)
				*pp_frame_type = "Probe Request";
		} else if (ieee80211_is_probe_resp(frame_control)) {
			/* just record our probe response */
			if (isTx)
				*pp_frame_type = "Probe Response";
		} else if (ieee80211_is_atim(frame_control))
			*pp_frame_type = "ATIM";
		else if (ieee80211_is_disassoc(frame_control))
			*pp_frame_type = "Disassociation";
		else if (ieee80211_is_deauth(frame_control))
			*pp_frame_type = "Deauthentication";
		else if (ieee80211_is_assoc_req(frame_control)) {
			/* just record peer's Association request */
			if (!isTx)
				*pp_frame_type = "Association Request";
		} else if (ieee80211_is_assoc_resp(frame_control)) {
			/* just record our Association response */
			if (isTx)
				*pp_frame_type = "Association Response";
		} else if (ieee80211_is_reassoc_req(frame_control)) {
			/* just record peer's Reassociation request */
			if (!isTx)
				*pp_frame_type = "Reassociation Request";
		} else if (ieee80211_is_assoc_resp(frame_control)) {
			/* just record our Reassociation response */
			if (isTx)
				*pp_frame_type = "Reassociation Response";
		} else if (ieee80211_is_auth(frame_control))
			*pp_frame_type = "Authentication";
		else
			MT_DEBUG_TRACE("not handled MGMT frme, subtype=0x%x\n", frame_control);
	} else if (ieee80211_is_ctl(frame_control)) {

		if (ieee80211_is_rts(frame_control)) {
			*pp_frame_type = "RTS";
			if (isTx)
				*pp_sta_addr = wifi_hdr->addr1;
			else
				*pp_sta_addr = wifi_hdr->addr2;
		} else if (ieee80211_is_cts(frame_control)) {
			*pp_frame_type = "CTS";
			if (isTx)
				*pp_sta_addr = wifi_hdr->addr1;
		} else if (ieee80211_is_cts(frame_control)) {
			*pp_frame_type = "ACK";
			if (isTx)
				*pp_sta_addr = wifi_hdr->addr1;
		} else
			MT_DEBUG_TRACE("not handled CNTL frme, subtype=0x%x\n", frame_control);
	}
}

void mt_diag_frame_cache(
	u8 device_idx,
	u8 band_idx,
	struct mt_diag_frame_info *info)
{
	struct mt_diag_log_entry *entry;
	u8 *p_frame_type = NULL;
	u8 *p_sta_addr = NULL;
	u64 now;
	struct mt_diag_ctrl *diag_ctrl;
	struct mt_diag_handle *diag_handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	diag_handle = &g_mt_diag_handle[device_idx];
	if (band_idx >= diag_handle->max_band_num) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", band_idx);
		return;
	}
	now = jiffies;
	diag_ctrl = &diag_handle->diag_ctrl[band_idx];
	if (!info || !diag_ctrl)
		return;

	if (!MT_DIAG_IN_TIME(now, diag_ctrl))
		return;

	mt_diag_get_frame_info((void *)info->pData, info->isTX, &p_frame_type, &p_sta_addr);
	if (!p_frame_type)
		return;
	entry = kmalloc(sizeof(struct mt_diag_log_entry), GFP_ATOMIC);
	if (entry == NULL) {
		MT_DEBUG_TRACE("fail to allocate log_entry memory\n");
		return;
	}
	memset(entry, 0, sizeof(struct mt_diag_log_entry));
	mt_get_time_info(&entry->tm_info);
	entry->isTX = info->isTX;

	if (info->ssid_len <= 32)
		entry->ssid_len = info->ssid_len;
	else
		entry->ssid_len = 32;
	memcpy(entry->ssid, info->ssid, entry->ssid_len);

	entry->band = info->band;

	if (strlen(p_frame_type) < 32)
		memcpy(entry->frame_type, p_frame_type, strlen(p_frame_type));
	else
		memcpy(entry->frame_type, p_frame_type, 32-1);

	if (p_sta_addr)
		memcpy(entry->sta_addr, p_sta_addr, 6);

	entry->data_len =
		(info->dataLen > MT_DIAG_FRAME_SIZE_MAX) ? MT_DIAG_FRAME_SIZE_MAX : info->dataLen;
	memcpy(entry->data, info->pData, entry->data_len);

	spin_lock_bh(&diag_ctrl->diag_log_lock);
	/*Ring Buffer enough*/
	if (((diag_ctrl->diag_log_write_idx + 1) % MT_DIAG_LOG_BUFF_SIZE)
		!= diag_ctrl->diag_log_read_idx) {
		if (diag_ctrl->diag_log_entry != NULL) {
			memcpy(&diag_ctrl->diag_log_entry[diag_ctrl->diag_log_write_idx],
				entry, sizeof(struct mt_diag_log_entry));
			/* update write_idx */
			diag_ctrl->diag_log_write_idx =
				(diag_ctrl->diag_log_write_idx + 1) % MT_DIAG_LOG_BUFF_SIZE;
		}
	}
	spin_unlock_bh(&diag_ctrl->diag_log_lock);
	kfree(entry);
}

void mt_diag_conn_error(
	u8 device_idx,
	u8 band_idx,
	u8 *ssid,
	u32 ssid_len,
	u8 *addr,
	enum MT_ENUM_DIAG_CONN_ERROR_CODE DiagCode,
	u32 Reason)
{
	struct mt_diag_assoc_error_entry *entry;
	struct mt_diag_ctrl *diag_ctrl;
	struct mt_diag_handle *diag_handle;

	if (!addr)
		return;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	diag_handle = &g_mt_diag_handle[device_idx];
	if (band_idx >= diag_handle->max_band_num) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", band_idx);
		return;
	}
	diag_ctrl = &diag_handle->diag_ctrl[band_idx];
	if (diag_ctrl && diag_ctrl->init_flag) {
		entry = kmalloc(sizeof(struct mt_diag_assoc_error_entry), GFP_ATOMIC);
		if (entry == NULL) {
			MT_DEBUG_TRACE("fail to allocate log_entry memory\n");
			return;
		}
		memset(entry, 0, sizeof(struct mt_diag_assoc_error_entry));
		/*collect error info*/
		mt_get_time_info(&entry->tm_info);
		memcpy(entry->StaAddr, addr, 6);
		if (ssid_len > 0 && ssid_len <= 32)
			memcpy(entry->Ssid, ssid, ssid_len);
		entry->errCode = DiagCode;
		entry->reason = Reason;

		spin_lock_bh(&diag_ctrl->assoc_error_lock);
		/* Ring not full */
		if (((diag_ctrl->assoc_error_write_idx + 1) % MT_DIAG_ASSOC_ERROR_BUFF_SIZE) !=
			diag_ctrl->assoc_error_read_idx) {
			if (diag_ctrl->assoc_error_entry != NULL) {
				memcpy(&diag_ctrl->assoc_error_entry[diag_ctrl->assoc_error_write_idx],
					entry,
					sizeof(struct mt_diag_assoc_error_entry));
				/* update write_idx */
				diag_ctrl->assoc_error_write_idx =
					(diag_ctrl->assoc_error_write_idx + 1) % MT_DIAG_ASSOC_ERROR_BUFF_SIZE;
			}
		}
		spin_unlock_bh(&diag_ctrl->assoc_error_lock);
		kfree(entry);
	} else
		MT_DEBUG_TRACE("diag_ctrl not init\n");
}

void mt_diag_conn_error_write(u8 device_idx, u8 band_idx)
{
	MT_OS_FD fd = NULL;
	MT_OS_FS_INFO osFSInfo;
	u8 buf[256];
	u32 buf_size = 256;
	struct mt_diag_assoc_error_entry *log_entry;
	u32 write_size;
	struct mt_diag_ctrl *diag_ctrl;
	u8 write_weight = 0;
	int ret = 0;
	struct mt_diag_handle *diag_handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	diag_handle = &g_mt_diag_handle[device_idx];
	if (band_idx >= diag_handle->max_band_num) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", band_idx);
		return;
	}
	diag_ctrl = &diag_handle->diag_ctrl[band_idx];
	if (diag_ctrl && diag_ctrl->init_flag) {
		memset(buf, 0, sizeof(buf));
		log_entry = kmalloc(sizeof(struct mt_diag_assoc_error_entry), GFP_ATOMIC);
		if (log_entry == NULL) {
			MT_DEBUG_TRACE("fail to allocate log_entry memory\n");
			return;
		}
		memset(log_entry, 0, sizeof(struct mt_diag_assoc_error_entry));
		mt_os_fs_info_change(&osFSInfo, true);
		fd = mt_os_file_open(
			mt_diag_get_var_file_name(device_idx, band_idx, 0), O_WRONLY, 0);
		if (IS_FILE_OPEN_ERR(fd)) {
			fd = mt_os_file_open(mt_diag_get_var_file_name(device_idx, band_idx, 0),
								O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
			if (IS_FILE_OPEN_ERR(fd)) {
				mt_os_fs_info_change(&osFSInfo, false);
				MT_DEBUG_TRACE("open conn_error_file fail band_idx=%d\n", band_idx);
				kfree(log_entry);
				return;
			}
			diag_ctrl->assoc_error_file_offset = 0;
		}

		fd->f_pos = diag_ctrl->assoc_error_file_offset;
		while (1) {
			/*avoid occupy mlme long time*/
			if (write_weight++ >= MT_DIAG_WRITE_FILE_PER_SECCOND)
				break;
			spin_lock_bh(&diag_ctrl->assoc_error_lock);
			/*ring buffer is empty*/
			if (diag_ctrl->assoc_error_write_idx == diag_ctrl->assoc_error_read_idx) {
				spin_unlock_bh(&diag_ctrl->assoc_error_lock);
				break;
			}
			/*memory is freed*/
			if (diag_ctrl->assoc_error_entry == NULL) {
				spin_unlock_bh(&diag_ctrl->assoc_error_lock);
				break;
			}
			memcpy(log_entry,
				&diag_ctrl->assoc_error_entry[diag_ctrl->assoc_error_read_idx],
				sizeof(struct mt_diag_assoc_error_entry));
			memset(&diag_ctrl->assoc_error_entry[diag_ctrl->assoc_error_read_idx],
				0, sizeof(struct mt_diag_assoc_error_entry));
			/* update read_idx */
			diag_ctrl->assoc_error_read_idx =
				(diag_ctrl->assoc_error_read_idx + 1) % MT_DIAG_ASSOC_ERROR_BUFF_SIZE;
			spin_unlock_bh(&diag_ctrl->assoc_error_lock);
			ret = snprintf(buf + strlen(buf),
				buf_size -  strlen(buf),
				"%04d-%02d-%02d %02d:%02d:%02d",
				log_entry->tm_info.year,
				log_entry->tm_info.month,
				log_entry->tm_info.day,
				log_entry->tm_info.hour,
				log_entry->tm_info.min,
				log_entry->tm_info.sec);
			if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
				MT_DEBUG_TRACE("Time snprintf error!\n");
			/*STA-MAC*/
			ret = snprintf(buf + strlen(buf),
				buf_size -  strlen(buf),
				MT_MACSTR" ", MT_MAC2STR(log_entry->StaAddr));
			if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
				MT_DEBUG_TRACE("MAC snprintf error!\n");
			/*SSID*/
			ret = snprintf(buf + strlen(buf),
				buf_size -  strlen(buf), log_entry->Ssid);
			if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
				MT_DEBUG_TRACE("SSID snprintf error!\n");
			/*ErrorInfo*/
			ret = snprintf(buf + strlen(buf),
				buf_size -  strlen(buf), ": %s", MT_DIAG_CONN_ERR_INFO[log_entry->errCode]);
			if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
				MT_DEBUG_TRACE("ErrInfo snprintf error!\n");
			/*Reason*/
			if (log_entry->errCode == MT_DIAG_CONN_DEAUTH ||
				log_entry->errCode == MT_DIAG_CONN_FRAME_LOST ||
				log_entry->errCode == MT_DIAG_CONN_AUTH_FAIL) {
				ret = snprintf(buf + strlen(buf),
					buf_size -  strlen(buf),
					"%d\n", log_entry->reason);
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("snprintf error!\n");
			}
			write_size = strlen(buf);
			if ((diag_ctrl->assoc_error_file_offset + write_size) > MT_DIAG_LOG_FILE_SIZE_MAX) {
				diag_ctrl->assoc_error_file_offset = 0; /* overwrite from file start */
				fd->f_pos = 0;
			}
			mt_os_file_write(fd, buf, write_size);
			diag_ctrl->assoc_error_file_offset += write_size;
			memset(log_entry, 0, sizeof(struct mt_diag_assoc_error_entry));
			memset(buf, 0, buf_size);
		}
		kfree(log_entry);
		filp_close(fd, NULL);
		mt_os_fs_info_change(&osFSInfo, false);
	}
}

void mt_diag_log_file_write(u8 device_idx, u8 band_idx)
{
	MT_OS_FD fd = NULL;
	MT_OS_FS_INFO osFSInfo;
	u8 *buf = NULL;
	u32 buf_size = 0;
	struct mt_diag_log_entry *log_entry;
	u32 write_size;
	u8 *pData = NULL;
	u32 dataLen = 0, index = 0;
	struct mt_diag_ctrl *diag_ctrl;
	u8 write_weight = 0;
	int ret = 0;
	struct mt_diag_handle *diag_handle;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	diag_handle = &g_mt_diag_handle[device_idx];
	if (band_idx >= diag_handle->max_band_num) {
		MT_DEBUG_TRACE("Invalid band_idx(=%d)\n", band_idx);
		return;
	}
	diag_ctrl = &diag_handle->diag_ctrl[band_idx];
	if (diag_ctrl && diag_ctrl->init_flag) {
		if (diag_ctrl->diag_enable == 0) {
			/*MTWF_DBG(pAd, DBG_CAT_TEST, CATTEST_DIAG, DBG_LVL_ERROR,
				"diag_log not enable dbdc_idx=%d\n", dbdc_idx); */
			return;
		}
		MT_DEBUG_TRACE("device_idx(=%d), band_idx(=%d)\n", device_idx, band_idx);
		buf_size = 2 * MT_DIAG_FRAME_SIZE_MAX + 128;
		buf = kmalloc(buf_size, GFP_ATOMIC);
		if (buf == NULL) {
			MT_DEBUG_TRACE("buf fail to allocate buf memory\n");
			return;
		}
		memset(buf, 0, buf_size);

		log_entry = kmalloc(sizeof(struct mt_diag_log_entry), GFP_ATOMIC);
		if (log_entry == NULL) {
			MT_DEBUG_TRACE("log entry fail to allocate log_entry memory\n");
			kfree(buf);
			return;
		}
		memset(log_entry, 0, sizeof(struct mt_diag_log_entry));
		mt_os_fs_info_change(&osFSInfo, true);
		fd = mt_os_file_open(
			mt_diag_get_var_file_name(device_idx, band_idx, 1), O_WRONLY, 0);
		if (IS_FILE_OPEN_ERR(fd)) {
			fd = mt_os_file_open(mt_diag_get_var_file_name(device_idx, band_idx, 1),
					O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
			if (IS_FILE_OPEN_ERR(fd)) {
				mt_os_fs_info_change(&osFSInfo, false);
				MT_DEBUG_TRACE("open diag_log_file fail band_idx=%d\n", band_idx);
				kfree(buf);
				kfree(log_entry);
				return;
			}
			diag_ctrl->diag_log_file_offset = 0;
		}

		fd->f_pos = diag_ctrl->diag_log_file_offset;
		while (1) {
			if (diag_ctrl->diag_enable == 0)
				break;
			/*avoid occupy mlme long time*/
			if (write_weight++ >= MT_DIAG_WRITE_FILE_PER_SECCOND)
				break;
			spin_lock_bh(&diag_ctrl->diag_log_lock);
			/*ring buffer is empty*/
			if (diag_ctrl->diag_log_write_idx == diag_ctrl->diag_log_read_idx) {
				spin_unlock_bh(&diag_ctrl->diag_log_lock);
				break;
			}
			/*memory is freed*/
			if (diag_ctrl->diag_log_entry == NULL) {
				spin_unlock_bh(&diag_ctrl->diag_log_lock);
				break;
			}
			memcpy(log_entry,
				&diag_ctrl->diag_log_entry[diag_ctrl->diag_log_read_idx],
				sizeof(struct mt_diag_log_entry));
			memset(&diag_ctrl->diag_log_entry[diag_ctrl->diag_log_read_idx],
				0, sizeof(struct mt_diag_log_entry));
			/* update read_idx */
			diag_ctrl->diag_log_read_idx =
				(diag_ctrl->diag_log_read_idx + 1) % MT_DIAG_LOG_BUFF_SIZE;
			spin_unlock_bh(&diag_ctrl->diag_log_lock);

			/*
			 * pData include 80211 header,
			 * because CNTL frame just include FCS except the 80211 header
			 */
			pData = log_entry->data;
			dataLen = log_entry->data_len;

			/* time */
			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf),
				buf_size -  strlen(buf),
				"%04d-%02d-%02d %02d:%02d:%02d ",
				log_entry->tm_info.year,
				log_entry->tm_info.month,
				log_entry->tm_info.day,
				log_entry->tm_info.hour,
				log_entry->tm_info.min,
				log_entry->tm_info.sec);
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("time snprintf error!\n");
			}
			/* SSID */
			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf), log_entry->ssid);
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("ssid snprintf error!\n");
			}
			/* sends/receives */
			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf),
					buf_size -  strlen(buf), (log_entry->isTX) ?  " sends " : " receives ");
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("dir snprintf error!\n");
			}
			/* frame type */
			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf), log_entry->frame_type);
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("ftype snprintf error!\n");
			}

			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf),
				(log_entry->isTX) ? " to " : " from ");
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("tf snprintf error!\n");
			}
			/* STA MAC address */
			if (strlen(buf) < (buf_size - 1)) {
				if (!!memcmp(mt_zero_mac_addr, log_entry->sta_addr, MT_MAC_ADDR_LEN))
					ret = snprintf(buf + strlen(buf),
						buf_size -  strlen(buf),
						MT_MACSTR,
						MT_MAC2STR(log_entry->sta_addr));
				else
					ret = snprintf(buf + strlen(buf),
						buf_size -  strlen(buf),
						"%s", "No-STA-Addr");
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("mac snprintf error!\n");
			}

			/* frame body */
			if (strlen(buf) < (buf_size - 1)) {
				ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf), "[");
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
					MT_DEBUG_TRACE("fbody snprintf error!\n");
			}
			for (index = 0; index < dataLen; index++) {
				if (strlen(buf) < (buf_size - 1))
					ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf), "%02X", pData[index]);
				if (mt_os_snprintf_error(buf_size -  strlen(buf), ret)) {
					MT_DEBUG_TRACE("data snprintf error!\n");
					break;
				}
			}

			if (strlen(buf) < (buf_size - 1))
				ret = snprintf(buf + strlen(buf), buf_size -  strlen(buf), "]\n");
			else
				ret = snprintf(buf + (buf_size - 2), 2, "]\n");
			if (mt_os_snprintf_error(buf_size -  strlen(buf), ret))
				MT_DEBUG_TRACE("end snprintf error!\n");
			write_size = strlen(buf);
			if ((diag_ctrl->diag_log_file_offset + write_size) > MT_DIAG_LOG_FILE_SIZE_MAX) {
				diag_ctrl->diag_log_file_offset = 0; /* overwrite from file start */
				fd->f_pos = 0;
			}
			mt_os_file_write(fd, buf, write_size);
			diag_ctrl->diag_log_file_offset += write_size;
			memset(buf, 0, buf_size);
			memset(log_entry, 0, sizeof(struct mt_diag_log_entry));
		}

		mt_os_file_close(fd);
		mt_os_fs_info_change(&osFSInfo, false);
		kfree(buf);
		kfree(log_entry);
	} else
		MT_DEBUG_TRACE("diag_ctrl not init, band_idx=%d\n", band_idx);
}

void mt_diag_proc_init(
	u8 device_idx,
	u8 band_idx,
	u8 max_band_num,
	struct net_device *net_dev,
	struct mt_diag_ops *ops)
{
	struct mt_diag_handle *diag_handle;
	struct mt_diag_ctrl *diag_ctrl;

	if (max_band_num < MT_DIAG_BAND_MAX) {
		MT_DEBUG_TRACE("Invalid max_band_num(=%d)\n", max_band_num);
		return;
	}
	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	if (band_idx >= max_band_num) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", band_idx);
		return;
	}
	MT_DEBUG_TRACE("device_idx = %d, band_idx = %d, max_band_num = %d\n",
		device_idx, band_idx, max_band_num);

	diag_handle = &g_mt_diag_handle[device_idx];
	if (diag_handle->ctrl_init == false) {
		diag_handle->max_band_num = max_band_num;
		diag_handle->diag_ctrl = kmalloc_array(
			max_band_num, sizeof(struct mt_diag_ctrl), GFP_ATOMIC);
		if (diag_handle->diag_ctrl) {
			diag_handle->ctrl_init = true;
			memset(diag_handle->diag_ctrl, 0, sizeof(struct mt_diag_ctrl)*max_band_num);
		} else {
			MT_DEBUG_TRACE("diag_ctrl init error\n");
			return;
		}
	}
	diag_ctrl = &g_mt_diag_handle[device_idx].diag_ctrl[band_idx];
	if (diag_ctrl->init_flag == true) {
		MT_DEBUG_TRACE("pDiagCtrl already init dbdc_idx=%d\n", band_idx);
		return;
	}
	diag_ctrl->device_idx = device_idx;
	diag_ctrl->diag_band = band_idx;
	diag_ctrl->net_dev = net_dev;
	diag_ctrl->channel = 0;
	diag_ctrl->diag_duration = 60;
	spin_lock_init(&(diag_ctrl->diag_log_lock));
	spin_lock_bh(&diag_ctrl->diag_log_lock);
	diag_ctrl->diag_log_entry = NULL;
	spin_unlock_bh(&diag_ctrl->diag_log_lock);
	spin_lock_init(&(diag_ctrl->assoc_error_lock));
	spin_lock_bh(&diag_ctrl->assoc_error_lock);
	diag_ctrl->assoc_error_entry = NULL;
	spin_unlock_bh(&diag_ctrl->assoc_error_lock);
	mt_diag_var_dir_mk(diag_ctrl->device_idx, diag_ctrl->diag_band);
	if (g_mt_diag_handle[device_idx].proc_init == false) {
		proc_mkdir(mt_diag_get_proc_path_name(device_idx), NULL);
		g_mt_diag_handle[device_idx].proc_init = true;
	} else
		MT_DEBUG_TRACE("/proc/%s already exists.\n",
			mt_diag_get_proc_path_name(device_idx));
	mt_diag_proc_entry_init(diag_ctrl->diag_band, diag_ctrl);
	if (ops) {
		ops->diag_frame_cache = mt_diag_frame_cache;
		ops->diag_conn_error = mt_diag_conn_error;
		ops->diag_conn_error_write = mt_diag_conn_error_write;
		ops->diag_log_file_write = mt_diag_log_file_write;
	}
	diag_ctrl->init_flag = true;
}
EXPORT_SYMBOL(mt_diag_proc_init);

void mt_diag_proc_exit(
	u8 device_idx, u8 band_idx, u8 b_last)
{
	struct mt_diag_handle *diag_handle;
	struct mt_diag_ctrl *diag_ctrl;

	if (device_idx >= MT_MAX_DEVICE_NUM) {
		MT_DEBUG_TRACE("Invalid device_idx(=%d)\n", device_idx);
		return;
	}
	diag_handle = &g_mt_diag_handle[device_idx];
	if (band_idx >= diag_handle->max_band_num) {
		MT_DEBUG_TRACE("Invalid diag_band(=%d)\n", band_idx);
		return;
	}
	diag_ctrl = &diag_handle->diag_ctrl[band_idx];
	if (diag_ctrl->init_flag == false) {
		MT_DEBUG_TRACE("pDiagCtrl already deinit. device_idx(=%d) diag_band(=%d)\n",
			device_idx, band_idx);
		return;
	}

	diag_ctrl->diag_enable = 0;
	diag_ctrl->init_flag = false;
	mt_diag_entry_free(diag_ctrl);
	/* stop diag_log handling */
	MT_OS_WAIT(500);

	mt_diag_proc_entry_deinit(diag_ctrl->device_idx, diag_ctrl->diag_band);
	if (diag_handle->proc_init && b_last)  {
		MT_DEBUG_TRACE("[Dev%d][Band%d] Remove proc entry\n", device_idx, band_idx);
		remove_proc_entry(mt_diag_get_proc_path_name(device_idx), NULL);
		diag_handle->proc_init = false;
	}
	if (diag_handle->ctrl_init && b_last)  {
		MT_DEBUG_TRACE("[Dev%d][Band%d] Free diag_ctrl\n", device_idx, band_idx);
		kfree(diag_handle->diag_ctrl);
		diag_handle->diag_ctrl = NULL;
		diag_handle->ctrl_init = false;
	}
}
EXPORT_SYMBOL(mt_diag_proc_exit);

DECLARE_SUB_MODULE_INIT(mt_osal_diag_init)
{
	int i;
	struct mt_diag_handle *diag_handle;

	MT_DEBUG_TRACE("loaded");

	memset(&g_mt_diag_handle[0], 0, sizeof(g_mt_diag_handle));
	for (i = 0; i < MT_MAX_DEVICE_NUM; i++) {
		diag_handle = &g_mt_diag_handle[i];
		diag_handle->proc_init = false;
		diag_handle->ctrl_init = false;
		diag_handle->diag_ctrl = NULL;
	}
	return 0;
}
sub_module_init(mt_osal_diag_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_diag_exit)
{
	int i;
	struct mt_diag_handle *diag_handle;

	MT_DEBUG_TRACE("unloaded");
	for (i = 0; i < MT_MAX_DEVICE_NUM; i++) {
		diag_handle = &g_mt_diag_handle[i];
		if (diag_handle->proc_init)  {
			remove_proc_entry(mt_diag_get_proc_path_name(i), NULL);
			diag_handle->proc_init = false;
		}
		kfree(diag_handle->diag_ctrl);
		diag_handle->diag_ctrl = NULL;
		diag_handle->ctrl_init = false;
	}
}
sub_module_exit(mt_osal_diag_exit);
