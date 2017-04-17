
#include <android/log.h>
#include <cutils/properties.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "amthreadpool.h"
#include "unitend.h"
#include "udrm.h"

#define  LOG_TAG    "libplayer_udrm"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define UDRM_CONFIG_PATH "/data/unitcfg.txt"
#define UDRM_ACCOUNT_NAME "AccountName"
#define UDRM_ACCOUNT_PWD "AccountPassword"
#define UDRM_DRM_URL "DRMURL"
#define UDRM_AUTH_URL "AuthorizationURL"

#define UDRM_URL_LEN 1024
#define UDRM_PWD_LEN 512

static int s_init = 0;
static char s_auth_url[UDRM_URL_LEN] = {0};
static udrm_notify s_fnNotify = NULL;
static void *s_data = NULL;
static int s_error_num = -1;

UTI_SINT32 UDRMMsg(IN UTI_UINT8 *pu8Msg, IN UTI_UINT32 u32Len, IN UTI_SINT32 s32UDRMError, IN decrypt_status status, IN UTI_VOID* pCallbackData)
{
    if (pu8Msg != NULL && pu8Msg[0] == 0x01) { //udrm msg
        UTI_CHAR Msg[UDRM_MAX_MSG_LEGNTH];
        UTI_UINT16  MsgLength = 0;
        MsgLength = pu8Msg[2];
        MsgLength = (MsgLength << 8) + pu8Msg[1];
        //printf("-----------UDRMInfo:%s\n", pu8Msg+3);
        LOGI("[%s] UDRMInfo:%s\n", __FUNCTION__, pu8Msg + 3);
        LOGI("[%s] status=%d, ErrorNum=%d\n", __FUNCTION__, status, s32UDRMError);
        if (s32UDRMError < 0 && s_fnNotify != NULL) {
            s_fnNotify((int)s32UDRMError, s_data);
        }
    }
    return 0;
}

void *udrm_thread_t(void *arg)
{
    UTI_RESULT ret = -1;
    FILE *fd = NULL;
    char cfg_data[UDRM_URL_LEN] = {0};
    char mac[PROPERTY_VALUE_MAX] = {0};
    char dev_name[PROPERTY_VALUE_MAX] = {0};
    char account_name[UDRM_URL_LEN] = {0};
    char account_pwd[UDRM_PWD_LEN] = {0};
    char drm_url[UDRM_URL_LEN] = {0};
    int url_len = 0;

    if (s_init) {
        LOGI("[%s] already inited.\n", __FUNCTION__);
        return NULL;
    }

    LOGI("[%s] begin\n", __FUNCTION__);
    usleep(5000000);
    LOGI("[%s] wait end\n", __FUNCTION__);

    //get mac & dev_name
    property_get("ro.mac", mac, NULL);
    property_get("ro.product.device", dev_name, NULL);
    LOGI("[%s] mac=%s, dev_name=%s\n", __FUNCTION__, mac, dev_name);

    fd = fopen(UDRM_CONFIG_PATH, "rb");
    if (fd == NULL) {
        LOGE("[%s] can not open %s!\n", __FUNCTION__, UDRM_CONFIG_PATH);
        return NULL;
    }

    //get account_name & account_pwd & drm_url & auth_url
    while (fgets(cfg_data, UDRM_URL_LEN, fd) != NULL) {
        if (strncmp(cfg_data, UDRM_ACCOUNT_NAME, strlen(UDRM_ACCOUNT_NAME)) == 0
            && strlen(cfg_data) > (strlen(UDRM_ACCOUNT_NAME) + 2)) {
            strcpy(account_name, &cfg_data[strlen(UDRM_ACCOUNT_NAME) + 1]);
            url_len = strlen(account_name);
            if (account_name[url_len - 1] == '\r' || account_name[url_len - 1] == '\n') {
                account_name[url_len - 1] = 0;
            }
            if (account_name[url_len - 2] == '\r' || account_name[url_len - 2] == '\n') {
                account_name[url_len - 2] = 0;
            }
            LOGI("[%s] account_name=%s, len=%d(%d)\n", __FUNCTION__, account_name, strlen(account_name), url_len);
        } else if (strncmp(cfg_data, UDRM_ACCOUNT_PWD, strlen(UDRM_ACCOUNT_PWD)) == 0
                   && strlen(cfg_data) > (strlen(UDRM_ACCOUNT_PWD) + 2)) {
            strcpy(account_pwd, &cfg_data[strlen(UDRM_ACCOUNT_PWD) + 1]);
            url_len = strlen(account_pwd);
            if (account_pwd[url_len - 1] == '\r' || account_pwd[url_len - 1] == '\n') {
                account_pwd[url_len - 1] = 0;
            }
            if (account_pwd[url_len - 2] == '\r' || account_pwd[url_len - 2] == '\n') {
                account_pwd[url_len - 2] = 0;
            }
            LOGI("[%s] account_pwd=%s\n", __FUNCTION__, account_pwd);
        } else if (strncmp(cfg_data, UDRM_DRM_URL, strlen(UDRM_DRM_URL)) == 0
                   && strlen(cfg_data) > (strlen(UDRM_DRM_URL) + 2)) {
            strcpy(drm_url, &cfg_data[strlen(UDRM_DRM_URL) + 1]);
            url_len = strlen(drm_url);
            if (drm_url[url_len - 1] == '\r' || drm_url[url_len - 1] == '\n') {
                drm_url[url_len - 1] = 0;
            }
            if (drm_url[url_len - 2] == '\r' || drm_url[url_len - 2] == '\n') {
                drm_url[url_len - 2] = 0;
            }
            LOGI("[%s] drm_url=%s\n", __FUNCTION__, drm_url);
        } else if (strncmp(cfg_data, UDRM_AUTH_URL, strlen(UDRM_AUTH_URL)) == 0
                   && strlen(cfg_data) > (strlen(UDRM_AUTH_URL) + 2)) {
            strcpy(s_auth_url, &cfg_data[strlen(UDRM_AUTH_URL) + 1]);
            url_len = strlen(s_auth_url);
            if (s_auth_url[url_len - 1] == '\r' || s_auth_url[url_len - 1] == '\n') {
                s_auth_url[url_len - 1] = 0;
            }
            if (s_auth_url[url_len - 2] == '\r' || s_auth_url[url_len - 2] == '\n') {
                s_auth_url[url_len - 2] = 0;
            }
            LOGI("[%s] s_auth_url=%s\n", __FUNCTION__, s_auth_url);
        }
    }

    fclose(fd);

    if (account_name[0] == 0
        || account_pwd[0] == 0
        || drm_url[0] == 0
        || s_auth_url[0] == 0) {
        LOGE("[%s] udrm config data no enough!\n", __FUNCTION__);
        return NULL;
    }

    do {
        usleep(5000000);
        LOGI("[%s] begin init udrm\n", __FUNCTION__);
        ret = UDRMAgentInit();
        if (ret != UDRM_ERROR_OK) {
            LOGE("[%s] UDRMAgentInit error! ret=%d\n", __FUNCTION__, ret);
            continue;
        }

        UDRMAgentInitDeviceLocalPath("/data");
        ret = UDRMAgentSetEnv(account_name, account_pwd, mac, dev_name, drm_url);
        if (ret != UDRM_ERROR_OK) {
            LOGE("[%s] UDRMAgentSetEnv error! ret=%d\n", __FUNCTION__, ret);
            UDRMAgentDestroy();
            continue;
        }

        ret = UDRMAgentCheckBindStatus();
        if (ret != UDRM_ERROR_OK) {
            LOGI("[%s] begin bind device\n", __FUNCTION__);
            ret = UDRMAgentBindDevice(account_name, account_pwd, mac, dev_name);
            if (ret != UDRM_ERROR_OK) {
                LOGE("[%s] UDRMAgentBindDevice error! ret=%d\n", __FUNCTION__, ret);
                UDRMAgentDestroy();
                s_error_num = ret;
                continue;
            }
        }

        LOGI("[%s] success!\n", __FUNCTION__);
        s_init = 1;
        break;
    } while (1);

    return NULL;
}

int udrm_init()
{
    int ret = 0;
    pthread_t tid;
    pthread_attr_t pthread_attr;

    LOGI("[%s]\n", __FUNCTION__);
    pthread_attr_init(&pthread_attr);
    pthread_attr_setstacksize(&pthread_attr, 1024 * 1024);
    ret = amthreadpool_pthread_create(&tid, &pthread_attr, (void*)&udrm_thread_t, NULL);
    if (ret != 0) {
        LOGE("[%s]  udrm_thread_t create error!\n", __FUNCTION__);
        return ret;
    }
    pthread_setname_np(tid, "udrm_bind_thread");
    pthread_attr_destroy(&pthread_attr);
    return 0;
}

void udrm_deinit()
{
    LOGI("[%s]\n", __FUNCTION__);
    if (s_init) {
        UDRMAgentDestroy();
    }
}

void udrm_set_msg_func(udrm_notify notify_cb, void *cb_data)
{
    LOGI("[%s] %s\n", __FUNCTION__, notify_cb == NULL ? "NULL" : "notify_cb");
    s_fnNotify = notify_cb;
    s_data = cb_data;
}

int udrm_decrypt_start(unsigned int program_num)
{
    UTI_VOID *handle = NULL;
    UTI_RESULT ret = -1;
    if (!s_init) {
        LOGI("[%s] send notify. error_num = %d\n", __FUNCTION__, s_error_num);
        s_fnNotify(s_error_num, s_data);
        return -1;
    }

    ret = UDRMAgentCheckBindStatus();
    if (ret != UDRM_ERROR_OK) {
        LOGE("[%s] UDRMAgentCheckBindStatus error! ret=%d\n", __FUNCTION__, ret);
        s_fnNotify((int)ret, s_data);
        return -1;
    }

    handle = UDRMAgentDecryptStart(program_num, s_auth_url);
    if (handle == NULL) {
        LOGE("[%s] UDRMAgentDecryptStart error!\n", __FUNCTION__);
        s_fnNotify((int)UDRM_ERROR_UNKNOWN_ERROR, s_data);
        return -1;
    }

    ret = UDRMPlaySetUDRMMsgFunc(handle, (UDRMMsgCallback)UDRMMsg, NULL);
    if (ret != UDRM_ERROR_OK) {
        LOGE("[%s] UDRMPlaySetUDRMMsgFunc error! ret=%d\n", __FUNCTION__, ret);
    }

    LOGI("[%s] handle=0x%x\n", __FUNCTION__, (int)handle);
    return (int)handle;
}

int udrm_decrypt_stop(int handle)
{
    UTI_SINT32 ret = -1;
    if (!s_init) {
        return -1;
    }

    LOGI("[%s] handle=0x%x\n", __FUNCTION__, handle);
    ret = UDRMAgentDecryptStop((void *)handle);
    if (ret != UDRM_ERROR_OK) {
        LOGE("[%s] UDRMAgentDecryptStop error! ret=%d\n", __FUNCTION__, ret);
        return -1;
    }

    return 0;
}

int udrm_ts_decrypt(int handle, char *ts_in, unsigned int len_in, char *ts_out, unsigned int len_out)
{
    UTI_SINT32 ret = -1;
    if (!s_init) {
        return -1;
    }

    //LOGI("[%s] handle=0x%x, len_in=%d\n", __FUNCTION__, handle, len_in);
    ret = UDRMAgentDecryptTS((void *)handle, (UTI_UINT8*)ts_in,
                             (UTI_UINT32)len_in, (UTI_UINT8*)ts_out, (UTI_UINT32)len_out);
    if (ret < 0) {
        LOGI("[%s] ts decrypt error! ret=%d\n", __FUNCTION__, ret);
        if (ret == UDRM_ERROR_INVALID_ARGUMENT) {
            LOGE("[%s] invalid argument!\n", __FUNCTION__);
        } else if (ret == UDRM_ERROR_NOT_ENOUGH_MEMORY) {
            LOGE("[%s] not enough memory!\n", __FUNCTION__);
        }
        return -1;
    }

    return (int)ret;
}

int udrm_mp4_set_pssh(int handle, unsigned char *pssh, unsigned int len)
{
    UTI_SINT32 ret = -1;
    if (!s_init) {
        return -1;
    }

    LOGI("[%s] handle=0x%x, len=%d\n", __FUNCTION__, handle, len);
    ret = UDRMSetMp4PsshData((void *)handle, (UTI_UINT8 *)pssh, (UTI_UINT32)len);
    LOGI("[%s] ret=%d\n", __FUNCTION__, ret);
    return 0;
}

int udrm_mp4_decrypt(int handle, char *iv_value, char *enc_data, unsigned int len, char *out_data)
{
    UTI_SINT32 ret = -1;
    if (!s_init) {
        return -1;
    }

    //LOGI("[%s] handle=0x%x\n", __FUNCTION__, handle);
    ret = UDRMPlayLoadMp4_data((void *)handle, iv_value, enc_data, (UTI_UINT32)len, out_data);
    if (ret < 0) {
        LOGI("[%s] mp4 decrypt error! ret=%d\n", __FUNCTION__, ret);
        if (ret == UDRM_ERROR_INVALID_ARGUMENT) {
            LOGE("[%s] invalid argument!\n", __FUNCTION__);
        } else if (ret == UDRM_ERROR_NOT_ENOUGH_MEMORY) {
            LOGE("[%s] not enough memory!\n", __FUNCTION__);
        }
        return -1;
    }

    return (int)ret;
}

