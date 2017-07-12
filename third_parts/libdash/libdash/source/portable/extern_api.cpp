#include "libdash_extern_api.h"

static int (*interrupt_cb_ext)() = NULL;

void extern_set_interrupt_cb(URL_IRQ_CB cb)
{
    interrupt_cb_ext = cb;
}

int extern_interrupt_cb()
{
    int ret = interrupt_cb_ext();
    if (ret) {
        LOGI("extern_interrupt_cb !\n");
    }
    return ret;
}