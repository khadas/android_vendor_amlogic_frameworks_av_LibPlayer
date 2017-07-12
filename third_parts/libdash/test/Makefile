PREFIX=$(LIBPLAYER_STAGING_DIR)/usr
TARGET_DIR=$(LIBPLAYER_TARGET_DIR)/usr
LIB_DIR=$(PREFIX)/lib/libplayer
BUILD_DIR=$(shell pwd)

libdash_test_s=libdash_test.cpp

INSTALL_FLAGS=-m 755

TARGET= libdash_test 

LDFLAGS= -L$(PREFIX)/lib -lxml2  -ldl -lrt -lstdc++  -lm  -lpthread -L$(PREFIX)/lib/libplayer  -static -ldash_wrapper -ldash  -lcurl_base -lcurl_common
CFLAGS+=-I${BUILD_DIR}/../libdash/include -I${BUILD_DIR}/../common
CFLAGS+=-O2 -g
CXX= $(STAGING_DIR)/../../bin/arm-linux-gnueabihf-g++ -I${BUILD_DIR}/../common -I${BUILD_DIR}/../libdash/include -I${BUILD_DIR}/../libdash_wrapper/input

.PHONY: all config  before_cmd install clean distclean

all: ${TARGET}

install:all
	install $(INSTALL_FLAGS) $(TARGET) $(TARGET_DIR)/bin

distclean:clean
    
clean:
	rm $(TARGET_DIR)/bin/$(TARGET)

${TARGET}:libdash_test.o
	$(CXX) $(CFLAGS) $(LDFLAGS) $($(@:%=%_s)) -o $@

