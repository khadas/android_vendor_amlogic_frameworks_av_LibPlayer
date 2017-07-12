LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(BOARD_AML_MEDIA_HAL_CONFIG)
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional


LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/../../../amffmpeg \
        $(AMAVUTILS_PATH)/include \
        $(LOCAL_PATH)/../common

LOCAL_STATIC_LIBRARIES := libdash_wrapper libdash libxml2 libcurl_base libcurl_common

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 6.0))
LOCAL_SHARED_LIBRARIES := libcurl libicuuc libamplayer libcutils
else
LOCAL_SHARED_LIBRARIES := libstlport libcurl libicuuc libamplayer libcutils
endif

LOCAL_MODULE := libdash_mod
LOCAL_MODULE_RELATIVE_PATH := amplayer
include $(BUILD_SHARED_LIBRARY)
