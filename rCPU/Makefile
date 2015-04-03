CC=gcc

rcpumake: rcpu.c dwebsvr.c
	$(CC) -o rcpu rcpu.c dwebsvr.c -pthread -std=gnu99

vocore: rcpu.c dwebsvr.c
	export STAGING_DIR=$$PWD; \
`find /home -name mipsel-openwrt-linux-gcc -print -quit -print 2>/dev/null` -o rcpu -Os -s rcpu.c dwebsvr.c -pthread

wr740n: rcpu.c dwebsvr.c
	export STAGING_DIR=$$PWD; \
`find /home -name mips-openwrt-linux-uclibc-gcc -print -quit -print 2>/dev/null` -o rcpu -Os -s rcpu.c dwebsvr.c -pthread

.PHONY: clean

clean:
	rm -f *.o *.exe rcpu
