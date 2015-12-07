#!/bin/sh

filename=`hostname`

ipcs|grep zf2|awk '{print $1}'|tee $filename

while read line 
do 
  ipcrm -M $line 
done <$filename


rm $filename
