#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

KSYMTAB_FUNC(nf_nat_fullcone_dying_tuple_list_add, "_gpl", "");
KSYMTAB_FUNC(nf_nat_fullcone_handle_dying_tuples, "_gpl", "");
KSYMTAB_FUNC(nf_nat_fullcone_destroy_mappings, "_gpl", "");
KSYMTAB_FUNC(nf_nat_fullcone_ipv4, "_gpl", "");
KSYMTAB_FUNC(nf_nat_fullcone_ipv6, "_gpl", "");

MODULE_INFO(depends, "nf_tables,nf_nat");

