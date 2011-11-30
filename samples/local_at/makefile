all: main

run: main
	LD_LIBRARY_PATH=../../lib ./$^

main: main.c ../../lib/libxbee.so
	gcc -m32 $(filter %.c,$^) -I../../ -L../../lib -lxbee -o $@
