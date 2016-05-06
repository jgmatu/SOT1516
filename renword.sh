#!/bin/bash

if [ $# -eq 1 ]
then
	ls=`ls`
	for i in $ls; do
		if grep '^'$1 $i > /dev/null 2> /dev/null
		then
			mv $i $i'.'$1;
		fi
	done
	exit 0
else
	echo 1>&2 "You have pass bad the arguments" 
	exit 1
fi

