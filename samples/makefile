all: main

run: main
	./$^

main: main.c /usr/lib/libxbee.so /usr/include/xbee.h
	gcc $(filter %.c,$^) -lxbee -o $@
