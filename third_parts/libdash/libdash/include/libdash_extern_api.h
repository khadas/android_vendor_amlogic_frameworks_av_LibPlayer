#ifndef LIBDASH_EXTERN_API_H_
#define LIBDASH_EXTERN_API_H_

#include "dash_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void extern_set_interrupt_cb(URL_IRQ_CB cb);
int extern_interrupt_cb();

#ifdef __cplusplus
}
#endif

#endif