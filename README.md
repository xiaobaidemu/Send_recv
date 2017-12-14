# Send_recv
##How to Use
##1.修改test.cpp中的masterip为启动的master进程的ip,maxpid为worker进程数，make编译
##2.修改hostfile文件，每一行对应一个worker进程启动时所在的节点名；修改程序运行时所在的共享目录src_path
##3.首先在master节点运行./test master, 之后运行./runworker.sh
