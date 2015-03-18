#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-05-07

source confparse
source utils

initConf

echo -n "Checking if DPA core module is added:"
if lsmod|grep -o "^dpa " >/dev/null; then
	echoOK
else
	echoFail
	#err=1
fi 

echo -n "Checking if DPA character device exists:"
if [ -c /dev/dpa ]; then
	echoOK
else
	echoFail
	#err=1
fi
for drv in ${driver_array[@]}
do	
	DRIVER_OK=0
	echo -n "Checking if DPA-aware $drv driver is added:"
	if (lsmod|grep -o "^${drv} " >/dev/null);  then
		DRIVER_VER=`modinfo -F version $drv`
		if [ "${DRIVER_VER:(-4):4}" = "-DPA" ]; then
			DRIVER_OK=1;
		fi
	fi

	if [ "$DRIVER_OK" -eq 1 ]; then
		echoOK
	else
		echoFail
		#err=1
	fi 
done

#if [ "$err" -eq 1 ]; then
#	printf "\033[31mCheck failed, please reinstall DPA.\033[0m\n"
#else
#	printf "\033[32mDPA has been successfully loaded.\033[0m\n"
#fi
