
HSBROOT=$(shell pwd)


#CFLAGS=-s  -Wall -Wstrict-prototypes -fomit-frame-pointer -Wunused
#CFLAGS=-g -Wstrict-prototypes 
CFLAGS=-O2 -Wstrict-prototypes -s 

# add default debug level
CFLAGS+=-DDEBUG_DEFAULT_LEVEL=3#-DSLIENT_REBOOT

HSB_LIBS_SRC = $(wildcard $(HSBROOT)/lib/*.c)
HSB_LIBS = $(patsubst %.c,%.obj,$(HSB_LIBS_SRC) ) 
HSB_INCLUDES=-I$(HSBROOT)/include

MISC_LIBS=-lrt -lm -lasound

ARCH=mips

EXEDIR=${HSBROOT}/install

GLIB_LIBS=`pkg-config --libs glib-2.0 gthread-2.0` 
GLIB_INCLUDES=`pkg-config --cflags glib-2.0 gthread-2.0`
CFLAGS+=$(GLIB_INCLUDES) $(HSB_INCLUDES)
LDFLAGS=$(HSB_LIBS) $(MISC_LIBS) $(GLIB_LIBS)  

export ARCH CC CFLAGS HSB_INCLUDES HSB_LIBS  LDFLAGS CFLAGS  PKG_CONFIG_PATH EXEDIR

.PHONY:  dep lib misc

SUBDIR=lib core_daemon awaken_daemon asr_daemon misc
all :
	for i in ${SUBDIR}; do make all -C "$${i}" ; done

lib :
	make all -C lib

exp :
	make all -C experiment

core : lib
	make all -C core_daemon

awaken : lib
	make all -C awaken_daemon

asr : lib
	make all -C asr_daemon

misc : lib
	make all -C misc

dep:
	for i in ${SUBDIR}; do make dep -C "$${i}" ; done

clean:
	for i in ${SUBDIR}; do make clean -C "$${i}" ; done 
	@rm install/* -fr

