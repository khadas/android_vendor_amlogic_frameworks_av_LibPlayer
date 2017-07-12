_PREFIX=$(LIBDASH_STAGING_DIR)
ROOTFS?=$(M_PREFIX)
LIB_DIR?=$(M_PREFIX)/usr/lib
HEADERS_DIR?=$(M_PREFIX)/usr/include

src=$(wildcard ./source/*.c ./source/helpers/*.c ./source/manager/*.c ./source/metrics/*.c ./source/mpd /*.c ./source/network/*.c ./source/portable/*.c ./source/portable/*.c)
dir=$(notdir $(src))
TARGET_OBJS=$(patsubst %.c,%.o,$(dir) )

TARGET=libdash.a

INSTALL_DIR=${LIB_DIR}/libplayer/third_parts/libdash

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC
CFLAGS+=-I$include -I$(SRC)/../../common -I$(STAGING_DIR)/usr/include/libxml2/libxml/ -I$(SRC)/../../libcurl-ffmpeg/include -I$(STAGING_DIR)/usr/include/curl
LDFLAGS + = libxml2.a libcurl_base.a libcurl_common.a

LDFLAGS += -llibcurl


export CC CFLAGS

all: $(TARGET)
	$(MAKE) install

$(TARGET): $(TARGET_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS:%.o=%.c) -o $@

install:
	-install -m 555 ${TARGET} $(INSTALL_DIR)
	cp	-rf $(SRC)/*.h  $(HEADERS_DIR)/

