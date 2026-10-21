SRC_DIR := $(src)

obj-m += mt_wifi_osal.o

CONFIG_FUZZ_SUPPORT?=n

ccflags-y += -I$(src)/inc
ccflags-y += -include linux/version.h
GLOBAL_INCLUDES_NL80211 = include/uapi/linux/mtk_nl80211_inc
ccflags-y += -I$(GLOBAL_INCLUDES_NL80211)
ifneq ($(LINUX_DIR),)
ccflags-y += -I$(LINUX_DIR)/include/uapi/linux/mtk_nl80211_inc
ccflags-y += -I$(LINUX_DIR)/user_headers/include
endif

ifeq ($(CONFIG_FUZZ_SUPPORT),y)
ccflags-y += -D__MOCK
endif

ifneq ($(CFG_CFG80211_VERSION),)
VERSION_STR = $(subst \",,$(subst ., , $(subst -, ,$(subst v,,$(CFG_CFG80211_VERSION)))))
X = $(firstword $(VERSION_STR))
Y = $(word 2 ,$(VERSION_STR))
Z = $(word 3 ,$(VERSION_STR))
ifeq ($Z, )
    Z = 0
endif
VERSION := $(shell echo "$$(( $X * 65536 + $Y * 256 + $Z))" )
ccflags-y += -DCFG_CFG80211_VERSION=$(VERSION)
endif

mt_wifi_osal-objs := mt_osal_main.o
mt_wifi_osal-objs += mt_osal_type.o
mt_wifi_osal-objs += mt_osal_debug.o
mt_wifi_osal-objs += mt_osal_mem.o
mt_wifi_osal-objs += mt_osal_dts_emi.o
mt_wifi_osal-objs += mt_osal_packet.o
mt_wifi_osal-objs += mt_osal_util.o
mt_wifi_osal-objs += mt_osal_crypto.o
mt_wifi_osal-objs += mt_osal_ipc.o
mt_wifi_osal-objs += mt_osal_lock.o
mt_wifi_osal-objs += mt_osal_timer.o
mt_wifi_osal-objs += mt_osal_task.o
mt_wifi_osal-objs += mt_osal_net.o
mt_wifi_osal-objs += mt_osal_fs.o
mt_wifi_osal-objs += mt_osal_wifi.o
mt_wifi_osal-objs += mt_osal_bus.o
mt_wifi_osal-objs += mt_osal_system.o
mt_wifi_osal-objs += mt_osal_iw_ext.o
ifeq ($(CONFIG_CFG80211_SUPPORT),y)
mt_wifi_osal-objs += mt_osal_cfg80211_vndr.o
mt_wifi_osal-objs += mt_osal_cfg80211.o
mt_wifi_osal-objs += mt_osal_cfg80211_event.o
endif
mt_wifi_osal-objs += mt_osal_csi.o
mt_wifi_osal-objs += mt_osal_proc.o
mt_wifi_osal-objs += mt_osal_diag.o

ldflags-y += -T $(SRC_DIR)/mt_osal.lds