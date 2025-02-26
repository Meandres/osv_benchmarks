VERSION=3.29
configure_flags = --disable-numa --disable-rdma --disable-rados --disable-rbd \
	--disable-http --disable-gfapi --disable-pmem --disable-shm \
	--disable-tcmalloc

INCLUDES= -Ifio -I../.. -I../../include -I../../arch/common -I../../arch/x64

.PHONY: module clean distclean

module: fio/fio ioengine

fio:
	git clone --depth 1 --single-branch -b fio-$(VERSION) http://git.kernel.dk/fio.git
	cd fio; patch -p1 < ../patches/0001-configure-disable-POSIX-pshared-attribute.patch

fio/fio: fio
	cd fio; ./configure $(configure_flags); $(MAKE)

ioengine: ioengine_nvme.c
	g++ $(INCLUDES) -O3 -shared -rdynamic -fPIC -DNDBUG -DCONF_debug_memory=0 -o ioengine_nvme ioengine_nvme.c


clean:
	if [ -d fio ]; then cd fio && $(MAKE) clean; fi

distclean:
	rm -rf fio
