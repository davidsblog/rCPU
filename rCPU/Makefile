CC=gcc

rcpumake: rcpu.c dwebsvr.c
	$(CC) -o rcpu rcpu.c dwebsvr.c -pthread -std=gnu99

res:
	xxd -i smoothie.js smoothie.h
	xxd -i index.html index.h
	xxd -i code.js code.h
	xxd -i jquery-2-1-0-min.js jquery-2-1-0-min.h

vocore: rcpu.c dwebsvr.c
	export STAGING_DIR=$$PWD; \
`find /home -name mipsel-openwrt-linux-gcc -print -quit -print 2>/dev/null` -o rcpu -Os -s rcpu.c dwebsvr.c -pthread

wr740n: rcpu.c dwebsvr.c
	export STAGING_DIR=$$PWD; \
`find /home -name mips-openwrt-linux-uclibc-gcc -print -quit -print 2>/dev/null` -o rcpu -Os -s rcpu.c dwebsvr.c -pthread

.PHONY: clean

clean:
	rm -f *.o *.exe rcpu
