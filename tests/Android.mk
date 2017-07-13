LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../config.mk
LOCAL_MODULE    := testlibplayer
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := testlibplayer.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
    $(AMCODEC_NEED_INCLUDE)\
	$(LOCAL_PATH)/../amffmpeg \
    $(JNI_H_INCLUDE) 

LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils
LOCAL_SHARED_LIBRARIES += libamcodec libamavutils libamplayer
include $(BUILD_EXECUTABLE)

