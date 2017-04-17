LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(TOP)/hardware/amlogic/media/media_base_config.mk

LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/libudrm \
    $(AMAVUTILS_PATH)/include

LOCAL_SRC_FILES := udrm.c

LOCAL_SHARED_LIBRARIES += libcutils libutils libc libudrm2w2_Android libamavutils

LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE := libudrm
#LOCAL_MODULE_PATH:=$(TARGET_OUT_SHARED_LIBRARIES)/amplayer

include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
