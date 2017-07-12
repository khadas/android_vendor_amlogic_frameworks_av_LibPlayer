LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(BOARD_AML_MEDIA_HAL_CONFIG)
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_C_INCLUDES := $(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../../amffmpeg \
		$(BOARD_AML_VENDOR_PATH)/external/libbluray/src \
        $(LOCAL_PATH)/../../amplayer/player/include \
        $(AMACODEC_PATH)\

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wno-multichar
LOCAL_CFLAGS += -DLOG_ENABLE=0

LOCAL_SHARED_LIBRARIES :=libamplayer libcutils libbluray libdl

LOCAL_MODULE := libbluray_mod
LOCAL_MODULE_RELATIVE_PATH:=$(TARGET_OUT)/lib/amplayer
include $(BUILD_SHARED_LIBRARY)

