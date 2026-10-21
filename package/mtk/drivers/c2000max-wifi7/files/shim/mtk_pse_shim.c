#include <linux/module.h>
#include <linux/types.h>

int mtk_set_pse_drop(u32 pse_port, u32 enable)
{
	return 0;
}
EXPORT_SYMBOL(mtk_set_pse_drop);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C2000MAX WiFi PSE compatibility symbol");
