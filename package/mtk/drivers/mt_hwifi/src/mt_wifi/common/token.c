/*
 * Copyright (c) [2020], MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws.
 * The information contained herein is confidential and proprietary to
 * MediaTek Inc. and/or its licensors.
 * Except as otherwise provided in the applicable licensing terms with
 * MediaTek Inc. and/or its licensors, any reproduction, modification, use or
 * disclosure of MediaTek Software, and information contained herein, in whole
 * or in part, shall be strictly prohibited.
*/
/***************************************************************************
 ***************************************************************************

*/

#include "rtmp_comm.h"

#ifdef LINUX
#include "os/rt_linux_cmm.h"
#include "os/rt_drv.h"
#endif /* LINUX */

#include "rt_os_util.h"
#include "rt_config.h"

static const UINT16 tx_free_notify_record_boundary[TX_FREE_NOTIFY_DEEP_STAT_SIZE]
					= {10, 20, 30, 40, 50, 60, 128, 200, 256, 312, 400, 512};

INT token_tx_setting(RTMP_ADAPTER *pAd, UINT8 q_idx, INT32 option,
								INT32 sub_option, INT32 value)
{
	INT32 ret = 0;
	PKT_TOKEN_CB *cb = hc_get_ct_cb(pAd->hdev_ctrl);
	struct token_tx_pkt_queue *que = &cb->que[q_idx];

	if (sub_option < 0)
		return ret;

	switch (option) {
	case TOKEN_WATERMARK:
		if (sub_option == TOKEN_LOWMARK)
			token_tx_set_lwmark(que, value);
		else if (sub_option == TOKEN_HIGHMARK)
			token_tx_set_hwmark(que, value);
		else
			ret = 1;
		break;
	case TOKEN_BOUNDARY:
		if (sub_option < TX_FREE_NOTIFY_DEEP_STAT_SIZE)
			que->deep_stat[sub_option].boundary = value;
		else
			ret = 1;
		break;
	case TOKEN_DEBUG:
		if (value)
			cb->dbg |= (1 << sub_option);
		else
			cb->dbg &= ~(1 << sub_option);
		break;
	default:
		ret = 1;
		MTWF_DBG(pAd, DBG_CAT_TOKEN, TOKEN_INFO, DBG_LVL_ERROR,
			"unknown token option %d\n", option);
	}

	return ret;
}

inline struct token_tx_pkt_queue *token_tx_get_queue_by_band(PKT_TOKEN_CB *cb, UINT32 band_idx)
{
	if (cb->que_nums != 2)
		return &cb->que[0];
	else
		return &cb->que[band_idx];
}

inline struct token_tx_pkt_queue *token_tx_get_queue_by_token_id(PKT_TOKEN_CB *cb, UINT32 token_id)
{
	struct token_tx_pkt_queue *que = &cb->que[0];

	if (cb->que_nums != 2) {
		return que;
	} else {
		if (token_id <= que->pkt_tkid_max)
			return que;
		else
			return &cb->que[1];
	}
}

inline BOOLEAN token_tx_get_state(struct token_tx_pkt_queue *que)
{
	return test_bit(0, &que->token_state);
}

inline INT token_tx_set_state(struct token_tx_pkt_queue *que, BOOLEAN state)
{
	if (state == TX_TOKEN_LOW)
		set_bit(0, &que->token_state);
	else
		clear_bit(0, &que->token_state);

	return NDIS_STATUS_SUCCESS;
}

inline VOID token_tx_inc_full_cnt(struct token_tx_pkt_queue *que)
{
	que->token_full_cnt++;
}

inline UINT32 token_tx_get_free_cnt(struct token_tx_pkt_queue *que)
{
	return atomic_read(&que->free_token_cnt);
}

inline VOID token_tx_set_lwmark(struct token_tx_pkt_queue *que, UINT32 value)
{
	que->low_water_mark = value;
}

inline UINT32 token_tx_get_lwmark(struct token_tx_pkt_queue *que)
{
	return que->low_water_mark;
}

inline VOID token_tx_set_hwmark(struct token_tx_pkt_queue *que, UINT32 value)
{
	que->high_water_mark = value;
}

inline UINT32 token_tx_get_hwmark(struct token_tx_pkt_queue *que)
{
	return que->high_water_mark;
}

inline VOID token_tx_record_free_notify(struct token_tx_pkt_queue *que, UINT32 token_cnt)
{
	UINT8 i = 0;

	do {
		if (token_cnt < que->deep_stat[i].boundary ||
				(i == (TX_FREE_NOTIFY_DEEP_STAT_SIZE - 1))) {
			que->deep_stat[i].cnt++;
			break;
		}
	} while (++i < TX_FREE_NOTIFY_DEEP_STAT_SIZE);
}

UINT32 token_rx_dmad_init(struct token_rx_pkt_queue *que, PNDIS_PACKET pkt,
								ULONG alloc_size, PVOID alloc_va, NDIS_PHYSICAL_ADDRESS alloc_pa)
{
	UINT32 token_id;

	token_id = atomic_read(&que->cur_free_idx);
	que->pkt_token[token_id].pkt_buf = pkt;
	que->pkt_token[token_id].dma_buf.AllocSize = alloc_size;
	que->pkt_token[token_id].dma_buf.AllocVa = alloc_va;
	que->pkt_token[token_id].dma_buf.AllocPa = alloc_pa;
	atomic_inc(&que->cur_free_idx);

	return token_id;
}
EXPORT_SYMBOL(token_rx_dmad_init);

INT token_rx_dmad_lookup(struct token_rx_pkt_queue *que, UINT32 token_id, PNDIS_PACKET *pkt,
							PVOID *alloc_va, NDIS_PHYSICAL_ADDRESS *alloc_pa)
{
	INT32 ret = 0;

	*pkt = que->pkt_token[token_id].pkt_buf;
	*alloc_va = que->pkt_token[token_id].dma_buf.AllocVa;
	*alloc_pa = que->pkt_token[token_id].dma_buf.AllocPa;

	return ret;
}

INT token_rx_dmad_lookup_pa(struct token_rx_pkt_queue *que, UINT32 *token_id, PNDIS_PACKET *pkt,
							PVOID *alloc_va, NDIS_PHYSICAL_ADDRESS alloc_pa)
{
	INT32 ret = -1, token_loop = 0;

	for (token_loop = 0 ; token_loop < que->pkt_tkid_cnt ; token_loop++) {
		if (alloc_pa == que->pkt_token[token_loop].dma_buf.AllocPa) {
			*token_id = token_loop;
			*alloc_va = que->pkt_token[token_loop].dma_buf.AllocVa;
			*pkt = que->pkt_token[token_loop].pkt_buf;

			MTWF_DBG(NULL, DBG_CAT_TOKEN, TOKEN_INFO, DBG_LVL_DEBUG,
				"tkn_rx_id: %d according to %04x\n",
				*token_id, (UINT32)alloc_pa);

			ret = 0;
		}
	}

	return ret;
}

EXPORT_SYMBOL(token_rx_dmad_lookup);

INT token_rx_dmad_update(struct token_rx_pkt_queue *que, UINT32 token_id, PNDIS_PACKET pkt,
								ULONG alloc_size, PVOID alloc_va, NDIS_PHYSICAL_ADDRESS alloc_pa)
{
	INT32 ret = 0;

	que->pkt_token[token_id].pkt_buf = pkt;
	que->pkt_token[token_id].dma_buf.AllocSize = alloc_size;
	que->pkt_token[token_id].dma_buf.AllocVa = alloc_va;
	que->pkt_token[token_id].dma_buf.AllocPa = alloc_pa;

	return ret;
}

