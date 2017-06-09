LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../config.mk

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 		

LOCAL_C_INCLUDES := $(TOP)/external/curl/include \
		$(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../../../amffmpeg \
        $(LOCAL_PATH)/../include

LOCAL_SHARED_LIBRARIES += libcurl libutils libamavutils libamplayer

LOCAL_MODULE := libcurl_base
LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_STATIC_LIBRARY)
