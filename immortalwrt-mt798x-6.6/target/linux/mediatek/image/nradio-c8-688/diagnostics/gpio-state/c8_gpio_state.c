// Read-only snapshot of the C8 SIM selector registers.
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>

static int __init c8_gpio_state_init(void)
{
	void __iomem *gpio;
	u32 dir, dout, din, mode;
	unsigned int pin;

	if (!of_machine_is_compatible("HCMT7981-688-emmc"))
		return -ENODEV;
	gpio = ioremap(0x11d00000, 0x1000);
	if (!gpio)
		return -ENOMEM;
	dir = readl(gpio);
	dout = readl(gpio + 0x100);
	din = readl(gpio + 0x200);
	mode = readl(gpio + 0x330);
	pr_info("c8_gpio_state: DIR=%08x DO=%08x DI=%08x MODE3=%08x\n",
		dir, dout, din, mode);
	for (pin = 29; pin <= 30; pin++)
		pr_info("c8_gpio_state: GPIO%u mode=%u output=%u do=%u di=%u\n",
			pin, (mode >> ((pin % 8) * 4)) & 15,
			(dir >> pin) & 1, (dout >> pin) & 1, (din >> pin) & 1);
	iounmap(gpio);
	return 0;
}

static void __exit c8_gpio_state_exit(void) { }
module_init(c8_gpio_state_init);
module_exit(c8_gpio_state_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read-only C8 SIM selector register snapshot");
