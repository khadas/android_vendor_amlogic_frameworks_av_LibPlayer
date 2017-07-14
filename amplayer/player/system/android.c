/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#include <cutils/properties.h>

#include <sys/system_properties.h>




int GetSystemSettingString(const char *path, char *value, char *defaultv)
{
    return property_get(path, value, defaultv);
}


