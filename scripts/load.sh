#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-05-07

source confparse
source utils

declare -a filepid_array
declare -a pid_array

function abort
{
	if [ -n "$1" ];then 
		awk 'BEGIN{print "Error information:"}{print "\t"$0}END{print "Abort."}'<<< "$1"
	else
		echo "Error occurred, abort."
	fi
	exit 1
}

function usage
{
	echo "Usage: load.sh load | unload"
}


function intfCheck
{
	intfnum=${#INTERFACES[@]}
	if [ $intfnum == 0 ]; then
		echo "Error: No interface is configured, abort."
		exit 1;
	fi
	if [ ${#relevant_intf_array[@]} == 0 ]; then
		return 0
	fi
	
	echo "Warning! This script will unload or reload some NIC driver(s), the following interface(s) will be affected:" 
	for intf in ${relevant_intf_array[@]}
	do
		addr=$(ifconfig $intf | awk -F ":"  '/inet addr/{split($2,a," ");print a[1]}')
		if [ -z "$addr" ]; then
			echo -e "\033[1m$intf\033[0m"
		else
			echo -e "\033[1;31m$intf (IPv4 address: $addr)\033[0;39m"
		fi
	done
	echo -n "Continue? [Y/N]:"
	while read -r -n 1 response
	do
	case $response in 
		[yY])
			echo ""
			break
			;;
		[nN])
			echo ""
			exit 0
			;;
		*)
			echo -n -e "\nContinue? [Y/N]:"
			;;
	esac
	done
}

function removeDriver
{
	for drv in ${driver_array[@]}
	do	
		if lsmod|grep -o "^${drv} " >/dev/null; then
			echo -n "Removing $drv driver:"
			errorinfo=$(rmmod $drv 2>&1 > /dev/null)
			if [ $? != 0 ]; then
				echoFail
				abort "$errorinfo"
			fi
			echoOK
		fi
	done
	for drv in ${relevant_driver_array[@]}
	do	
		if lsmod|grep -o "^${drv} " >/dev/null; then
			echo -n "Removing $drv driver:"
			errorinfo=$(rmmod $drv 2>&1 > /dev/null)
			if [ $? != 0 ]; then
				echoFail
				abort "$errorinfo"
			fi
			echoOK
		fi
	done
}
function removeCore
{
	if lsmod|grep -o "^dpa " >/dev/null; then
		echo -n "Removing DPA core module:"
		errorinfo=$(rmmod dpa 2>&1 > /dev/null)
		if [ $? != 0 ]; then
			echoFail
			abort "$errorinfo"
		fi
		echoOK
	fi
}
function addCore
{
	dpaparam_ifnum="num_interfaces=$intfnum"
	if [ $RSS_QUEUES != 0 ]; then
		dpaparam_queues="num_queues=$RSS_QUEUES"
	fi
	if [ $AUTOSWITCH_PROMISC == 0 ]; then
		dpaparam_promisc="alter_promisc=0"
	fi
	if [ $VLAN_STRIP == 0 ]; then
		dpaparam_vlan="vlan_strip=0"
	fi
	echo -n "Adding DPA core module:"
	errorinfo=$(modprobe dpa $dpaparam_ifnum $dpaparam_queues $dpaparam_promisc $dpaparam_vlan 2>&1 > /dev/null)
	if [ $? != 0 ]; then
		echoFail
		abort "$errorinfo"
	fi
	echoOK
}
function addDriver
{
	if [ $RSS_QUEUES != 0 ]; then
		dpaparam_rss="rss_queues=$RSS_QUEUES"
	fi
	
	for drv in ${driver_array[@]}
	do
		echo -n "Adding $drv driver:"
		errorinfo=$(modprobe $drv $dpaparam_rss 2>&1 > /dev/null)
		if [ $? != 0 ]; then
			echoFail
			abort "$errorinfo"
		fi
		echoOK
	done
}

function usageCheck
{
	unset pid_array

	unset filepid_array
	if [ ! -e /dev/dpa ] ; then
		return 0
	fi
	filepid_array=(`lsof -Fp /dev/dpa | awk '/^p[0-9]*$/{gsub(/^p/,"");print}'`)
	if [ ${#filepid_array[*]} -eq 0 ]; then
		return 0
	fi
	echo "DPA is used by:"
	for pid in  ${filepid_array[@]}
	do
		if [ -n $pid ]; then
			cmd=`ps -p $pid -o comm=`
			echo "${cmd} (PID: ${pid})"
			pid_array=("${pid_array[@]}" $pid)
		fi
	done

	if [ ${#pid_array[*]} -eq 0 ] ; then
		return 0
	fi
	
	echo -n "Stop process(es) list above and continue? [Y/N]:"
	while read -r -n 1 response
	do
	case $response in 
		[yY])
			echo ""
			break
			;;
		[nN])
			echo ""
			exit 0
			;;
		*)
			echo -n -e "\nStop process(es) list above and continue? [Y/N]:"
			;;
	esac
	done
	
	echo -n "Stopping process(es):"
	
	for pid in ${pid_array[@]}
	do
		errorinfo=`kill -9 $pid`
		if [ $? != 0 ]; then
			echoFail
			cmd=`ps -p $pid -o comm=`
			echo "Error: Failed to stop process $cmd (PID:${pid})."
			echo "ErrorInfo: errorinfo"
			exit 1
		fi
	done
	echoOK
}
function removeCheck
{
	unset relevant_driver_array
	dpausedby=$(lsmod |grep "^dpa " |awk '{print $4}')
	OLD_IFS="$IFS"
	IFS=","
	dpa_used_by_array=($dpausedby)
	IFS="$OLD_IFS"
	
	for drv in ${dpa_used_by_array[@]}
	do
		findcomp=0
		for compdrv in ${driver_array[@]}
		do
			if [ "$drv" == "$compdrv" ];then
			findcomp=1
			break
			fi
		done
		if [ $findcomp = 0 ];then
			echo "DPA is used by $drv driver, it needed to be removed too."
			echo -n "Remove $drv driver? [Y/N]:"
			while read -r -n 1 response
			do
			case $response in 
			[yY])
				relevant_driver_array=("${relevant_driver_array[@]}" $drv)
				echo ""
				break
				;;
			[nN])
				echo ""
				exit 0
				;;
			*)
				echo -n -e "\nRemove $drv driver? [Y/N]:"
				;;
			esac
			done
		fi
	done
	getRelevantIntf
}

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

userCheck
kernelCheck

case $1 in 
	load)
		initConf
		usageCheck
		removeCheck
		intfCheck
		removeDriver
		removeCore
		addCore
		addDriver
		exit 0
        ;;
	unload)
		initConf
		usageCheck
		removeCheck
		intfCheck
		removeDriver
		removeCore
		exit 0
		;;
    *)
        usage
		exit 1
        ;;
esac
