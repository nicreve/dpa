# Makefile for DPA toolkit
# Author: <nicrevelee@gmail.com>
# Last Update:2014-07-10

SRCS:=$(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp)
OBJECT:=$(patsubst %.cpp,%.o,$(patsubst %.c, %.o,$(patsubst %.cc, %.o, $(SRCS))))
EXECUTABLE:=dpastats
GCC=gcc
CC=g++
LIB=-lrt -lpcap -lpthread -ldpa
USR_OPTION=-g

.c.o:
	$(CC) $(USR_OPTION) -c $< $(INCLUDES)

all:$(OBJECT)
	$(CC) -o $(EXECUTABLE) -g $(OBJECT) $(LIBPATH) $(LIB)

clean:
	-rm -f $(OBJECT) $(EXECUTABLE)
