intermediates := $(local-intermediates-dir)

SRC := $(call my-dir)/include/vnd_jordan.txt

ifneq ($(BOARD_BLUETOOTH_LIBBT_VNDCFG),)
# use board config if available
SRC := $(BOARD_BLUETOOTH_LIBBT_VNDCFG)
endif

GEN := $(intermediates)/vnd_buildcfg.h
TOOL := $(TOP_DIR)external/bluetooth/bluedroid/tools/gen-buildcfg.sh

$(GEN): PRIVATE_PATH := $(call my-dir)
$(GEN): PRIVATE_CUSTOM_TOOL = $(TOOL) $< $@
$(GEN): $(SRC)  $(TOOL)
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES += $(GEN)
