/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef USBNET_RX_POOL_H
#define USBNET_RX_POOL_H

#include <linux/mm.h>
#include <linux/page_ref.h>

#define USBNET_RX_POOL_RESERVE 5
#define USBNET_RX_POOL_MAX 64
#define USBNET_RX_SIZE_MAX (64 * 1024)

enum usbnet_rx_pause {
	USBNET_RX_DOWN = BIT(0),
	USBNET_RX_SUSPENDED = BIT(1),
};

struct usbnet_rx_slot {
	struct sk_buff *skb;
	struct page *page;
	void *data;
	dma_addr_t dma;
};

struct usbnet_rx_cb {
	struct skb_data entry;
	struct usbnet_rx_slot *slot;
};

struct usbnet_rx_pool {
	struct napi_struct napi;
	struct sk_buff_head free;
	/* Both slot queues use free.lock. Only NAPI can issue a new loan. */
	struct sk_buff_head held;
	struct sk_buff_head done;
	struct sk_buff *partial;
	struct usbnet *dev;
	void *cursor;
	size_t size;
	unsigned int order;
	unsigned int count;
	unsigned int backlog_max;
	unsigned int loan_limit;
	struct mutex state_lock;
	unsigned int paused;
	atomic64_t stats[USBNET_RX_STATS_COUNT];
	atomic_t completing;
	wait_queue_head_t callbacks_idle;
	bool coherent;
	struct usbnet_rx_slot slots[];
};

static unsigned int rx_pool_mode = 1;
module_param(rx_pool_mode, uint, 0444);
MODULE_PARM_DESC(rx_pool_mode, "NCM lab RX: 0=legacy, 1=streaming pool, 2=coherent pool, 3=budget only");

static unsigned int rx_pool_copybreak;
module_param(rx_pool_copybreak, uint, 0444);
MODULE_PARM_DESC(rx_pool_copybreak, "Minimum frame length for one shared page fragment per NTB (0=off; mode 1 only)");

static unsigned int rx_pool_ab_mode = 1;
module_param(rx_pool_ab_mode, uint, 0644);
MODULE_PARM_DESC(rx_pool_ab_mode, "A/B on a mode 1 device: 1 uses the pool; 3 bypasses it without rebinding");

static bool rx_pool_gro;
module_param(rx_pool_gro, bool, 0644);
MODULE_PARM_DESC(rx_pool_gro, "Send decoded frames through GRO in the existing NCM NAPI poll");

static struct usbnet_rx_cb *usbnet_rx_cb(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct usbnet_rx_cb) > sizeof(skb->cb));
	return (struct usbnet_rx_cb *)skb->cb;
}

static void usbnet_rx_count(struct usbnet_rx_pool *pool,
			    enum usbnet_rx_stat index)
{
	atomic64_inc(&pool->stats[index]);
}

u64 usbnet_rx_stat(const struct usbnet *dev, enum usbnet_rx_stat index)
{
	if (!dev->rx_pool || (unsigned int)index >= USBNET_RX_STATS_COUNT)
		return 0;
	return atomic64_read(&dev->rx_pool->stats[index]);
}

void usbnet_rx_note_shared(struct usbnet *dev)
{
	if (dev->rx_pool)
		usbnet_rx_count(dev->rx_pool, USBNET_RX_SHARED_FRAG);
}

void usbnet_rx_lab_return(struct usbnet *dev, struct sk_buff *skb)
{
	struct usbnet_rx_pool *pool = dev->rx_pool;

	if (!READ_ONCE(rx_pool_gro) || test_bit(EVENT_RX_PAUSED, &dev->flags)) {
		usbnet_skb_return(dev, skb);
		return;
	}
	if (!skb->protocol)
		skb->protocol = eth_type_trans(skb, dev->net);
	dev_sw_netstats_rx_add(dev->net, skb->len);
	memset(skb->cb, 0, sizeof(struct skb_data));
	if (skb_defer_rx_timestamp(skb))
		return;
	usbnet_rx_count(pool, USBNET_RX_GRO_FRAME);
	napi_gro_receive(&pool->napi, skb);
}

static struct sk_buff *usbnet_rx_pool_get(struct usbnet_rx_pool *pool,
					 size_t size)
{
	struct sk_buff *skb;
	unsigned long flags;

	if ((rx_pool_mode == 1 && READ_ONCE(rx_pool_ab_mode) == 3) ||
	    !pool->count || pool->size != size)
		return NULL;

	spin_lock_irqsave(&pool->free.lock, flags);
	/* Rotate one outstanding loan per submission, bounding IRQ-off work. */
	skb = __skb_dequeue(&pool->held);
	if (skb) {
		struct page *page = usbnet_rx_cb(skb)->slot->page;

		if (page_ref_count(page) == 1)
			__skb_queue_tail(&pool->free, skb);
		else
			__skb_queue_tail(&pool->held, skb);
	}
	skb = __skb_dequeue(&pool->free);
	spin_unlock_irqrestore(&pool->free.lock, flags);
	if (skb)
		usbnet_rx_count(pool, USBNET_RX_POOL_HIT);
	return skb;
}

static void usbnet_rx_put(struct usbnet *dev, struct sk_buff *skb)
{
	struct usbnet_rx_pool *pool = dev->rx_pool;
	struct usbnet_rx_slot *slot;
	unsigned long flags;

	if (!pool || !usbnet_rx_cb(skb)->slot) {
		dev_kfree_skb_any(skb);
		return;
	}

	slot = usbnet_rx_cb(skb)->slot;
	spin_lock_irqsave(&pool->free.lock, flags);
	if (rx_pool_copybreak && slot->page && page_ref_count(slot->page) > 1)
		__skb_queue_tail(&pool->held, skb);
	else
		__skb_queue_tail(&pool->free, skb);
	spin_unlock_irqrestore(&pool->free.lock, flags);
}

static u32 usbnet_rx_share_threshold(struct usbnet_rx_pool *pool,
				    struct usbnet_rx_slot *slot)
{
	unsigned long flags;
	bool available;

	if (!rx_pool_copybreak || !slot || !slot->page)
		return 0;
	spin_lock_irqsave(&pool->free.lock, flags);
	available = pool->free.qlen && pool->held.qlen < pool->loan_limit;
	spin_unlock_irqrestore(&pool->free.lock, flags);
	return available ? rx_pool_copybreak : 0;
}

static void usbnet_rx_finish(struct usbnet *dev, struct sk_buff *skb)
{
	usb_free_urb(usbnet_rx_cb(skb)->entry.urb);
	usbnet_rx_put(dev, skb);
}

static unsigned char *usbnet_rx_data(struct sk_buff *skb)
{
	struct usbnet_rx_slot *slot = usbnet_rx_cb(skb)->slot;

	return slot ? slot->data : skb->data;
}

static int usbnet_rx_poll(struct napi_struct *napi, int budget)
{
	struct usbnet_rx_pool *pool = container_of(napi, struct usbnet_rx_pool, napi);
	struct usbnet *dev = pool->dev;
	struct usbnet_rx_budget left = usbnet_rx_budget_init(budget);

	while (left.packets > 0) {
		struct sk_buff *skb = pool->partial;
		struct usbnet_rx_slot *slot;
		int ret;

		if (!skb) {
			skb = skb_dequeue(&pool->done);
			if (!skb)
				break;
			memset(pool->cursor, 0, dev->driver_info->rx_cursor_size);
			pool->partial = skb;
		}

		slot = usbnet_rx_cb(skb)->slot;
		ret = dev->driver_info->rx_walk(dev, usbnet_rx_data(skb),
					      usbnet_rx_cb(skb)->entry.length,
					      slot ? slot->page : NULL,
					      slot && slot->page ? PAGE_SIZE << pool->order : 0,
					      usbnet_rx_share_threshold(pool, slot),
					      pool->cursor, &left);
		if (ret == -EAGAIN) {
			usbnet_rx_count(pool, left.packets ? USBNET_RX_SCAN_YIELD :
					USBNET_RX_BUDGET_YIELD);
			return budget;
		}
		if (ret < 0)
			dev->net->stats.rx_errors++;
		pool->partial = NULL;
		usbnet_rx_finish(dev, skb);
	}

	/* Each producer calls napi_schedule(), which records a missed wakeup. */
	if (left.packets > 0)
		napi_complete_done(napi, budget - left.packets);

	return budget - left.packets;
}

static void usbnet_rx_drain(struct usbnet_rx_pool *pool)
{
	struct sk_buff *skb;
	unsigned int blocks = 0;

	if (pool->partial) {
		usbnet_rx_finish(pool->dev, pool->partial);
		pool->partial = NULL;
		blocks++;
	}
	while ((skb = skb_dequeue(&pool->done))) {
		usbnet_rx_finish(pool->dev, skb);
		blocks++;
	}
	atomic64_add(blocks, &pool->stats[USBNET_RX_FLUSH_NTBS]);
}

static void usbnet_rx_set_paused(struct usbnet *dev, enum usbnet_rx_pause reason,
				 bool paused)
{
	struct usbnet_rx_pool *pool = dev->rx_pool;
	unsigned int old;

	if (!pool)
		return;

	mutex_lock(&pool->state_lock);
	old = pool->paused;
	if (paused)
		pool->paused |= reason;
	else
		pool->paused &= ~reason;
	if (!old && pool->paused) {
		/* Caller already closed its admission gate in defer_bh(). */
		spin_lock_irq(&dev->rxq.lock);
		spin_unlock_irq(&dev->rxq.lock);
		napi_disable(&pool->napi);
		usbnet_rx_drain(pool);
	} else if (old && !pool->paused) {
		napi_enable(&pool->napi);
	}
	mutex_unlock(&pool->state_lock);
}

/*
 * Wait after rxq drains. Resubmitting callbacks take a count before
 * leaving rxq, and cannot submit new work while stopped or suspended.
 */
static void usbnet_rx_wait_callbacks(struct usbnet *dev)
{
	struct usbnet_rx_pool *pool = dev->rx_pool;

	if (!pool)
		return;

	wait_event(pool->callbacks_idle, !atomic_read(&pool->completing));
	spin_lock_irq(&pool->callbacks_idle.lock);
	spin_unlock_irq(&pool->callbacks_idle.lock);
}

static void usbnet_rx_complete_put(struct usbnet_rx_pool *pool)
{
	unsigned long flags;

	if (!pool)
		return;

	spin_lock_irqsave(&pool->callbacks_idle.lock, flags);
	if (atomic_dec_and_test(&pool->completing))
		wake_up_locked(&pool->callbacks_idle);
	spin_unlock_irqrestore(&pool->callbacks_idle.lock, flags);
}

static void usbnet_rx_pool_free(struct usbnet *dev)
{
	struct usbnet_rx_pool *pool = dev->rx_pool;
	unsigned int i;

	if (!pool)
		return;
	netif_napi_del(&pool->napi);
	for (i = 0; i < pool->count; i++) {
		struct usbnet_rx_slot *slot = &pool->slots[i];

		if (slot->page)
			put_page(slot->page);
		else if (slot->data)
			usb_free_coherent(dev->udev, pool->size, slot->data, slot->dma);
		kfree_skb(slot->skb);
	}
	kfree(pool->cursor);
	kfree(pool);
	dev->rx_pool = NULL;
}

static void usbnet_rx_pool_init(struct usbnet *dev)
{
	struct usbnet_rx_pool *pool;
	unsigned int count, i;

	if (rx_pool_mode < 1 || rx_pool_mode > 3 || !dev->driver_info->rx_walk)
		return;
	if (!dev->rx_urb_size || dev->rx_urb_size > USBNET_RX_SIZE_MAX)
		return;

	count = rx_pool_mode == 3 ? 0 :
		min_t(unsigned int, RX_QLEN(dev) + USBNET_RX_POOL_RESERVE,
		      USBNET_RX_POOL_MAX);
	pool = kzalloc(struct_size(pool, slots, count), GFP_KERNEL);
	if (!pool)
		goto fallback;

	pool->cursor = kzalloc(dev->driver_info->rx_cursor_size, GFP_KERNEL);
	if (!pool->cursor) {
		kfree(pool);
		goto fallback;
	}
	pool->dev = dev;
	pool->size = dev->rx_urb_size;
	pool->order = get_order(pool->size);
	pool->coherent = rx_pool_mode == 2;
	pool->count = count;
	/* Keep at least the target in-flight depth and one unloaned spare. */
	if (count > RX_QLEN(dev) + 1)
		pool->loan_limit = count - RX_QLEN(dev) - 1;
	pool->backlog_max = max_t(unsigned int, 16, 2 * RX_QLEN(dev));
	skb_queue_head_init(&pool->free);
	__skb_queue_head_init(&pool->held);
	mutex_init(&pool->state_lock);
	pool->paused = USBNET_RX_DOWN;
	skb_queue_head_init(&pool->done);
	init_waitqueue_head(&pool->callbacks_idle);
	atomic_set(&pool->completing, 0);
	netif_napi_add(dev->net, &pool->napi, usbnet_rx_poll);
	dev->rx_pool = pool;

	for (i = 0; i < count; i++) {
		struct usbnet_rx_slot *slot = &pool->slots[i];

		slot->skb = alloc_skb(0, GFP_KERNEL);
		if (!slot->skb)
			goto free;
		if (pool->coherent) {
			slot->data = usb_alloc_coherent(dev->udev, pool->size,
						       GFP_KERNEL, &slot->dma);
		} else {
			slot->page = alloc_pages(GFP_KERNEL | __GFP_COMP,
						 pool->order);
			if (slot->page)
				slot->data = page_address(slot->page);
		}
		if (!slot->data)
			goto free;
		usbnet_rx_cb(slot->skb)->slot = slot;
		skb_queue_tail(&pool->free, slot->skb);
	}
	netdev_info(dev->net, "NCM lab RX mode %u, %u x %zu bytes\n",
		    rx_pool_mode, count, pool->size);
	return;
free:
	usbnet_rx_pool_free(dev);
fallback:
	netdev_warn(dev->net, "NCM lab RX init failed, using legacy RX\n");
}

#endif
