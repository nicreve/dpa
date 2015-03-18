# Makefile for DPA lib
# Author: <nicrevelee@gmail.com>
# Last Update:2014-04-30

PREFIX = /usr/local
INCLUDEDIR = ${PREFIX}/include
LIBDIR =  ${PREFIX}/lib

SRCS = $(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp)
OBJ = $(patsubst %.cpp,%.o,$(patsubst %.c, %.o,$(patsubst %.cc, %.o, $(SRCS))))
PUBHDR = dpa.h

GCC = gcc
CC = g++
AR = ar
RANLIB = ranlib
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

CCOPT = -fpic
INCLS = -I.
DEFS =
CFLAGS = -g -W
FULL_CFLAGS = $(CCOPT) $(INCLS) $(DEFS) $(CFLAGS)
LDFLAGS = 

LIBS = 
ADDLOBJS =
ADDLARCHIVEOBJS =   


.c.o:
ifeq (,$(wildcard dpa_c.h))
	ln -sf ../kernel/dpa_c.h dpa_c.h 
endif
	$(GCC) $(FULL_CFLAGS) -c $*.c

	
all: archive shared


archive: libdpa.a

libdpa.a: $(OBJ)
	$(AR) rc $@ $(OBJ) $(ADDLARCHIVEOBJS)
	$(RANLIB) $@


shared: libdpa.so

libdpa.so: $(OBJ)
	VER=`cat VERSION`; \
	MAJOR_VER=`sed 's/\([0-9][0-9]*\)\..*/\1/' VERSION`; \
	$(GCC) -fPIC -shared -Wl,-soname,$@.$$MAJOR_VER $(LDFLAGS) \
	    -o $@.$$VER $(OBJ) $(ADDLOBJS) $(LIBS)

		

install: install-archive install-shared
	$(INSTALL_DATA) $(PUBHDR) $(INCLUDEDIR)/$(PUBHDR);

	
install-archive: libdpa.a
	[ -d $(LIBDIR) ] || \
	    (mkdir -p $(LIBDIR); chmod 755 $(LIBDIR))
	$(INSTALL_DATA) libdpa.a $(LIBDIR)/libdpa.a
	$(RANLIB) $(LIBDIR)/libdpa.a

install-shared: libdpa.so
	[ -d $(LIBDIR) ] || \
	    (mkdir -p $(LIBDIR); chmod 755 $(LIBDIR))
	VER=`cat VERSION`; \
	MAJOR_VER=`sed 's/\([0-9][0-9]*\)\..*/\1/' VERSION`; \
	$(INSTALL_PROGRAM) libdpa.so.$$VER $(LIBDIR)/libdpa.so.$$VER; \
	ln -sf libdpa.so.$$VER $(LIBDIR)/libdpa.so.$$MAJOR_VER; \
	ln -sf libdpa.so.$$MAJOR_VER $(LIBDIR)/libdpa.so


uninstall: uninstall-archive uninstall-shared
	rm -f $(INCLUDEDIR)/$(PUBHDR)

uninstall-archive:
	rm -f $(LIBDIR)/libdpa.a
	
uninstall-shared:
	VER=`cat VERSION`; \
		MAJOR_VER=`sed 's/\([0-9][0-9]*\)\..*/\1/' VERSION`; \
		rm -f $(LIBDIR)/libdpa.so.$$VER; \
		rm -f $(LIBDIR)/libdpa.so.$$MAJOR_VER; \
		rm -f $(LIBDIR)/libdpa.so


clean:
	-rm -f $(OBJ) libdpa.so.* libdpa.a
