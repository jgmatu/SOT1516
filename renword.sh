#!/bin/bash

if [ $# -ne 1 ]
then
	echo 1>&2 "You have pass bad the arguments"
	exit 1
fi

ls=`ls`
for i in $ls; do
	if grep -s '^'$1 $i
	then
		mv $i $i'.'$1;
	fi
done
exit 0
