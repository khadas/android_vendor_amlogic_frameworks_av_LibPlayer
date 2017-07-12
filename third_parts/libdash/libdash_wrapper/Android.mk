LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false

TEMP_SRC_FILES := $(wildcard $(LOCAL_PATH)/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/Buffer/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/Input/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/MPD/*.cpp)
TEMP_SRC_FILES += $(wildcard $(LOCAL_PATH)/Portable/*.cpp)
LOCAL_SRC_FILES := $(TEMP_SRC_FILES:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
    	$(LOCAL_PATH)/../libdash/include \
    	$(LOCAL_PATH)/../common

ifneq (0, $(shell expr $(PLATFORM_VERSION) \< 6.0))
include external/stlport/libstlport.mk
endif

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_MODULE := libdash_wrapper
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
