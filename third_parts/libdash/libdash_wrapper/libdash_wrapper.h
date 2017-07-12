#ifndef LIBDASH_WRAPPER_H
#define LIBDASH_WRAPPER_H
#include <stdint.h>
#include "dash_common.h"

typedef struct _DASH_SESSION_CONTEXT{
	void *dashsession;		// DASHSession
	int64_t seekStartTime;	// mircsecond
	int session_type;
	int flags;
}DASH_SESSION_CONTEXT;

typedef struct _DASH_CONTEXT{
	void * dashmanager;
	int64_t durationUs;						// millisecond
	int nb_session;
	DASH_SESSION_CONTEXT sessions[2];
}DASH_CONTEXT;

#ifdef __cplusplus 
extern "C" { 
#endif
       int64_t GetDuration(const char *strDuration);

	int dash_probe(const char *url, URL_IRQ_CB cb);

	int dash_open(DASH_CONTEXT *dashCtx,const char *url, URL_IRQ_CB cb);

	// return: -1 failed to seek; success to really time(second).
	int dash_seek(DASH_CONTEXT *dashCtx, int64_t pos);			// pos: second

	//int dash_open_stream(void *dashstream);

	int dash_read(DASH_SESSION_CONTEXT *sessionCtx, uint8_t *data, size_t len);

	//void dash_close_stream(void *dashstream);
	
	void dash_close(DASH_CONTEXT *dashCtx);
	 
#ifdef __cplusplus 
} 
#endif 


#endif
