all: main plugin.so

run: all
	LD_LIBRARY_PATH=../../lib ./main

main: main.c ../../lib/libxbee.so
	gcc $(filter %.c,$^) -I../../ -L../../lib -lxbee -o $@

plugin.so: plugin.c
	gcc -shared -Wl,-soname,myplugin.so -fPIC -I../../ $^ -o $@
