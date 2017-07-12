LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := libdash_test.cpp 

LOCAL_C_INCLUDES := \
    	$(LOCAL_PATH)/../libdash/include \
    	$(LOCAL_PATH)/../common

ifneq (0, $(shell expr $(PLATFORM_VERSION) \< 6.0))
include external/stlport/libstlport.mk
endif

LOCAL_STATIC_LIBRARIES := libdash_wrapper libdash libxml2 libcurl_base libcurl_common

ifneq (0, $(shell expr $(PLATFORM_VERSION) \< 6.0))
LOCAL_SHARED_LIBRARIES := libstlport libcurl libicuuc libcutils
else
LOCAL_SHARED_LIBRARIES := libcurl libicuuc libcutils
endif

LOCAL_MODULE := libdash_test 

include $(BUILD_EXECUTABLE)
