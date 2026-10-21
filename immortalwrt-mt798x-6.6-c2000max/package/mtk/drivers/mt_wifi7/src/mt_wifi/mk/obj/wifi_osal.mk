ifeq ($(TARGET),LINUX)
WIFI_OSAL_PATH := $(PARENT_DIR)/../wifi_osal

obj_wifi_osal += \
	$(WIFI_OSAL_PATH)/mt_osal_bus.o\
	$(WIFI_OSAL_PATH)/mt_osal_crypto.o\
	$(WIFI_OSAL_PATH)/mt_osal_debug.o\
	$(WIFI_OSAL_PATH)/mt_osal_fs.o\
	$(WIFI_OSAL_PATH)/mt_osal_ipc.o\
	$(WIFI_OSAL_PATH)/mt_osal_lock.o\
	$(WIFI_OSAL_PATH)/mt_osal_mem.o\
	$(WIFI_OSAL_PATH)/mt_osal_net.o\
	$(WIFI_OSAL_PATH)/mt_osal_packet.o\
	$(WIFI_OSAL_PATH)/mt_osal_system.o\
	$(WIFI_OSAL_PATH)/mt_osal_task.o\
	$(WIFI_OSAL_PATH)/mt_osal_timer.o\
	$(WIFI_OSAL_PATH)/mt_osal_type.o\
	$(WIFI_OSAL_PATH)/mt_osal_util.o\
	$(WIFI_OSAL_PATH)/mt_osal_wifi.o\
	$(WIFI_OSAL_PATH)/mt_osal_iw_ext.o\
	$(WIFI_OSAL_PATH)/mt_osal_dts_emi.o

ifeq ($(CONFIG_CFG80211_SUPPORT),y)
obj_wifi_osal += \
	$(WIFI_OSAL_PATH)/mt_osal_cfg80211.o\
	$(WIFI_OSAL_PATH)/mt_osal_cfg80211_vndr.o\
	$(WIFI_OSAL_PATH)/mt_osal_cfg80211_event.o
endif
endif

