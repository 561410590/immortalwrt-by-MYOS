ATE_PATH := $(PARENT_DIR)/ate

ifeq ($(CONFIG_ATE_SUPPORT),y)

obj_ate += \
	$(ATE_PATH)/ate_agent.o

ifeq ($(CONFIG_MT_FMAC),y)
obj_ate +=
endif

ifeq ($(CONFIG_MT_BMAC),y)
obj_ate +=
endif

########################################################
# Wlan service related files
########################################################
ifeq ($(CONFIG_WLAN_SERVICE),y)
SERV_DIR := $(ATE_PATH)/wlan_service
obj_ate += \
	$(SERV_DIR)/agent/agent.o\
	$(SERV_DIR)/service/service_test.o\
	$(SERV_DIR)/glue/hal/logan/test_dmac.o\
	$(SERV_DIR)/glue/hal/logan/operation_logan.o\
	$(SERV_DIR)/glue/osal/logan/net_adaption_logan.o\
	$(SERV_DIR)/glue/osal/logan/sys_adaption_logan.o
endif

endif
