#!/bin/bash
# Shell script for DPA
# Author: <nicrevelee@gmail.com>
# Last Update: 2014-05-09

source confparse
source utils
source affinity


userCheck
kernelCheck

initConf

getAffinityCPUs

for intf in ${INTERFACES[@]}
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
		for vec in `seq 0 1 $maxvec`
		do
			irq=`cat /proc/interrupts | grep -i $intf-$rxdir-$vec"$"  | cut  -d:  -f1 | sed "s/ //g"`
			if [ -n  "$irq" ]; then
				irq_name_array=("${irq_name_array[@]}" "queue $vec of $intf")
				irq_number_array=("${irq_number_array[@]}" $irq)
			else
				irq=`cat /proc/interrupts | egrep -i $intf:v$vec-$rxdir"$"  | cut  -d:  -f1 | sed "s/ //g"`
				if [ -n  "$irq" ]; then
					irq_name_array=("${irq_name_array[@]}" "$intf, queue $vec")
					irq_number_array=("${irq_number_array[@]}" $irq)
				fi
			fi
		done
	done
done

if [ ${#irq_number_array[@]} -gt  ${#affinity_cpus_array[@]} ] ; then
	echo "Error: Invalid affinity range $AFFINITY_RANGE, ${#irqnumber[@]} CPUs needed."
	exit 1
fi


irqbalance=`ps ax | grep -v grep | grep -q irqbalance; echo $?`
if [ "$irqbalance" == "0" ] ; then
	echo "Warning! irqbalance is running and will likely override this script's affinitization."
	echo -n "Stop irqbalance and continue? [Y/N]:"
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
			echo -n -e "\nKill irqbalance and continue? [Y/N]:"
			;;
	esac
	done
	echo -n "Stopping irqbalance:"
	killall irqbalance
	if [ $? != 0 ]; then
		echoFail
		exit 1
	fi
	echoOK
fi

for i in ${!irq_number_array[@]}
do
	irq=${irq_number_array[$i]}
	irqname=${irq_name_array[$i]}
	cpu=${affinity_cpus_array[$i]}
	mask=$((1<<$cpu))
	echo -n "Setting $irqname's IRQ affinity to CPU $cpu:"
	printf "%X" $mask > /proc/irq/$irq/smp_affinity
	echoOK
done

