#ifndef __MTK_DEBUGFS_H__
#define __MTK_DEBUGFS_H__

struct mtk_hw_dev;

int
mtk_dbg_init_device(struct mtk_hw_dev *dev);

int
mtk_dbg_exit_device(struct mtk_hw_dev *dev);

#endif
