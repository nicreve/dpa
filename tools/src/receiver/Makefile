# Makefile for DPA toolkit
# Author: <nicrevelee@gmail.com>
# Last Update:2014-07-08

SRCS:=$(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp)
OBJECT:=$(patsubst %.cpp,%.o,$(patsubst %.c, %.o,$(patsubst %.cc, %.o, $(SRCS))))
EXECUTABLE:=receiver
GCC=gcc
CC=g++
LIB=-lrt -lpcap -lpthread -ldpa
USR_OPTION=-g -W

.c.o:
	$(CC) $(USR_OPTION) -c $< $(INCLUDES)

all:$(OBJECT)
	$(CC) -o $(EXECUTABLE) $(OBJECT) $(LIBPATH) $(LIB)

clean:
	-rm -f $(OBJECT) $(EXECUTABLE)
