#!/bin/bash

if [ $# -ne 4 ] 
then
	echo 1>&2 "Bad pass of arguments"
	exit 1
fi


users=`sort $1 $2 $3 $4 | awk '{print $1}' | uniq`

for user in $users; do

	if `cat notas1 notas2 notas3 notas4 |  grep -E '*\b'$user'\b.*si$' | uniq -d` 2> /dev/null;  then
		
		echo $user " no"

	else

		echo $user " si"	

	fi 
done
exit 0
