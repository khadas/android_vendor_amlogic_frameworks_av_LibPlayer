/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Luan.Yuan
 *  @version  1.0
 *  @date     2017/02/22
 *  @par function description:
 */

Bluray Library contains two git directorys:
    1. vendor/amlogic/external/libbluray
    2. vendor/amlogic/frameworks/av/LibPlayer/third_parts/libbluray

	git clone git://git.myamlogic.com/vendor/amlogic/external/libbluray
	git clone git://git.myamlogic.com/vendor/amlogic/frameworks/av/LibPlayer/third_parts/libbluray

Need two library to decoder Bluray File:
    libbluray_mod.so  libbluray.so

The process is as follows:
Go to [vendor/amlogic/external] directory
1. git clone git://git.myamlogic.com/vendor/amlogic/external/libbluray
2. execute [mm] command in vendor/amlogic/external/libbluray can generate libbluray.so.
3. remove vendor/amlogic/frameworks/av/LibPlayer/third_parts/libbluray directory and\
   execute [git clone git://git.myamlogic.com/vendor/amlogic/frameworks/av/LibPlayer/third_parts/libbluray] in third_parts

4. cp 2nd libbluray.so to this directory, and execute [mm] to generate libbluray_mod.so.

This directory has two .mk files: Android.mk and Android.mk.1
Android.mk : use libbluray.so and bluray.c etc, to generate libbluray_mod.so
Android.mk.1: cp libbluray_mod.so and libbluray.so in this directory to out/.

or you can add code as follows in Android.mk, then it will generate libbluray_mod.so and cp libbluray.so to out/.

#####################################################
include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE := libbluray.so
LOCAL_MODULE_TAGS := optional
LOCAL_IS_HOST_MODULE := true

LOCAL_SRC_FILES := libbluray.so
LOCAL_MODULE_PATH:=$(TARGET_OUT)/lib

include $(BUILD_PREBUILT)
#######################################################


