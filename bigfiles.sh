#!/bin/bash
 
	if [ $# -gt 1 ]
	then
		echo 1>&2 "Bad pass of arguments only one argument" 
		exit 1
	fi


	if [ x$1 ==  x ]   
	then
		for file in `du -ab | sort -n -r | awk '{print $2}'`; do
			if [ -f $file ] 
			then		
				echo `du -b $file`
			fi
		done | sed 1q | awk '{print $2 "\t" $1}'
		exit 0
	fi

	if echo $1 | grep -v '^[0-9]\+$'; then  
		
		echo 1>&2 "You must pass a number like argument"
		exit 1	
		
	fi

	for file in `du -a | awk '{print $2}'`; do
		if [ -f $file ] 
		then
			echo `du -b $file`
		fi
	done | sort -n -r | sed $1q | awk '{print $2 "\t" $1}'
	exit 0
