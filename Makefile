BUILDROOT := /home/user/prj/cloud_demo/buildroot/buildroot-at91/output/host/bin/arm-buildroot-linux-gnueabihf-
CC        := $(BUILDROOT)cc
AR        := $(BUILDROOT)ar
RANLIB    := $(BUILDROOT)ranlib
CFLAGS    := -g -Wall
LDFLAGS   := -L./ -lcgic -lcjson -lmosquitto

all: gateway.cgi install

install: libcgic.a
	cp gateway.cgi ~/srv/nfs
	#cp gateway.cgi /var/www/html/cgi-bin

libcgic.a: cgic.o cgic.h
	rm -f libcgic.a
	$(AR) rc libcgic.a cgic.o
	$(RANLIB) libcgic.a

gateway.cgi: gateway.o mosq.o libcgic.a
	$(CC) gateway.o mosq.o -o gateway.cgi $(LDFLAGS)

clean:
	rm -f *.o *.a gateway.cgi
