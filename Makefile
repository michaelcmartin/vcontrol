CFLAGS=`sdl2-config --cflags` -c -Iinclude -O2
LDOPTS=`sdl2-config --libs` -Llib -lvcontrol
LIBS=lib/libvcontrol.a

COBJS=src/vcontrol.o src/keynames.o \
	src/demo/basic_demo.o \
	src/demo/multi_demo.o \
	src/demo/lock_demo.o

all: bin/basic_demo bin/c++_demo bin/multi_demo bin/lock_demo

clean:
	rm -f ${COBJS} src/demo/c++_demo.o bin/basic_demo bin/c++_demo bin/multi_demo bin/lock_demo bin/test.cfg lib/libvcontrol.a

bin/basic_demo: src/demo/basic_demo.o ${LIBS} bin/test.cfg
	mkdir -p bin && gcc -o bin/basic_demo src/demo/basic_demo.o ${LDOPTS}

bin/c++_demo: src/demo/c++_demo.o ${LIBS} bin/test.cfg
	mkdir -p bin && g++ -o bin/c++_demo src/demo/c++_demo.o ${LDOPTS}

bin/multi_demo: src/demo/multi_demo.o ${LIBS} bin/test.cfg
	mkdir -p bin && gcc -o bin/multi_demo src/demo/multi_demo.o ${LDOPTS}

bin/lock_demo: src/demo/lock_demo.o ${LIBS} bin/test.cfg
	mkdir -p bin && gcc -o bin/lock_demo src/demo/lock_demo.o ${LDOPTS}

bin/test.cfg: src/demo/test.cfg
	mkdir -p bin && cp src/demo/test.cfg bin/test.cfg

lib/libvcontrol.a: src/vcontrol.o src/keynames.o
	mkdir -p lib && ar r lib/libvcontrol.a src/vcontrol.o src/keynames.o

$(COBJS): %.o: %.c
	gcc ${CFLAGS} -o $@ $<

$(COBJS): include/vcontrol.h

src/demo/c++_demo.o: src/demo/c++_demo.cpp include/vcontrol.h
	g++ ${CFLAGS} -o $@ $<
