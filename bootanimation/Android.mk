LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := bootanimation
LOCAL_MODULE_SUFFIX := .zip
LOCAL_MODULE_PATH := $(TARGET_OUT)/media

include $(BUILD_SYSTEM)/base_rules.mk

MY_intermediates := $(call local-intermediates-dir)
BOOTANIMATION_FRAMES := $(wildcard $(LOCAL_PATH)/480/*/*.jpg)
BOOTANIMATION_DESC := $(LOCAL_PATH)/480/desc.txt
BOOTANIMATION_INTERMEDIATE_DIR := $(LOCAL_BUILT_MODULE).dir

$(BOOTANIMATION_INTERMEDIATE_DIR):
	mkdir -p $@

$(BOOTANIMATION_INTERMEDIATE_DIR)/text.txt: FORCE
	(date) > $@

$(BOOTANIMATION_INTERMEDIATE_DIR)/text.pbm: $(BOOTANIMATION_INTERMEDIATE_DIR)/text.txt | $(BOOTANIMATION_INTERMEDIATE_DIR)
	cat $<|pbmtext|pnminvert > $@

$(BOOTANIMATION_INTERMEDIATE_DIR)/%.jpg: $(LOCAL_PATH)/480/%.jpg $(BOOTANIMATION_INTERMEDIATE_DIR)/text.pbm | $(BOOTANIMATION_INTERMEDIATE_DIR)
	mkdir -p $(dir $@)
	(jpegtopnm $<|pnmcomp -align=center -valign=middle -alpha=$(word 2,$^) $(word 2,$^)|pnmtojpeg) > $@

$(BOOTANIMATION_INTERMEDIATE_DIR)/desc.txt: $(LOCAL_PATH)/480/desc.txt | $(BOOTANIMATION_INTERMEDIATE_DIR)
	cat $< > $@

$(BOOTANIMATION_INTERMEDIATE_DIR)/bootanimation.zip: $(patsubst $(LOCAL_PATH)/480/%,$(BOOTANIMATION_INTERMEDIATE_DIR)/%,$(BOOTANIMATION_FRAMES) $(BOOTANIMATION_DESC)) | $(BOOTANIMATION_INTERMEDIATE_DIR)
	(cd $(BOOTANIMATION_INTERMEDIATE_DIR)/; zip $@ -0r desc.txt part0 part1)

$(LOCAL_BUILT_MODULE): $(BOOTANIMATION_INTERMEDIATE_DIR)/bootanimation.zip  | $(BOOTANIMATION_INTERMEDIATE_DIR)
	mkdir -p $(dir $@)
	cat $< > $@

#######################################
