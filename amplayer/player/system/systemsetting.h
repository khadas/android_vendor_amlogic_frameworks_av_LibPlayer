/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#ifndef GET_SYSTEM_SETTING_HHHH
#define GET_SYSTEM_SETTING_HHHH

int GetSystemSettingString(const char *path, char *value, char *defaultv);
float PlayerGetSettingfloat(const char* path);
int PlayerSettingIsEnable();
int PlayerGetVFilterFormat();
int PlayerGetAFilterFormat(const char *prop);

#endif

