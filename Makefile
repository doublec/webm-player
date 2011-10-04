UNAME=$(shell uname -s)
ifeq "$(UNAME)" "Linux"
INCLUDE=
LIBS=-lasound
endif

ifeq "$(UNAME)" "Darwin"
#INCLUDE=-I/opt/local/include
LIBS=-lSDLmain -framework Carbon -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Cocoa
endif

INCLUDE +=-I. -Inestegg/include -Inestegg/halloc -Ilibvpx/vpx -Ilibvpx/vpx_codec -Ilibvpx/vpx_ports
LIBS += vpx-build/libvpx.a

all: webm

vpx-build/vpx_config.h: libvpx/configure
	mkdir -p vpx-build && cd vpx-build && ../libvpx/configure

vpx-build/libvpx.a: vpx-build/vpx_config.h
	cd vpx-build && make

nestegg/configure: nestegg/configure.ac
	cd nestegg && autoreconf --install && ./configure && cd ..

nestegg/src/nestegg.o: nestegg/configure nestegg/src/nestegg.c
	make -C nestegg

webm.o: webm.cpp vpx-build/libvpx.a nestegg/src/nestegg.o
	g++ -g -c $(INCLUDE) -o webm.o webm.cpp

webm: webm.o nestegg/halloc/src/halloc.o nestegg/src/nestegg.o vpx-build/libvpx.a
	g++ -g -o webm webm.o nestegg/halloc/src/halloc.o nestegg/src/nestegg.o -lvorbis -logg -lSDL $(LIBS)

clean: 
	rm *.o webm && rm -r vpx-build && make -C nestegg clean

nestegg/halloc/src/halloc.o: nestegg/halloc/src/halloc.c
	gcc -g -c $(INCLUDE) -o halloc.o nestegg/halloc/src/halloc.c

nestegg.o: nestegg/src/nestegg.c
	gcc -g -c $(INCLUDE) -o nestegg.o nestegg/src/nestegg.c

