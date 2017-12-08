#!/bin/bash

pids=$(ps -Af | grep test | grep -v grep | grep -v qemu | grep -v "test.cpp" |  awk '{print $2}')

for pid in $pids; do
    kill -9 $pid
done

pids=$(ps -Af | grep pex | grep Debug | grep -v grep | grep -v qemu | awk '{print $2}')

for pid in $pids; do
    kill -9 $pid
done

exit 0
