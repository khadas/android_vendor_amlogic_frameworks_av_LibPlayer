LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../../config.mk
LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 		

LOCAL_C_INCLUDES := $(TOP)/external/curl/include \
        $(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../../../amffmpeg \
        $(LOCAL_PATH)/../include

LOCAL_STATIC_LIBRARIES += libcurl_base libcurl_common
LOCAL_SHARED_LIBRARIES += libcurl libutils libamavutils libamplayer liblog

LOCAL_MODULE := libcurl_mod
LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
#LOCAL_MODULE_PATH:=$(TARGET_OUT_SHARED_LIBRARIES)/amplayer
LOCAL_MODULE_RELATIVE_PATH := amplayer
include $(BUILD_SHARED_LIBRARY)
