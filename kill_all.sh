#!/bin/bash
#src_path="/home/cuixiang/hbx/graduate_2/net"
src_path="/home/heboxin/Desktop/net"
while read hostip;do
	echo "kill process on ${hostip}"
	ssh -n $hostip "cd ${src_path} && ./kill.sh"
	process_num=$(ssh -n $hostip  " ps -Af | grep 'test ' | grep -v grep | grep -v qemu | wc -l")

	echo "rest procress num $process_num"
done < hostfile
