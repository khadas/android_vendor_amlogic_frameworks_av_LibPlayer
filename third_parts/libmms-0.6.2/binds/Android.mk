LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(LOCAL_PATH)/../../../config.mk
LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c)) 		

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
        $(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../../../amffmpeg


LOCAL_STATIC_LIBRARIES += libmms libiconv 
LOCAL_SHARED_LIBRARIES += libutils libamavutils libamplayer liblog

LOCAL_MODULE := libmms_mod
LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
#LOCAL_MODULE_PATH:=$(TARGET_OUT_SHARED_LIBRARIES)/amplayer
LOCAL_MODULE_RELATIVE_PATH := amplayer
include $(BUILD_SHARED_LIBRARY)
