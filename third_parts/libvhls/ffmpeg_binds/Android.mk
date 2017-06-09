LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../config.mk

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false

ifeq ($(LIVEPLAY_SEEK), true)
 LOCAL_CFLAGS += -DLIVEPLAY_SEEK
endif

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/../hls_main \
        $(LOCAL_PATH)/../common \
        $(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../../../amffmpeg

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wno-multichar

LOCAL_STATIC_LIBRARIES := libhls libhls_http libhls_common libhls_cmf
LOCAL_SHARED_LIBRARIES :=libamplayer libcutils libssl libamavutils libcrypto

ifeq ($(BUILD_WITH_VIEWRIGHT_WEB), true)
 LOCAL_CFLAGS += -DENABLE_VIEWRIGHT_WEB
endif

LOCAL_SHARED_LIBRARIES +=libdl
LOCAL_MODULE := libvhls_mod
#LOCAL_MODULE_PATH:=$(TARGET_OUT_SHARED_LIBRARIES)/amplayer
LOCAL_MODULE_RELATIVE_PATH := amplayer
include $(BUILD_SHARED_LIBRARY)
