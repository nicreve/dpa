#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-04-21

source confparse
source utils

function usage
{
	echo "Usage: build.sh build | unbuild [verbose]"

}

function cleanup
{
	echo -n "Cleaning up $1 directory:"
	make clean >/dev/null
	echoOK
}

function build
{
	echo -n "Building $1:"
	if [ $verbose = 1 ];then
		info=$(make 2>&1)
	else
		info=$(make 2>&1 >/dev/null)
	fi
	
	if [ $? != 0 ]; then
		echoFail
		awk 'BEGIN{print "Error information:"}{print "\t"$0}END{print "Abort."}'<<< "$info"
		exit 1
	fi
	
	echoOK
	if [ $verbose = 1 ]; then
		awk 'BEGIN{print "Build information:"}{print "\t"$0;}'<<< "$info"
	fi
}

function install
{
	echo -n "Installing $1:"
	
	errorinfo=$(make install 2>&1 >/dev/null)
	
	if [ $? != 0 ]; then
		echoFail
		echo "$errorinfo"
		exit 1
	fi
	echoOK
}

function uninstall
{
	echo -n "Uninstalling $1:"
	
	errorinfo=$(make uninstall 2>&1 >/dev/null)
	
	if [ $? != 0 ]; then
		echoFail
		echo "$errorinfo"
		exit 1
	fi
	echoOK
}

function fullBuild
{
	cleanup "$1"
	build "$1"
	install "$1"
}

function fullUnbuild
{
	cleanup "$1"
	uninstall "$1"
}
#编译主模块
function buildCore
{
	cd kernel
	if [ $? != 0 ]; then
		exit 1
	fi
	
	if [ $1 == 1 ];then
		fullBuild "DPA core module"
	else
		fullUnbuild "DPA core module"
	fi
	
	cd ../
	if [ $? != 0 ]; then
		exit 1
	fi
	return 0
}

function buildDriver
{
	cd drivers
	if [ $? != 0 ]; then
		exit 1
	fi
	
	for map in ${inuse_driver_map_array[@]}
	do
		drv=$(awk -F":" '{print $1}' <<< $map)
		dir=$(awk -F":" '{print $2}' <<< $map)
		if [ -n "$dir" ] && [ -n "$drv" ]; then
			cd $dir
			if [ $? != 0 ]; then
				continue
			fi
			if [ $1 == 1 ]; then
				fullBuild "DPA-aware $drv driver"
			else
				fullUnbuild "DPA-aware $drv driver"
			fi
			cd ../
		fi
	done
	cd ../
	if [ $? != 0 ]; then
		exit 1
	fi
	return 0
}

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

if [ -n $2 ] && [ "$2" == "verbose" ]; then
	verbose=1
else
	verbose=0
fi

userCheck
kernelCheck

case $1 in 
	build)
		initConf
		cd ../
		buildCore 1
		buildDriver 1
		exit 0
        ;;
	unbuild)
		initConf
		cd ../
		buildCore 0
		buildDriver 0
		exit 0
		;;
    *)
        usage
		exit 1
        ;;
esac



