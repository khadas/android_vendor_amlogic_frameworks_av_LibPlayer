M_PREFIX=$(LIBPLAYER_STAGING_DIR)/usr
ROOTFS?=$(M_PREFIX)
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

TARGET_OBJS=libdash_mod.o ffmpeg_dash_demuxer.o ffmpeg_dash_protocol.o

TARGET=libdash_mod.so

INSTALL_DIR=${LIB_DIR}/libplayer/

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC
CFLAGS+= -I$(SRC)/../common -I$(SRC)/../../../amffmpeg/ -I$(SRC)/../../../amffmpeg/libavutil/ -I$(STAGING_DIR)/usr/include/libxml2
LDFLAGS+= -L$(M_PREFIX)/lib/libplayer -lavutil -lavformat -lavcodec -lm -lz -lpthread -lamcodec -ldl -lamplayer -lamadec -lasound -lamstreaming -L$(STAGING_DIR)/usr/lib/aml_libs -lamavutils -lrt -lstdc++ -lcurl -lxml2

all: $(TARGET)
	$(MAKE) install

$(TARGET): $(TARGET_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS:%.o=%.c) -shared -o $@  -Wl,--whole-archive -L$(M_PREFIX)/lib/libplayer -lcurl_common -lcurl_base -ldash_wrapper -ldash  -Wl,--no-whole-archive

install:
	-install -m 555 ${TARGET} $(INSTALL_DIR)

force:

clean:
	-rm -rf *.o

