#CC=${HOST_GCC}

#export CC BUILD_DIR STAGING_DIR TARGET_DIR
all:
	mkdir -p $(shell pwd)/../../staging/usr/lib/libdash
	-$(MAKE) -C libdash/source all
	-$(MAKE) -C libdash/source install
	-$(MAKE) -C libdash_wrapper all
	-$(MAKE) -C libdash_wrapper install
	-$(MAKE) -C ffmpeg_binds all
	-$(MAKE) -C ffmpeg_binds install

install:
	-$(MAKE) -C test install

clean:
	-$(MAKE) -C ffmpeg_binds clean
	-$(MAKE) -C libdash clean
	-$(MAKE) -C libdash_wrapper  clean

