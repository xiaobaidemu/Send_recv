/*************************************************************************
 > File Name: net.h
 > Author: He Boxin
 > Mail: heboxin@pku.edu.cn
 > Created Time: 2017年11月22日 星期三 15时14分35秒
 ************************************************************************/
#ifndef _NET_H
#define _NET_H
#include <iostream>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <pthread.h>
#define MAX_NUM_IP 1024

#define COMM_ANY_SOURCE  (-2) //Fiala
#define COMM_ANY_TAG     (-1) //Fiala
#define COMM_SUCCESS     0 

using std::vector;
using std::deque;
enum {
	NON_SYS	   = 0,	RECV_OK    = 1, HANDSHAKE  = 2, ISEND      = 3,
	SEND_REQ   = 4, BARRIER	   = 6, BARRIER_GO = 7, IRECV	   = 10,
	NA       = -999
};

typedef unsigned long len_t;
typedef int comm_request;
typedef struct comm_status{
	int count;
	int cancelled;
	int comm_src;
	int comm_tag;
	int comm_error;
}comm_status;

typedef struct m{
	int src;
	int dest;
	int tag;
	int sys;
	int size;
	void *msg;
}msg;
//m1代表队列中的msg指针，m2代表被查询的msg
//这个还需要修改
bool cmp(const msg* m1, const msg* m2);
typedef deque<msg*>::iterator IT;
class Network{
public:
    struct phoneinfo{
        char  wip[16];
        int   wpid;
		int   wport;
    };
	typedef struct sockdata_{
		int rank;//the rank in all process
		char* buffer_send;
		char* buffer_recv_withead;//used for recv the whole msg
		char* buffer_recv;//used for recv the msg without head
		int totalsize_s;//the total buffer size should be sent
		int restsendsize;//the rest size of buffer should be sent
		int totalsize_r;//the total buffer size should be received
		int haverecvsize;//the size of buffer have been received
		int handle_s;
		int tag_s;
	}sockdata;
	typedef struct phoneinfo Phone;
	enum {
		ADD = 0,
		DEL,
		MOD
	};
    int    maxpid;
	int    pid; //master的pid = -1
    char   masterip[16];
	int    port;
    Phone  phonebook[MAX_NUM_IP];
    int    efd;//epoll
	int    listen_st;
	struct epoll_event ev;//这两个变量可以不作为属性
	struct epoll_event *evs;
public:
	Network(){};
	//~Network();
    void init(const char* masterip, int port, int predict_maxpid);//init the master()
    void print_phoneinfo();
public:
    void setup_to_accept(const char* ip, int p);
	int accept_worker();
protected:
	void event_op(int efd, int fd, sockdata* sd, int state, const int op);
	len_t blocksend(int fd, const char* msg, len_t size);
	len_t blockrecv(int fd, char* msg, len_t size);
};

class MasterNetwork : public Network{
private:
	int st_list[MAX_NUM_IP];
	char* buildphoneinfo();
	int construct_msg(int pid, char* buf, char **outmsg);
public:
	MasterNetwork(const char *masterip, int port, int predict_maxpid, int timeout);
	int master_accept_worker(int &num_workers);
    void waitforworkers(int timeout);//wait for all worker and then make phonebook 
	void sendphoneinfo();
	void closeallfd();
};

class WorkerNetwork : public Network{
private:
//	int wst_list[MAX_NUM_IP];
	char workerip[16];
	int worker_listenport;
	vector<int> rc_list;
	vector<int> sc_list;
	vector<sockdata*> SD;
	pthread_t wthread;
	pthread_mutex_t wmutex;
	int req_index;
	int act_isends;
	int act_irecvs;
	vector<int> req_handles;
	deque<msg*> send_msg_q;
	deque<msg*> recv_msg_q;
	int to_masterfd;
	int done;
	int act_barrier_msg;
	int act_barrier_go;
	void calc_tree();
	void tree_connect();
	sockdata* initsockdata(int rank);
	int make_socket_non_blocking(int fd);
	void handshake(int rank);
	void send_handshake_id(int dest, int st);
	int read_env(int fd, msg *env);
	void setmsg(msg &tofind, int src, int dest, int tag, int sys, int size);
	void send_sys_msg(int dest, int tag, int sys, int size, const char *buf);
	IT queue_find(deque<msg*> &msg_queue, msg &m);
	int asynch_queue_find(IT &pos, deque<msg*> &msg_queue, msg &m);
	void dealrecvzero(deque<msg*> &msg_queue, int srcrank, void* buff1, void* buff2, int recvsize);
	void setsockdata_s(sockdata* sd, char* buffer_send, int totalsize_s, int restsendsize, int handle_s, int tag_s);
	void setsockdata_r(sockdata* sd, char* totalbuff, char* buffer_recv, int totalsize_r, int haverecvsize);
	void process(int sys);
    void send_partmsg(int efd, char* sendmsg, int totalsize_s, int restsendsize, int destofrank, int handle_s, int tag_s );
    char* construct_sendmsg(int dest, int tag, int sys, int size, const char* buf);
public:
	struct communicator{
		int size;
		int rank;
		int pa;
		int lchild;
		int rchild;
	} C;
	int get_to_masterfd() { return to_masterfd; }
	int get_comm_size() { return this->maxpid; }
	int get_rank() { return this->pid; }
	//基于char的类型,进行试验
	int comm_isend(void *buf, int count, int dest, int tag, comm_request *handle);
	int comm_recv(void *buf, int count, int src, int tag, comm_status *status);
    int comm_irecv(void *buf, int count, int src, int tag, comm_request *handle);
	int comm_test(comm_request *rq, int *flag, comm_status* status);
	int comm_wait(comm_request *rq);
	//int comm_barrier();

	WorkerNetwork(const char *masterip, int port, int predict_maxpid);
	//用于worker间进行初始化
	int comm_init();
	void comm_thread();
	int connectserver(const char* serverip, int port);
	void recvphoneinfo();
	//在接受完之后主动关闭
	void close_to_masterfd();
};

#endif

