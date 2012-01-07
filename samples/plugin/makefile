all: main plugin.so

run: all
	./main

main: main.c /usr/lib/libxbee.so
	gcc $(filter %.c,$^) -g -lxbee -lpthread -lrt -ldl -o $@

plugin.so: plugin.c
	gcc -shared -Wl,-soname,myplugin.so -g -fPIC $^ -o $@
