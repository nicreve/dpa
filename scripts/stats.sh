#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-11-14

source confparse
source utils


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
	echo "Usage: stats.sh start | stop"
}


function start
{
	if [ ! -f "dpastats" ]; then 
		echo "Error: dpastats tool does not exist."
		exit 1
	fi
	
	stop 1
	
	for i in ${!INTERFACES[@]}
	do
		intf=${INTERFACES[$i]}
		echo -n "Starting dpastats process for $intf:"
		./dpastats -p $STATS_LOG_PERIOD -w ${logdirabspath}/${STATS_LOG_NAME} $intf >/dev/null &
		proctermed=`ps ax | grep -v grep | grep -q dpastats.*${intf}'$'; echo $?`
		if [ ! "$proctermed" == "0" ] ; then
			echoFail
			exit 1
		fi
		echoOK
	done
}

function stop
{
	statsproc=`ps ax | grep -v grep | grep -q "dpastats "; echo $?`

	if [ "$statsproc" != "0" ] ; then
		return 0
	fi

	if [ $1 -eq 1 ] ; then
		echo "Dpastats is/are already running."
		echo -n "Stop the running dpastats process(es)? [Y/N]:"

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
				echo -n -e "\nStop the running dpastats process(es)? [Y/N]:"
				;;
		esac
		done
	fi

	echo -n "Stopping dpastats process(es):"

	errorinfo=`killall dpastats 2>&1 >/dev/null`
	if [ $? != 0 ]; then
		echoFail
		echo "Error: ${errorinfo}."
		exit 1
	fi
	sleep 1
	echoOK
	return 1
}

if [ $# -lt 1 ]; then
    usage
    exit 1
fi


case $1 in 
	start)
		initConf
		logdirabspath=`readlink -m $STATS_LOG_DIR`
		cd ../tools
		if [ $? != 0 ]; then
			exit 1
		fi
		start
		exit 0
		;;
	stop)
		initConf
		stop 0
		if [ $? = 0 ];then
			echo "No dpastats tool is running."
		fi
		exit 0
		;;
    *)
        usage
		exit 1
        ;;
esac
