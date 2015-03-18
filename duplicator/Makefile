# Makefile for DPA duplicator
# Author: <nicrevelee@gmail.com>
# Last Update:2014-04-30

SRCS:=$(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp)
OBJECT:=$(patsubst %.cpp,%.o,$(patsubst %.c, %.o,$(patsubst %.cc, %.o, $(SRCS))))
EXECUTABLE:=duplicator
GCC=gcc
CC=g++
LIB=-lrt
USR_OPTION=-g -W

.c.o:
ifeq (,$(wildcard dpa_c.h))
	ln -sf ../kernel/dpa_c.h dpa_c.h 
endif
	$(CC) $(USR_OPTION) -c $< $(INCLUDES)

all:$(OBJECT)
	$(CC) -o $(EXECUTABLE) $(OBJECT) $(LIBPATH) $(LIB)

clean:
	-rm -f $(OBJECT) $(EXECUTABLE)
