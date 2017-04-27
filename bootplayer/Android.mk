LOCAL_PATH := $(call my-dir)

ifeq ($(BUILD_WITH_BOOT_PLAYER),true)
include $(TOP)/hardware/amlogic/media/media_base_config.mk 

include $(CLEAR_VARS)
LOCAL_MODULE    := bootplayer
LOCAL_MODULE_TAGS := samples
LOCAL_SRC_FILES := bootplayer.c
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../amplayer/player/include \
	$(AMCODEC_NEED_INCLUDE)\
    $(LOCAL_PATH)/../amffmpeg \
    $(LOCAL_PATH)/../streamsource \
    $(JNI_H_INCLUDE) \

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec_alsa  libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libasound libamavutils
else
LOCAL_STATIC_LIBRARIES := libamplayer libamplayer libamcodec libavformat librtmp libavcodec libavutil libamadec_alsa libamavutils_alsa  libiconv
LOCAL_SHARED_LIBRARIES += libutils libmedia libbinder libz libdl libcutils libssl libcrypto libasound
endif

include $(BUILD_EXECUTABLE)

endif

