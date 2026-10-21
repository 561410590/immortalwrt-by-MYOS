/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <asm/unaligned.h>

#define SUBMODULE "mt_osal_csi"
#include <mt_osal_debug.h>
#include <mt_osal_net.h>
#include <mt_osal_wifi.h>
#include <mt_osal_proc.h>
#include <mt_osal_csi.h>
#include <mt_osal_module.h>

/*csi policy*/
static struct nla_policy csi_genl_policy[CSI_ATTR_MAX + 1] = {
	[CSI_ATTR_REPORT_MSG] = { .type = NLA_STRING },
};

/*csi ops init*/
static struct genl_ops csi_genl_ops[] = {
	{
		.cmd = CSI_OPS_REPORT,
		.flags = 0,
#if KERNEL_VERSION(5, 2, 0) > LINUX_VERSION_CODE
		.policy = csi_genl_policy,
#endif
		.doit = mt_csi_genl_recv_doit,
		.dumpit = NULL,
	}
};

/*csi family*/
static struct genl_family csi_genl_family = {
#if KERNEL_VERSION(4, 9, 300) >= LINUX_VERSION_CODE
	.id = GENL_ID_GENERATE,
#endif
#if KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE
	.policy = csi_genl_policy,
#endif
	.hdrsize = 0,
	.name = CSI_GENL_NAME,
	.version = 1,
	.maxattr = CSI_ATTR_MAX,
#if KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE
	.ops = csi_genl_ops,
	.n_ops = ARRAY_SIZE(csi_genl_ops),
	.module = THIS_MODULE,
#endif
};

#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
static const struct file_operations csi_debug_ops = {
	.owner = THIS_MODULE,
	.open = mt_procDbgOpen,
	.read	= seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations csidata_ops = {
	.owner = THIS_MODULE,
	.read = mt_procFileRead,
	.open = mt_procFileOpen,
	.release = mt_procFileRelease,
};
#else
static const struct proc_ops csi_debug_ops = {
	.proc_open = mt_procDbgOpen,
	.proc_read	= seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops csidata_ops = {
	.proc_read = mt_procFileRead,
	.proc_open = mt_procFileOpen,
	.proc_release = mt_procFileRelease,
};
#endif

int mt_csi_genl_recv_doit(struct sk_buff *skb_temp, struct genl_info *info)
{
	struct nlattr *na = NULL;
	struct net_device * dev = NULL;
	char cmd_msg[32] = {0};
	char dev_string[16] = {0};
	char req_type[16] = {0}; /*chain or pkt*/
	struct mt_net_dev_priv *net_priv_info = NULL;
	struct mt_os_wifi_ioctl_cmd_cfg IoctlConfig, *pIoctlConfig = &IoctlConfig;
	int ret;

	if (!info || !skb_temp) {
		MT_DEBUG_TRACE("genl_info or skb null!\n");
		return -EINVAL;
	}

	/*step1: parse the net dev and request dump number*/
	na = info->attrs[CSI_ATTR_REPORT_MSG];

	if (!na) {
		MT_DEBUG_TRACE("get attr fail!!!\n");
		return -EINVAL;
	}

#if KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE
	if (nla_validate(na, na->nla_len, CSI_ATTR_MAX, csi_genl_policy, NULL))
#else
	if (nla_validate(na, na->nla_len, CSI_ATTR_MAX, csi_genl_policy))
#endif
	{
		MT_DEBUG_TRACE("nla_validate fail!!!\n");
		return -EINVAL;
	}

	memmove(cmd_msg, (char *)nla_data(na), na->nla_len);
	MT_DEBUG_INFO("csi nl get msg from usr space: %s .\n", cmd_msg);

	/*msg ex: rax0-pkt, rax0-pkt-n, rax0-chain-n, rax0-chain-0, rax0-auto-interval*/
	if (strlen(cmd_msg)) {
		char *find = NULL;

		find = strpbrk(cmd_msg, "-");
		if (find) {
			*find = '\0';
			strncpy(req_type, find + 1, sizeof(req_type));
			req_type[sizeof(req_type) - 1] = '\0';
		}
		strncpy(dev_string, cmd_msg, sizeof(dev_string));
		dev_string[sizeof(dev_string) - 1] = '\0';
	} else {
		MT_DEBUG_TRACE("empty data from csi user!!!\n");
		return -EINVAL;
	}

	dev = dev_get_by_name(genl_info_net(info), dev_string);

	if (!dev) {
		MT_DEBUG_TRACE("CSI genl_rev not find net_dev(%s).\n", dev_string);
		return -EFAULT;
	}

	if (!netif_running(dev)) {
		MT_DEBUG_TRACE("net dev(%s) is not running.\n", dev->name);
		return -EFAULT;
	}

	net_priv_info = (struct mt_net_dev_priv *)netdev_priv((struct net_device *)dev);
	if (!net_priv_info)
		return -ENETDOWN;
	if (!net_priv_info->ioctl)
		return -ENETDOWN;
	pIoctlConfig->net_dev = dev;
	pIoctlConfig->priv_flags = mt_os_net_dev_get_priv_flags(dev);
	pIoctlConfig->cmd = MT_IO_CSI_DOIT;
	pIoctlConfig->data = (void *)info;
	pIoctlConfig->data_len = sizeof(struct genl_info);
	pIoctlConfig->name = req_type;
	ret = net_priv_info->ioctl(dev, pIoctlConfig);

	return 0;
}

int mt_make_common_info(struct sk_buff *nl_skb, struct CSI_DATA_T *tmp_csi_data, void *msginfo)
{
	struct csi_msg_Info *pwdevinfo = (struct csi_msg_Info *)msginfo;

	/*add common info*/
	if (nla_put_u16(nl_skb, CSI_ATTR_PKT_IDX, PARSE_CSI_SEQ_NUM(tmp_csi_data->chain_info)) ||
		nla_put_u32(nl_skb, CSI_ATTR_TS, tmp_csi_data->u4TimeStamp) ||
		nla_put_u8(nl_skb, CSI_ATTR_BAND_IDX, tmp_csi_data->ucDbdcIdx) ||
		nla_put_u16(nl_skb, CSI_ATTR_CHANNEL, pwdevinfo->channel) ||
		nla_put_u8(nl_skb, CSI_ATTR_CBW, tmp_csi_data->ucBw) ||
		nla_put_u8(nl_skb, CSI_ATTR_DBW, tmp_csi_data->ucDataBw) ||
		nla_put_u8(nl_skb, CSI_ATTR_CHAIN_NUM, PARSE_MAX_CHAIN_NUM(tmp_csi_data->chain_info)) ||
		nla_put_u8(nl_skb, CSI_ATTR_CH_IDX, tmp_csi_data->ucPrimaryChIdx) ||
		nla_put_u16(nl_skb, CSI_ATTR_FRAME_MODE, tmp_csi_data->ucRxMode) ||
		nla_put_u8(nl_skb, CSI_ATTR_FRAME_TYPE, FC_TYPE_DATA + (SUBTYPE_QDATA << 2)) ||
		nla_put_u16(nl_skb, CSI_ATTR_CSI_LEN, tmp_csi_data->u2DataCount) ||
		nla_put_u16(nl_skb, CSI_ATTR_MCS_RATE, tmp_csi_data->rx_rate)
	) {
		MT_DEBUG_ERROR("%s fail!!!\n", __func__);
		return -1;
	}

	if (nla_put(nl_skb, CSI_ATTR_RA, MAC_ADDR_LEN, pwdevinfo->bssid)) {
		MT_DEBUG_ERROR("add RA MAC fail!\n");
		return -1;
	}

	if (nla_put(nl_skb, CSI_ATTR_TA, MAC_ADDR_LEN, tmp_csi_data->aucTA)) {
		MT_DEBUG_ERROR("add TA MAC fail!\n");
		return -1;
	}

	if (nla_put_u32(nl_skb, CSI_ATTR_EXTRA_INFO, tmp_csi_data->u4ExtraInfo)) {
		MT_DEBUG_ERROR("add EXTRA INFO fail!\n");
		return -1;
	}

	/*put some csi info to skb cb*/
	SET_CSI_INFO_TO_SKB(nl_skb, CB_CHAIN_NUM, PARSE_MAX_CHAIN_NUM(tmp_csi_data->chain_info));

	return 0;
}

int mt_append_specific_info(struct sk_buff *nl_skb, struct CSI_DATA_T *tmp_csi_data)
{
	struct nlattr *chain_attr = NULL;
	u16 data_count = tmp_csi_data->u2DataCount;

	chain_attr = nla_nest_start(nl_skb, CSI_ATTR_CHAIN_HEADER);
	if (!chain_attr) {
		MT_DEBUG_ERROR("invalid chain_attr!\n");
		return -1;
	}

	if (nla_put_u8(nl_skb, CSI_ATTR_CHAIN_IDX, PARSE_CHAIN_IDX(tmp_csi_data->chain_info)) ||
		nla_put_s8(nl_skb, CSI_ATTR_RSSI, tmp_csi_data->cRssi) ||
		nla_put_u8(nl_skb, CSI_ATTR_SNR, tmp_csi_data->ucSNR) ||
		nla_put_u16(nl_skb, CSI_ATTR_TX_IDX, (u16)(GET_CSI_TX_IDX(tmp_csi_data->Tx_Rx_Idx))) ||
		nla_put_u16(nl_skb, CSI_ATTR_RX_IDX, (u16)(GET_CSI_RX_IDX(tmp_csi_data->Tx_Rx_Idx)))) {
		MT_DEBUG_ERROR("add fragment info fail!\n");
		return -1;
	}

	if (nla_put(nl_skb, CSI_ATTR_I, data_count*sizeof(s16), tmp_csi_data->ac2IData)) {
		MT_DEBUG_ERROR("add I data info fail!\n");
		return -1;
	}

	if (nla_put(nl_skb, CSI_ATTR_Q, data_count*sizeof(s16), tmp_csi_data->ac2QData)) {
		MT_DEBUG_ERROR("add Q data info fail!\n");
		return -1;
	}

	nla_nest_end(nl_skb, chain_attr);

	/*put some csi info to skb cb*/
	SET_CSI_INFO_TO_SKB(nl_skb, CB_CHAIN_IDX, PARSE_CHAIN_IDX(tmp_csi_data->chain_info));

	return 0;
}



PNDIS_PACKET mt_make_csi_nlmsg_fragment(struct CSI_DATA_T *tmp_csi_data, void *info, void *msginfo)
{
	void *msg_header = NULL;
	void *tmp_attr = NULL;
	struct sk_buff *nl_skb = NULL;
	struct genl_info *reply_info = (struct genl_info *)info;
	struct csi_msg_Info *pmsginfo = (struct csi_msg_Info *)msginfo;
	u32 pkt_total_chain = 0;
	u32 pkt_chain_idx = 0;
	u8 new_pkt_flag = 0;
	int type = pmsginfo->reqtype;

	do {
		nl_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

		if (!nl_skb) {
			MT_DEBUG_ERROR("csi alloc skb fail!!!\n");
			goto out;
		}

		/*netlink header*/
		msg_header = genlmsg_put(nl_skb, 0, 0, &csi_genl_family, 0, CSI_OPS_REPORT);

		if (!msg_header) {
			MT_DEBUG_ERROR("csi nlmsg_put fail!!!\n");
			nlmsg_free(nl_skb);
			nl_skb = NULL;
			goto out;
		}

		/*all attr integrate into a nested type*/
		tmp_attr = nla_nest_start(nl_skb, CSI_ATTR_DATA_HEADER);

		if (!tmp_attr) {
			MT_DEBUG_ERROR("invalid tmp_attr!\n");
			nlmsg_free(nl_skb);
			nl_skb = NULL;
			goto out;
		}

		if (mt_make_common_info(nl_skb, tmp_csi_data, pmsginfo) ||
			mt_append_specific_info(nl_skb, tmp_csi_data)) {
			nlmsg_free(nl_skb);
			nl_skb = NULL;
			goto out;
		}

		nla_nest_end(nl_skb, tmp_attr);

		genlmsg_end(nl_skb, msg_header);

		pkt_chain_idx = GET_CSI_INFO_FROM_SKB(nl_skb, CB_CHAIN_IDX);
		pkt_total_chain = GET_CSI_INFO_FROM_SKB(nl_skb, CB_CHAIN_NUM);

		if (!pkt_chain_idx)
			new_pkt_flag = 1;

		if (type & !new_pkt_flag) {
			MT_DEBUG_TRACE("drop csi fragment chain, wait for first chain of a pkt.\n");
			nlmsg_free(nl_skb);
			continue;
		} else {
			if (genlmsg_reply(nl_skb, reply_info)) {
				MT_DEBUG_ERROR("genlmsg_reply fail!\n");
				break;
			}

			/*the last chain of the pkt*/
			if (type & (pkt_chain_idx == (pkt_total_chain - 1))) {
				new_pkt_flag = 0;
				MT_DEBUG_INFO("send total(%d) chains for a whole pkt.\n", pkt_total_chain);
				break;
			}
		}
	}while (type);
out:

	return (PNDIS_PACKET)nl_skb;
}
EXPORT_SYMBOL(mt_make_csi_nlmsg_fragment);

int mt_send_msg_reply(void *info, int attrtype, char *cmd_msg)
{
	struct sk_buff *skb = NULL;
	void *msg_head = NULL;
	struct genl_info *nlinfo = info;

	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

	if (!skb) {
		MT_DEBUG_ERROR(
		"csi skb alloc fail!!!\n");
		return -1;
	}

	msg_head = genlmsg_put(skb, 0, nlinfo->snd_seq + 1, &csi_genl_family, 0, CSI_OPS_REPORT);

	if (!msg_head) {
		MT_DEBUG_ERROR(
		"csi genl header alloc fail!!!\n");
		nlmsg_free(skb);
		return -1;
	}

	if (nla_put_string(skb, attrtype, cmd_msg)) {
		MT_DEBUG_ERROR(
		"nla_put_attr fail!!!\n");
		nlmsg_free(skb);
		return -1;
	}

	genlmsg_end(skb, msg_head);

	if (genlmsg_reply(skb, nlinfo)) {
		MT_DEBUG_ERROR("genl reply status fail!!!\n");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(mt_send_msg_reply);


int mtk_csi_genl_register(void)
{
	int ret;
#if KERNEL_VERSION(4, 10, 0) <= LINUX_VERSION_CODE
	ret = genl_register_family(&csi_genl_family);
#else
	ret = genl_register_family_with_ops(&csi_genl_family, csi_genl_ops);
#endif
	if (ret < 0) {
		MT_DEBUG_TRACE("failed to register CSI genl family(%d)!!!\n", ret);
		return ret;
	}

	MT_DEBUG_TRACE("register CSI genl family(%d)!!!\n", ret);

	return ret;
}
EXPORT_SYMBOL(mtk_csi_genl_register);

int mtk_csi_genl_unregister(void)
{
	int ret;

	ret = genl_unregister_family(&csi_genl_family);

	if (ret < 0) {
		MT_DEBUG_TRACE("failed to unregister CSI genl family(%d)!!!\n", ret);
		return ret;
	}

	MT_DEBUG_TRACE("unregister CSI genl family(%d)!!!\n", ret);

	return ret;
}
EXPORT_SYMBOL(mtk_csi_genl_unregister);

void *mt_csi_proc_create(void *csi_proc_dir, char *data_name, void *data, int type)
{
	struct proc_dir_entry *data_proc = NULL;

	/*for data proc*/
	if (type == CSI_PROC_DATA)
		data_proc = proc_create_data(data_name, 0664, (struct proc_dir_entry *)csi_proc_dir, &csidata_ops, data);
	else if (type == CSI_PROC_DEBUG)
		data_proc = proc_create_data(data_name, 0664, (struct proc_dir_entry *)csi_proc_dir, &csi_debug_ops, data);

	return (void *)data_proc;
}
EXPORT_SYMBOL(mt_csi_proc_create);

/*
*this function prepare a pkt for APP to use.
 *the format of pkt depends on some specific rules as follows.
*/
u32 mt_procCSIDataPrepare(u8 *buf, struct CSI_DATA_T *prCSIData, u8 ucValue1, u8 ucValue2)
{
	u32 i4Pos = 0;
	u8 *tmpBuf = buf;
	u16 u2DataSize = prCSIData->u2DataCount * sizeof(s16);
	u16 u2Rsvd1Size = prCSIData->ucRsvd1Cnt * sizeof(s32);

	/* magic number */
	put_unaligned(0xAA, (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(0xBB, (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(0xCC, (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(0xDD, (tmpBuf + i4Pos));
	i4Pos++;

	/*Just bypass total length feild here and update it in the end*/
	i4Pos += 2;

	put_unaligned(CSI_DATA_VER, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->FWVer, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_TYPE, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->ucBw, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_TS, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(4, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->u4TimeStamp, (u32 *) (tmpBuf + i4Pos));
	i4Pos += 4;

	put_unaligned(CSI_DATA_RSSI, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->cRssi, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_SNR, (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->ucSNR, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_DBW, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->ucDataBw, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_CH_IDX, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(1, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->ucPrimaryChIdx, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;

	put_unaligned(CSI_DATA_TA, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(MAC_ADDR_LEN, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	memmove((tmpBuf + i4Pos), prCSIData->aucTA, MAC_ADDR_LEN);
	i4Pos += MAC_ADDR_LEN;

	put_unaligned(CSI_DATA_EXTRA_INFO, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(4, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->u4ExtraInfo, (u32 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u32);

	put_unaligned(CSI_DATA_I, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(u2DataSize, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	memmove((tmpBuf + i4Pos), prCSIData->ac2IData, u2DataSize);
	i4Pos += u2DataSize;

	put_unaligned(CSI_DATA_Q, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(u2DataSize, (u16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	memmove((tmpBuf + i4Pos), prCSIData->ac2QData, u2DataSize);
	i4Pos += u2DataSize;

	if (ucValue1) {
		put_unaligned(CSI_DATA_RSVD1, (u8 *) (tmpBuf + i4Pos));
		i4Pos++;
		put_unaligned(u2Rsvd1Size, (u16 *) (tmpBuf + i4Pos));
		i4Pos += 2;
		memmove((tmpBuf + i4Pos), prCSIData->ai4Rsvd1, u2Rsvd1Size);
		i4Pos += u2Rsvd1Size;

		put_unaligned(CSI_DATA_RSVD2, (u8 *) (tmpBuf + i4Pos));
		i4Pos++;
		put_unaligned(u2Rsvd1Size, (u16 *) (tmpBuf + i4Pos));
		i4Pos += 2;
		memmove((tmpBuf + i4Pos), prCSIData->au4Rsvd2, u2Rsvd1Size);
		i4Pos += u2Rsvd1Size;

		put_unaligned(CSI_DATA_RSVD3, (u8 *) (tmpBuf + i4Pos));
		i4Pos++;
		put_unaligned(sizeof(s32), (s16 *) (tmpBuf + i4Pos));
		i4Pos += 2;
		put_unaligned(prCSIData->i4Rsvd3, (s32 *) (tmpBuf + i4Pos));
		i4Pos += sizeof(s32);
	}

	if (ucValue2) {
		put_unaligned(CSI_DATA_RSVD4, (u8 *) (tmpBuf + i4Pos));
		i4Pos++;
		put_unaligned(sizeof(u8), (s16 *) (tmpBuf + i4Pos));
		i4Pos += 2;
		put_unaligned(prCSIData->ucRsvd4, (u8 *) (tmpBuf + i4Pos));
		i4Pos += sizeof(u8);
	}

	put_unaligned(CSI_DATA_TX_IDX, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u16), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned((u16)(GET_CSI_TX_IDX(prCSIData->Tx_Rx_Idx)), (u16 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u16);

	put_unaligned(CSI_DATA_RX_IDX, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u16), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned((u16)(GET_CSI_RX_IDX(prCSIData->Tx_Rx_Idx)), (u16 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u16);

	put_unaligned(CSI_DATA_FRAME_MODE, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u16), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->ucRxMode, (u16 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u16);

	/* add antenna pattern*/
	put_unaligned(CSI_DATA_H_IDX, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u32), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->chain_info, (u32 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u32);

	put_unaligned(CSI_DATA_RX_RATE, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u16), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->rx_rate, (u16 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u16);

	put_unaligned(CSI_DATA_PKT_SN, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u16), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->pkt_sn, (u16 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u16);

	put_unaligned(CSI_DATA_TR_STREAM, (u8 *) (tmpBuf + i4Pos));
	i4Pos++;
	put_unaligned(sizeof(u8), (s16 *) (tmpBuf + i4Pos));
	i4Pos += 2;
	put_unaligned(prCSIData->tr_stream, (u8 *) (tmpBuf + i4Pos));
	i4Pos += sizeof(u8);

	/*
	 * The lengths of magic number (4 byte) and total length (2 bytes)
	 * fields should not be counted in the total length value
	*/
	put_unaligned(i4Pos - 6, (u16 *) (tmpBuf + 4));

	return i4Pos;
}
EXPORT_SYMBOL(mt_procCSIDataPrepare);
//#endif /* CFG_SUPPORT_CSI*/

DECLARE_SUB_MODULE_INIT(mt_osal_csi_init)
{
	MT_DEBUG_TRACE("loaded");

	return 0;
}
sub_module_init(mt_osal_csi_init);

DECLARE_SUB_MODULE_EXIT(mt_osal_csi_exit)
{
	MT_DEBUG_TRACE("unloaded");
}
sub_module_exit(mt_osal_csi_exit);
