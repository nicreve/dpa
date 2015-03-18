#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-11-29

source confparse
source utils
source affinity

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
	echo "Usage: duplicator.sh build | unbuild | start | stop"
}


function cleanup
{
	echo -n "Cleaning up directory:"
	make clean >/dev/null
	echoOK
}

function build
{
	verbose=0
	echo -n "Building duplicator:"
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
	#if [ $verbose = 1 ];then
	#	awk 'BEGIN{print "Build information:"}{print "\t"$0;}'<<< "$info"
	#fi
}

function fileusagecheck
{
	unset $pid_array
	for intf in ${DUP_INTERFACES[@]}
	do
		for index in `seq 0 $[ COPIES - 1 ]`
		do
			unset filepid_array
			if [ ! -e /dev/shm/dpa_${intf}_${index} ] ; then
				continue
			fi
			filepid_array=(`lsof -Fp /dev/shm/dpa_${intf}_${index} | awk '/^p[0-9]*$/{gsub(/^p/,"");print}'`)
			if [ ${#filepid_array[*]} -eq 0 ]; then
				continue
			fi
			echo "Duplicator file ""dpa_${intf}_${index}"" is opened by:"
			for pid in  ${filepid_array[@]}
			do
				if [ -n $pid ]; then
					cmd=`ps -p $pid -o comm=`
					echo "${cmd} (PID: ${pid})"
					pid_array=("${pid_array[@]}" $pid)
				fi
			done
		done
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

function start
{
	if [ ! -f "duplicator" ]; then 
		echo "Error: Duplicator does not exist."
		exit 1
	fi
	
	if [ $ON_ALL_INTERFACES == 1 ]; then
		DUP_INTERFACES=("${INTERFACES[@]}")
	fi
	
	DUP_INTERFACES=($(awk -vRS=' ' '!a[$1]++' <<< ${DUP_INTERFACES[@]}))
	
	intfnum=${#DUP_INTERFACES[@]}
	if [ $intfnum == 0 ]; then
		echo "Error: No interface is configured, abort."
		exit 1;
	fi

	if [ $SET_AFFINITY = 1 ]; then
	
		getAffinityCPUs
		totalvec=0
	
		for intf in ${DUP_INTERFACES[@]}
		do
			for rxdir in rx TxRx
			do
				maxvec=`grep $intf-$rxdir /proc/interrupts | wc -l`
				if [ "$maxvec" == "0" ] ; then
					maxvec=`egrep -i "$intf:.*$rxdir" /proc/interrupts | wc -l`
				fi
				if [ "$maxvec" == "0" ] ; then
					continue
				fi
				maxvec_array=("${maxvec_array[@]}" $maxvec)
				totalvec=$[totalvec + maxvec]
				break
			done
		done
		affinityfirst=${affinity_cpus_array[0]};
		affinitylast=${affinity_cpus_array[@]: -1}
		rangenum=${#affinity_cpus_array[@]}
	
		if [ $[ affinitylast - affinityfirst ] -ne $[rangenum - 1] ]; then
			echo "Error: Invalid affinity range $AFFINITY_RANGE, it should be continuous."
		fi
	
		if [ $totalvec -gt ${#affinity_cpus_array[@]} ]; then
			echo "Error: Invalid affinity range $AFFINITY_RANGE, $totalvec CPUs needed."
			exit 1
		fi
	fi
	
	stop 1
	
	fileusagecheck
	
	datestr=`date +%F_%H-%M-%S`
	
	affinitybegin=${affinity_cpus_array[0]}
	
	for i in ${!DUP_INTERFACES[@]}
	do
		intf=${DUP_INTERFACES[$i]}
		echo -n "Starting duplicator for $intf:"
		affinityend=$[affinitybegin + ${maxvec_array[$i]} - 1]
		if [ $SET_AFFINITY = 1 ] ; then
			affinitystr="-a ${affinitybegin}:${affinityend}"
		else
			affinitystr=""
		fi
		
		./duplicator -c $COPIES $affinitystr -p $DUP_REPORT_PERIOD $intf > ${intf}_${datestr}.log &
		proctermed=`ps ax | grep -v grep | grep -q duplicator.*${intf}'$'; echo $?`
		if [ ! "$proctermed" == "0" ] ; then
			echoFail
			exit 1
		fi
		echoOK
		affinitybegin=$[affinityend + 1]
	done
}

function stop
{
	dupproc=`ps ax | grep -v grep | grep -q "duplicator "; echo $?`

	if [ "$dupproc" != "0" ] ; then
		return 0
	fi

	if [ $1 -eq 1 ] ; then
		echo "Duplicator is/are already running."
		echo -n "Stop the running duplicator process(es)? [Y/N]:"
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
				echo -n -e "\nStop the running duplicator process(es)? [Y/N]:"
				;;
		esac
		done
	fi
	
	echo -n "Stopping duplicator(s):"
	errorinfo=`killall duplicator 2>&1 >/dev/null`
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
	build)
		initConf
		cd ../duplicator
		if [ $? != 0 ]; then
			exit 1
		fi
		cleanup
		build
		exit 0
        ;;
	unbuild)
		initConf
		cd ../duplicator
		if [ $? != 0 ]; then
			exit 1
		fi
		cleanup
		exit 0
		;;
	start)
		initConf
		cd ../duplicator
		if [ $? != 0 ]; then
			exit 1
		fi
		start
		if [ $SET_AFFINITY = 1 ] ; then
			cd ../scripts
			./irqaffinity.sh
			if [ $? != 0 ]; then
				exit 1
			fi
		fi
		exit 0
		;;
	stop)
		initConf
		stop 0
		if [ $? = 0 ];then
			echo "No duplicator is running."
		fi
		exit 0
		;;
    *)
        usage
		exit 1
        ;;
esac
