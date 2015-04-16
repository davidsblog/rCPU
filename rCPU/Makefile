CC=gcc
CFLAGS=-pthread -std=gnu99
CFILES=rcpu.c dwebsvr.c
RESOURCES=index.h smoothie.h code.h jquery-2-1-0-min.h
TARGET=rcpu

$(TARGET): $(CFILES) $(RESOURCES)
	$(CC) $(CFILES) $(CFLAGS) -o $@

index.h: index.html
	xxd -i index.html index.h

smoothie.h: smoothie.js
	xxd -i smoothie.js smoothie.h

code.h: code.js
	xxd -i code.js code.h

jquery-2-1-0-min.h: jquery-2-1-0-min.js
	xxd -i jquery-2-1-0-min.js jquery-2-1-0-min.h

vocore: $(CFILES) $(RESOURCES)
	export STAGING_DIR=$$PWD; \
    `find /home -name mipsel-openwrt-linux-gcc -print -quit -print 2>/dev/null` -o $(TARGET) \
    -Os -s $(CFILES) $(CFLAGS)

wr740n: $(CFILES) $(RESOURCES)
	export STAGING_DIR=$$PWD; \
    `find /home -name mips-openwrt-linux-uclibc-gcc -print -quit -print 2>/dev/null` -o $(TARGET) \
    -Os -s $(CFILES) $(CFLAGS)

.PHONY: clean

install: $(TARGET)
	cp -n rcpu /usr/local/bin
	cp -n init_script /etc/init.d/rCPU
	chmod +x /etc/init.d/rCPU
	/etc/init.d/rCPU start

clean:
	rm -f *.o $(TARGET) $(RESOURCES)
