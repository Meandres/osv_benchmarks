VERSION=3.29
configure_flags = --disable-numa --disable-rdma --disable-rados --disable-rbd \
	--disable-http --disable-gfapi --disable-pmem --disable-shm \
	--disable-tcmalloc --dynamic-libengines

INCLUDES= -Ifio -I../.. -I../../include -I../../arch/common -I../../arch/x64 -I../../build/release.x64/gen/include

.PHONY: module clean distclean

module: fio/fio ioengine

fio:
	git clone --depth 1 --single-branch -b fio-$(VERSION) http://git.kernel.dk/fio.git
	cd fio; patch -p1 < ../patches/0001-configure-disable-POSIX-pshared-attribute.patch

fio/fio: fio
	cd fio; ./configure $(configure_flags) --cc=g++; $(MAKE)

ioengine: ioengine_ufs.cc
	g++ $(INCLUDES) -g -O3 -shared -fPIC -o ioengine_ufs.so ioengine_ufs.cc

clean:
	if [ -d fio ]; then cd fio && $(MAKE) clean; fi

distclean:
	rm -rf fio
