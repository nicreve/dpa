# Makefile for DPA toolkit
# Author: <nicrevelee@gmail.com>
# Last Update:2014-04-30

all:
	(cd src/receiver; ${MAKE};cp -f receiver ../../;)
	(cd src/dpastats; ${MAKE};cp -f dpastats ../../;)
	
clean:
	(cd src/receiver; ${MAKE} clean;)
	(cd src/dpastats; ${MAKE} clean;)
	-rm -f receiver dpastats