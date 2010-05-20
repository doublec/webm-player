UNAME=$(shell uname -s)
ifeq "$(UNAME)" "Linux"
INCLUDE=
LIBS=-lasound
endif

ifeq "$(UNAME)" "Darwin"
#INCLUDE=-I/opt/local/include
LIBS=-lSDLmain -framework Carbon -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Cocoa
endif

INCLUDE +=-I. -Ivp8/include
LIBS += vp8-build/libon2_codecs.a

all: webm

webm.o: webm.cpp 
	g++ -g -c $(INCLUDE) -o webm.o webm.cpp

webm: webm.o halloc.o nestegg.o
	g++ -g -o webm webm.o halloc.o nestegg.o -lsydneyaudio -lvorbis -logg -lSDL $(LIBS)

clean: 
	rm *.o webm

halloc.o: libnestegg/src/halloc.c
	gcc -g -c $(INCLUDE) -o halloc.o libnestegg/src/halloc.c

nestegg.o: libnestegg/src/nestegg.c
	gcc -g -c $(INCLUDE) -o nestegg.o libnestegg/src/nestegg.c

