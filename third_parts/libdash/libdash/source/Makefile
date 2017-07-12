M_PREFIX=$(LIBPLAYER_STAGING_DIR)/usr
ROOTFS?=$(M_PREFIX)
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

DIRS := $(shell find . -maxdepth 3 -type d)
FILES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cpp))
TARGET_OBJS=$(patsubst %.cpp,%.o, $(FILES))
TARGET=libdash.a

INSTALL_DIR=${LIB_DIR}/libplayer/

TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

CFLAGS+=-O2 -fPIC
CFLAGS+= -I$(SRC)/../include -I$(SRC)/../../common -I$(STAGING_DIR)/usr/include/libxml2  -I$(SRC)/../../../libcurl-ffmpeg/include -I$(STAGING_DIR)/usr/include/curl
LDFLAGS+=-lm -lz -lpthread -ldl -lrt -lstdc++ -lcurl  -lxml2

all: $(TARGET)
	$(MAKE) install

$(TARGET): $(TARGET_OBJS)
	$(AR) cq $(TARGET) $(TARGET_OBJS)

$(TARGET_OBJS):%.o:%.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

install:
	echo $(FILES)
	echo $(TARGET_OBJS)
	echo $(CXX)
	-install -m 555 ${TARGET} $(INSTALL_DIR)
	cp	-rf $(SRC)/../include/*.h  $(HEADERS_DIR)/
	cp	-rf $(SRC)/../../common/*.h  $(HEADERS_DIR)/

