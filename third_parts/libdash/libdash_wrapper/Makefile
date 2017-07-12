M_PREFIX=$(LIBPLAYER_STAGING_DIR)/usr
ROOTFS?=$(M_PREFIX)
LIB_DIR?=$(M_PREFIX)/lib
HEADERS_DIR?=$(M_PREFIX)/include

DIRS := $(shell find . -maxdepth 3 -type d)
FILES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cpp))
TARGET_OBJS=$(patsubst %.cpp,%.o, $(FILES))
INSTALL_DIR=${LIB_DIR}/libplayer/
TOPDIR=$(shell pwd)
SRC?=$(TOPDIR)

TARGET=libdash_wrapper.a

CFLAGS+=-O2 -fPIC
CFLAGS+=-I$(SRC) -I$(SRC)/../libdash/include -I$(SRC)/../common -I$(SRC)/Buffer/*.h -I$(SRC)/helpers/*.h -I$(SRC)Input/*.h -I$(SRC)/MPD/*.h -I$(SRC)/Portable/*.h
LDFLAGS += -lm -lz -lpthread -ldl -lrt -lstdc++ -lcutils

all: $(TARGET) 
	$(MAKE) install

$(TARGET): $(TARGET_OBJS)
	$(AR) cq $(TARGET) $(TARGET_OBJS)

$(TARGET_OBJS):%.o:%.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

install:
	-install -m 555 ${TARGET} $(INSTALL_DIR)
	cp	-rf $(SRC)/*.h  $(HEADERS_DIR)/
