#!/bin/bash
#src_path="/home/cuixiang/hbx/graduate_2/net"
src_path="/home/heboxin/Desktop/net";
i=1;
while read hostip; do
	echo "start worker on ${hostip}"
	echo "cd ${src_path} && ./test worker"
	ssh -n $hostip "cd ${src_path} && ./test worker" > out/$i.out 2>&1 &
	((i++))
done < hostfile
tail -f out/*
