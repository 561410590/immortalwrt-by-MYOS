/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CDC_NCM_RX_WALK_H
#define CDC_NCM_RX_WALK_H

#include "usbnet_rx_budget.h"

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t u32;
#endif

/* Internal cursor for an immutable USB Network Transfer Block (NTB). */
struct ncm_rx_cursor {
	u32 end, ndp, next_ndp, entry, last;
	u32 tables, payload;
	bool open_table;
};

enum ncm_rx_result {
	NCM_RX_BAD = -1, NCM_RX_END, NCM_RX_SKIP, NCM_RX_FRAME, NCM_RX_YIELD,
};

static inline u32 ncm_get16(const unsigned char *p)
{
	return (u32)p[0] | (u32)p[1] << 8;
}

static inline u32 ncm_get32(const unsigned char *p)
{
	return ncm_get16(p) | ncm_get16(p + 2) << 16;
}

static inline bool ncm_rx_begin(struct ncm_rx_cursor *c,
				const unsigned char *data, u32 bytes,
				u32 max, bool ntb32)
{
	u32 header = ntb32 ? 16 : 12;
	u32 block;

	if (bytes < header || bytes > max ||
	    ncm_get32(data) != (ntb32 ? 0x686d636eU : 0x484d434eU) ||
	    ncm_get16(data + 4) != header)
		return false;

	block = ntb32 ? ncm_get32(data + 8) : ncm_get16(data + 8);
	/* A zero block length denotes the actual USB transfer size. */
	if (block && (block < header || block > bytes))
		return false;

	*c = (struct ncm_rx_cursor) {
		.end = block ? block : bytes,
		.ndp = ntb32 ? ncm_get32(data + 12) : ncm_get16(data + 10),
	};
	return !c->ndp || c->ndp >= header;
}

/*
 * One call examines at most one NDP header or one DPE. Charge both against
 * the work limit so empty, foreign or cyclic tables cannot monopolize RX.
 */
static inline enum ncm_rx_result
ncm_rx_next(struct ncm_rx_cursor *c, const unsigned char *data, bool ntb32,
	    u32 *offset, u32 *len)
{
	u32 head = ntb32 ? 16 : 8;
	u32 step = ntb32 ? 8 : 4;
	u32 header = ntb32 ? 16 : 12;
	u32 length;
	const unsigned char *p;

	if (!c->ndp)
		return NCM_RX_END;
	if (!c->open_table) {
		if (++c->tables > 51 || c->ndp < header || c->ndp > c->end ||
		    c->end - c->ndp < head)
			return NCM_RX_BAD;

		p = data + c->ndp;
		length = ncm_get16(p + 4);
		if (length < head + step || (length - head) % step ||
		    length > c->end - c->ndp)
			return NCM_RX_BAD;

		c->next_ndp = ntb32 ? ncm_get32(p + 8) : ncm_get16(p + 6);
		if (c->next_ndp == c->ndp)
			return NCM_RX_BAD;
		if (ncm_get32(p) != (ntb32 ? 0x306d636eU : 0x304d434eU)) {
			c->ndp = c->next_ndp;
			return NCM_RX_SKIP;
		}

		c->entry = c->ndp + head;
		c->last = c->ndp + length - step;
		c->open_table = true;
		return NCM_RX_SKIP;
	}

	p = data + c->entry;
	*offset = ntb32 ? ncm_get32(p) : ncm_get16(p);
	*len = ntb32 ? ncm_get32(p + 4) : ncm_get16(p + 2);
	if (!*offset && !*len) {
		c->ndp = c->next_ndp;
		c->open_table = false;
		return NCM_RX_SKIP;
	}
	if (c->entry == c->last || *offset < header || *len < 14 ||
	    *offset > c->end || *len > c->end - *offset ||
	    *len > c->end - c->payload)
		return NCM_RX_BAD;

	c->entry += step;
	c->payload += *len;
	return NCM_RX_FRAME;
}

/* cursor.end is zero only before a valid NTH has been consumed. */
static inline enum ncm_rx_result
ncm_rx_advance(struct ncm_rx_cursor *c, const unsigned char *data, u32 bytes,
	       u32 max, bool ntb32, struct usbnet_rx_budget *budget,
	       u32 *offset, u32 *len)
{
	enum ncm_rx_result result;

	if (c->end && !c->ndp)
		return NCM_RX_END;
	if (!budget->packets || !budget->steps)
		return NCM_RX_YIELD;

	budget->steps--;
	if (!c->end)
		return ncm_rx_begin(c, data, bytes, max, ntb32) ?
			NCM_RX_SKIP : NCM_RX_BAD;

	result = ncm_rx_next(c, data, ntb32, offset, len);
	if (result == NCM_RX_FRAME)
		budget->packets--;
	return result;
}

#endif
