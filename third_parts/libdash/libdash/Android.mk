LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false

TEMP_SRC_FILES := $(wildcard $(LOCAL_PATH)/source/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/helpers/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/manager/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/metrics/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/mpd/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/network/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/portable/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/source/xml/*.cpp)
LOCAL_SRC_FILES := $(TEMP_SRC_FILES:$(LOCAL_PATH)/%=%)

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
ICU_DIR=$(TOP)/external/icu/icu4c/source/common
CURL_MOD_DIR=$(TOP)/vendor/amlogic/frameworks/av/LibPlayer/third_parts/libcurl-ffmpeg/include
else
ICU_DIR=$(TOP)/external/icu4c/common
CURL_MOD_DIR=$(TOP)/packages/amlogic/LibPlayer/third_parts/libcurl-ffmpeg/include
endif

LOCAL_C_INCLUDES := \
		$(TOP)/external/curl/include \
		$(TOP)/external/libxml2/include \
		$(ICU_DIR) \
		$(CURL_MOD_DIR) \
		$(LOCAL_PATH)/include \
		$(LOCAL_PATH)/../common

ifneq (0, $(shell expr $(PLATFORM_VERSION) \< 6.0))
include external/stlport/libstlport.mk
endif

LOCAL_SHARED_LIBRARIES :=libcurl
LOCAL_STATIC_LIBRARIES := libxml2 libcurl_base libcurl_common

LOCAL_MODULE := libdash
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
