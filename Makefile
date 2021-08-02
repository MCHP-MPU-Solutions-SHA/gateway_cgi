BUILDROOT := /home/wayne/src/a5d2/buildroot-at91/output/host/bin/arm-buildroot-linux-gnueabihf-
CC        := $(BUILDROOT)cc
AR        := $(BUILDROOT)ar
RANLIB    := $(BUILDROOT)ranlib
CFLAGS    := -g -Wall
LDFLAGS   := -L./ -lcgic -lcjson

all: gateway.cgi install

install: libcgic.a
	#cp gateway.cgi ~/src
	#cp gateway.cgi /var/www/html/cgi-bin

libcgic.a: cgic.o cgic.h
	rm -f libcgic.a
	$(AR) rc libcgic.a cgic.o
	$(RANLIB) libcgic.a

gateway.cgi: gateway.o libcgic.a
	$(CC) gateway.o -o gateway.cgi $(LDFLAGS)

clean:
	rm -f *.o *.a gateway.cgi
