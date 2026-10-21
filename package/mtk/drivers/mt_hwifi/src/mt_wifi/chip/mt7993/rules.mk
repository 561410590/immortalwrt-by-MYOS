ifneq ($(findstring mt7993,$(CHIPSET)),)
CHIP_7993_PATH := $(PARENT_DIR)/chip/mt7993

obj_chips += \
	$(CHIP_7993_PATH)/mt7993.o\
	$(CHIP_7993_PATH)/mt7993_dbg.o\
	$(CHIP_7993_PATH)/mt7993_ser.o
endif
