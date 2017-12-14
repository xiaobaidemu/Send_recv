/*************************************************************************
 > File Name: net.cpp
 > Author: He Boxin
 > Mail: heboxin@pku.edu.cn
 > Created Time: 2017年11月22日 星期三 17时14分42秒
 ************************************************************************/

#include <iostream>
#include "net.h"
#include "check.h"
#define FD_SIZE   64
#define MAXEVENTS 64
#define DEFAULT_BACKLOG  20
#define WORKER_LISTEN_PORT 7000

extern "C"
void * _run_thread (void *arg){
	if(arg != NULL){
		((WorkerNetwork*) arg)->comm_thread();
	}
	return NULL;
}	
//m1为队列中的数，m2为要在队列中查询的数
bool cmp(const msg* m1, const msg* m2){
    //printf("m1->s:%d m1->sys:%d m1->t:%d m2->s:%d m2->sys:%d m2->t:%d\n", 
    //        m1->src, m1->sys, m1->tag, m2->src, m2->sys, m2->tag);
	int rc;
	if(m2->sys == NON_SYS && m1->sys == NON_SYS){
		rc = 1;
        //队列中的消息不可能有anysource,或者anycomm这些
		if (m2->src == COMM_ANY_SOURCE && m2->tag == COMM_ANY_TAG) {
            rc=0; printf("find comm_any_source and comm_any_tag\n");
        }
		else if (m2->src == COMM_ANY_SOURCE) {
            if(m2->tag == m1->tag){
                rc = 0; printf("find comm_any_source and same tag\n");
            }
        }
		else if (m2->tag == COMM_ANY_TAG) {
            if(m2->src == m1->src) {
                rc = 0; printf("find comm_any_tag and same sourec\n");
            }
        }
		else if (m1->src == m2->src && m1->tag == m2->tag) {
            rc=0; printf("find same source and same tag\n");
        }
	}
    else if(m2->sys == m1->sys && (m1->sys == ISEND || m1->sys == IRECV)){
        printf("find ISEND/IRECV env\n");
    	return true;
    }
	else {
		rc = 0;
		if(m2->src != NA && m1->src != m2->src) rc++;
		if(m2->dest != NA && m1->dest != m2->dest) rc++;
		if(m2->tag != NA && m1->tag != m2->tag) rc++;
		if(m2->sys != NA && m1->sys != m2->sys) rc++;
		if(m2->size != NA && m1->size != m2->size) rc++;
	}
	return rc == 0 ? true : false;
}

void Network::init(const char *masterip, int port, int predict_maxpid)
{
	strcpy(this->masterip, masterip);
	this->port=port;
	maxpid = predict_maxpid;
	setup_to_accept(this->masterip, this->port);
	printf("create epoll_fd for Network.\n");
	efd = epoll_create(FD_SIZE); error_check(efd, "epoll_create efd");
	evs = (struct epoll_event*)calloc(MAXEVENTS, sizeof(ev));
	ev.data.fd = listen_st;
	ev.events = EPOLLIN;
	safecall(epoll_ctl(efd, EPOLL_CTL_ADD, listen_st, &ev));
}

void Network::setup_to_accept(const char* ip, int p){
	printf("Master node start accept connection.\n");
	struct sockaddr_in sin, from;
	int optvalue = 1;
	sin.sin_family = AF_INET;
	//sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(p);
	listen_st = socket(PF_INET, SOCK_STREAM, 0);
	error_check(listen_st, "master/worker node setup_to_accept socket");
	safecall(setsockopt(listen_st, SOL_SOCKET, SO_REUSEADDR, (char*) &optvalue, sizeof(optvalue)));	
	safecall(bind(listen_st, (struct sockaddr *)&sin, sizeof(sin)));
	safecall(listen(listen_st, DEFAULT_BACKLOG));
}

int Network::accept_worker(){
	int new_st, optval = 1; struct sockaddr_in from; socklen_t fromlen = sizeof(from);
	while(true){
		new_st = accept(listen_st, (struct sockaddr *)&from, &fromlen);
		if (new_st == -1){
			if(errno == EINTR) continue;
			else error_check(new_st, "accept_connection accept");
		}
		else break;
	}
	setsockopt(new_st, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
	return new_st;
}

int MasterNetwork::master_accept_worker(int &num_workers){
	char hbuf[NI_MAXHOST], sbuf[NI_MAXHOST];
	int new_st, optval = 1;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	while(true){
		new_st = accept(listen_st, (struct sockaddr *)&from, &fromlen);
		if (new_st == -1){
			if(errno == EINTR) continue;
			else error_check(new_st, "accept_connection accept");
		}
		else break;
	}
	assertcall(getnameinfo((sockaddr*)&from, fromlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),NI_NUMERICHOST | NI_NUMERICSERV), 0);
	printf("Accepted connection: st=%d, host=%s, port=%s\n", new_st, hbuf, sbuf);
	setsockopt(new_st, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
	//int new_st = accept_worker(hbuf, sbuf);
	strcpy(phonebook[num_workers].wip, hbuf);
	phonebook[num_workers].wpid = num_workers;
	phonebook[num_workers].wport = atoi(sbuf);
	//barrier
	return new_st;
}

void Network::event_op(int efd, int fd, sockdata* sd, int state, const int op){
	struct epoll_event event;
	event.data.ptr = (void*)sd;
	event.events = state;
	switch (op){
		case ADD:
			safecall(epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event));
			break;
		case DEL:
			safecall(epoll_ctl(efd, EPOLL_CTL_DEL, fd, &event));
			break;
		case MOD:
			safecall(epoll_ctl(efd, EPOLL_CTL_MOD, fd, &event));
			break;
		default:
			break;
	}
}	

len_t Network::blocksend(int fd, const char* buf, len_t size){
	len_t slen = 0; len_t rc;
	while(slen < size){
		rc = write(fd, buf, size-slen);
		if (rc > 0){
			slen += rc;
			buf += rc;
		}
		else if(rc == -1){
			errmsg("something wrong when send to socket %d, buf size %lu, have sent size %lu\n", 
					fd, size, slen);
			return -1;
		}
	}
	return slen;
}

len_t Network::blockrecv(int fd, char* buf, len_t size){
	len_t rlen = 0;
	while(rlen < size){
		len_t rc = read(fd, buf, size);
		if(rc > 0){
			rlen += rc;
			buf += rc;
		}
		else if(rc == -1){
			 errmsg("something wrong when recv from socket %d, buf size %lu, have recv size %lu\n",
					 fd, size, rlen);
			 return -1;
		}
	}
	return rlen;
}

void Network::print_phoneinfo(){
	printf("phoneinfo below ...\n");
	for(int i = 0;i < maxpid;i++){
		printf("%d: %s %d %d\n", i, phonebook[i].wip, phonebook[i].wpid, 
				                 phonebook[i].wport);
	}
}

MasterNetwork::MasterNetwork(const char *masterip, int port, int predict_maxpid, int timeout){
	this->init(masterip, port, predict_maxpid);
	waitforworkers(timeout);
	sendphoneinfo();
	closeallfd();
}

void MasterNetwork::closeallfd(){
	safecall(close(this->listen_st));
	for(int i = 0;i < maxpid;i++)
		safecall(close(st_list[i]));
}

void MasterNetwork::waitforworkers(int timeout){
	printf("master start waitforworkers.\n");
	int i, n, num_workers = 0;
	double start = now();
	while(num_workers < maxpid && now()-start < timeout){	
		n = epoll_wait(efd, evs, MAXEVENTS, 0);
		for(i = 0;i < n;i++){
			if((evs[i].events & EPOLLERR) || (evs[i].events & EPOLLHUP) || 
					!(evs[i].events & EPOLLIN)){
				fprintf(stderr, "master epoll efd error\n");
				close (evs[i].data.fd); continue;
			}
			else if(evs[i].data.fd == listen_st){
				int new_st = master_accept_worker(num_workers);
				st_list[num_workers] = new_st;
				num_workers++;
				continue;
			}
		}
	}
	if(num_workers < maxpid){
		printf("Only %d workers have been connected with master because of timeout.\n", num_workers);
		maxpid = num_workers;
	}
	else printf("workers num is %d.\n", maxpid);
}

//发送分配给worker的进程pid,以及全部的worker信息
void MasterNetwork::sendphoneinfo(){
	char* allphoneinfo = buildphoneinfo();
	for(int i = 0;i < maxpid;i++){
		char *msg = NULL;
		int size = construct_msg(i, allphoneinfo, &msg);
		assertcall(blocksend(st_list[i], msg, size), size);
		char ackmsg[9];
		assertcall(blockrecv(st_list[i], ackmsg, 9), 9);
		if(strcmp(ackmsg, "RECV ACK") == 0)
			printf("Send to %s phoneinfo finished.\n", phonebook[i].wip);
		if(msg) free(msg);
	}
	if(allphoneinfo) delete [] allphoneinfo;
}

char* MasterNetwork:: buildphoneinfo()
{
	char* phonestr = new char[16*MAX_NUM_IP+1];
	char tmpint[10];
	char* head = phonestr;int len;
	for(int i = 0;i < maxpid;i++){
		len = strlen(strcpy(head, phonebook[i].wip));
		*(head+len)=' '; head += (len+1); sprintf(tmpint, "%d", phonebook[i].wpid);
		len = strlen(strcpy(head, tmpint));
		*(head+len)=' '; head += (len+1); sprintf(tmpint, "%d", phonebook[i].wport);
		len = strlen(strcpy(head, tmpint));
		*(head+len)='|'; head += (len+1);
	}
	return phonestr;
}

int MasterNetwork::construct_msg(int pid, char* buf, char** outmsg){
	//发送的消息包括：maxpid,分配给对方的pid,phoneinfo的大小,buf字符串
	int size = strlen(buf), msglen;
	char env[31];
	sprintf(env, "%010d%010d%010d", maxpid, pid, size);
	printf("maxpid:%d pid:%d size:%d\n", maxpid, pid, size);
	msglen = 30 + size;
	*outmsg = (char*)malloc(msglen);
	memcpy(*outmsg, env, 30);
	memcpy(*outmsg + 30, buf, size);
	return msglen;
}

WorkerNetwork::WorkerNetwork(const char *masterip, int port, int predict_maxpid){
	strcpy(this->masterip, masterip);
	this->port=port;
	maxpid = predict_maxpid;
	connectserver(this->masterip, port);
	recvphoneinfo();
	close_to_masterfd();
}

void WorkerNetwork::close_to_masterfd(){
	safecall(close(to_masterfd));
}

int WorkerNetwork::connectserver(const char* serverip, int port){
	int i, rc, wst, optval = 1;
	struct sockaddr_in listener;
	struct hostent *hp;
	hp = gethostbyname(serverip);
	if(!hp){
		printf("Worker connect_to_server: gethostbyname %s: %s -- exiting\n", serverip, strerror(errno));
		exit(99);
	}
	bzero((void *)&listener, sizeof(listener));
	bcopy((void *)hp->h_addr, (void *)&listener.sin_addr, hp->h_length);
	listener.sin_family = hp->h_addrtype;
	listener.sin_port = htons(port);
	wst = socket(AF_INET, SOCK_STREAM, 0);
	error_check(wst, "Get worker's socket");
	rc = connect(wst, (struct sockaddr *) &listener, sizeof(listener));
	if(rc == -1){
		for(i = 0;i < 5000;i++){
			if ((rc = connect(wst, (struct sockaddr *) &listener, sizeof(listener))) == 0) 
				break;
			else sleep(i/4);
		}
	}
	error_check(rc, "worker_connect_to_server socket");
	setsockopt(wst, SOL_SOCKET, SO_REUSEADDR, (char*) &optval, sizeof(optval));
	setsockopt(wst, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
	to_masterfd = wst;
	return wst;
}
//接收全部的worker的ip信息并解析
void WorkerNetwork::recvphoneinfo(){
	int fd = to_masterfd, size;
	printf("start recvphoneinfo ... ");
	char head[31];
	assertcall(blockrecv(fd, head, 30), 30);
   	sscanf(head, "%010d%010d%010d", &(this->maxpid), &(this->pid), &size);
	char* phoneinfo = (char*)malloc(size);
	assertcall(blockrecv(fd, phoneinfo, size), size);
	const char *sep = "|";
	char* p = strtok(phoneinfo, sep);int i = 0;
	while(p){
		sscanf(p, "%s %d %d", phonebook[i].wip, &(phonebook[i].wpid), 
				&(phonebook[i].wport));
		p = strtok(NULL, sep);
		i++;
	}
	print_phoneinfo();
	//发送确认消息
	char ackmsg[9] = "RECV ACK";
	assertcall(blocksend(fd, ackmsg, 9), 9);
	printf("end recvphoneinfo ...");
}

int WorkerNetwork::comm_init(){
	done = 0;
	strcpy(workerip, phonebook[pid].wip);
	worker_listenport = pid + WORKER_LISTEN_PORT; 
	rc_list.resize(maxpid, -1);
	sc_list.resize(maxpid, -1);
	SD.resize(maxpid);
	C.size = maxpid;
	C.rank = pid;
	req_index = 0;
	act_isends = 0; act_irecvs = 0;
	act_barrier_msg = 0; act_barrier_go = 0;
	req_handles.resize(1000, 0);
	calc_tree();
	setup_to_accept(workerip, worker_listenport);
	tree_connect();
	pthread_mutex_init(&wmutex, NULL);
	pthread_create(&wthread, NULL, _run_thread, this);
	//上面这个需要修改
	printf("Worker %s have finished comm_init.\n", workerip);
}
//类似堆的构成
void WorkerNetwork::calc_tree(){
	int l = 2 * pid + 1;
	int r = 2 * pid + 2;
	if (pid == 0) C.pa = -1;
	else C.pa = (pid - 1)/2;
	if (l > C.size - 1) C.lchild = -1;
	else C.lchild = l;
	if (r > C.size - 1) C.rchild = -1;
	else C.rchild = r;
	printf("[pid:%d] pa=%d lc=%d lr=%d\n", C.rank, C.pa, C.lchild, C.rchild);
}

void WorkerNetwork::tree_connect(){
	int i, cc = 0, fds[2];	
	if(C.lchild != -1) fds[cc++] = C.lchild; 
	if(C.rchild != -1) fds[cc++] = C.rchild;
	if(C.rank != 0){
		rc_list[C.pa] = sc_list[C.pa] = accept_worker();
		SD[C.pa] = initsockdata(C.pa);
		safecall(make_socket_non_blocking(rc_list[C.pa]));
		printf("[pid:%d] accept %d\n",C.rank, C.pa);
	}
	if(cc > 0){
		for(i = 0;i < cc;i++){
			sc_list[fds[i]] = rc_list[fds[i]] = 
			connectserver(phonebook[fds[i]].wip, WORKER_LISTEN_PORT + fds[i]);
			SD[fds[i]]	= initsockdata(fds[i]);
			safecall(make_socket_non_blocking(rc_list[fds[i]]));
			printf("[pid:%d] connect %d(%s,%d)\n",C.rank, fds[i], phonebook[fds[i]].wip, WORKER_LISTEN_PORT + fds[i]);
		}
	}
	
}

int WorkerNetwork::make_socket_non_blocking (int fd){
	int flags, s;
	flags = fcntl (fd, F_GETFL, 0);
  	if (flags == -1) {
      perror ("fcntl");
      return -1;
    }
	flags |= O_NONBLOCK;
  	s = fcntl (fd, F_SETFL, flags);
  	if (s == -1) {
      perror ("fcntl");
      return -1;
    }
	return 0;
}
//此处之后要变为非malloc分配
Network::sockdata*  WorkerNetwork::initsockdata(int rank){
    sockdata *sd = (sockdata*)malloc(sizeof(sockdata));
    sd->rank = rank;
    sd->buffer_send = NULL;
    sd->buffer_recv_withead = NULL;
    sd->buffer_recv= NULL;
    sd->totalsize_s = 0;
    sd->restsendsize = 0;
    sd->totalsize_r = -1;
    sd->haverecvsize = 0;
    sd->handle_s = -1;
    sd->tag_s = -1;
    printf("initsockdata...\n");
    return sd;
}

void WorkerNetwork::handshake(int rank){
	int new_st; msg env; 
	if(sc_list[rank] == -1){
		new_st = connectserver(phonebook[rank].wip, rank + WORKER_LISTEN_PORT);
		send_handshake_id(rank, new_st);
		if(rank == C.rank){//自己和自己握手
			sc_list[rank] = new_st;
			SD[rank] = initsockdata(C.rank);
			while(rc_list[rank] == -1){}
		}
		else{
			read_env(new_st, &env);
			rc_list[rank] = sc_list[rank] = new_st;
			SD[rank] = initsockdata(rank);
		}
		safecall(make_socket_non_blocking(new_st));
		printf("[pid:%d] handshake with %d finished.\n", pid, rank);
	}
}

void WorkerNetwork::send_handshake_id(int dest, int st){
	char msgTag[51];
	sprintf(msgTag, "%010d%010d%010d%010d%010d", C.rank, dest, 0, HANDSHAKE, 0);
	blocksend(st, msgTag, 50);	
	printf("[pid:%d] send_handshake_msg to pid %d.\n", pid, dest);
}

void WorkerNetwork::send_sys_msg(int dest, int tag, int sys, int size, const char *buf){
	int i, msglen;
	char env[51], *message;
	sprintf(env, "%010d%010d%010d%010d%010d", C.rank, dest, tag, sys, size);
	int othersize = size < 0 ? 0 : size;
	msglen = 50 + othersize;
	message = (char*)malloc(msglen);
	memcpy(message, env, 50); memcpy(message+50, buf, othersize);
	if(sc_list[dest] == -1) 
		printf("[pid%d] ERROR -- empty fd for dest %d.\n", pid, dest);
	blocksend(sc_list[dest], message, msglen);
	if(message) { free(message); message = NULL;}
}

int WorkerNetwork::read_env(int fd, msg *env){
	char buf[51];int size, rc;
	env->msg = NULL;
	rc = blockrecv(fd, buf, 50);
	sscanf(buf, "%010d%010d%010d%010d%010d", &env->src, &env->dest, &env->tag, &env->sys, &env->size);
	if(rc != -1)
		printf("[pid:%d] s:%d d:%d t:%d sys:%d sz:%d\n", pid, env->src, env->dest, env->tag, env->sys, env->size);
	return rc;
}

char* WorkerNetwork::construct_sendmsg(int dest, int tag, int sys, int size, const char* buf){
    int msglen;
    char env[51], *message;
    sprintf(env, "%010d%010d%010d%010d%010d", C.rank, dest, tag, sys, size);
    printf("[pid:%d] send msg ==> s:%d d:%d t:%d sys:%d sz:%d\n", 
                               pid, C.rank, dest, tag, sys, size);
    if(size < 0) msglen = 50;
    else msglen = 50 + size;
    message = (char*)malloc(msglen);
    memcpy(message, env, 50);
    memcpy(message + 50, buf, size);
    if(sc_list[dest] == -1) printf("[pid:%d] BAD fd for %d\n", pid, dest);
    return message;
}
int WorkerNetwork::comm_isend(void *buf, int count, int dest, int tag, comm_request * handle){
	printf("[pid:%d] begin isend to dest %d, tag %d.\n", pid, dest, tag);
	handshake(dest);
	pthread_mutex_lock(&wmutex);
	*handle = req_index;
	req_handles[req_index++]=0;
	pthread_mutex_unlock(&wmutex);
	msg *msgEnv, *isendEnv;

	msgEnv = (msg*)malloc(sizeof(msg));//must to free
	msgEnv->src = C.rank; msgEnv->dest = dest; msgEnv->tag = tag;
	msgEnv->sys = NON_SYS;msgEnv->size = count; msgEnv->msg = buf;
	//msgEnv->msg = (char *)malloc(count);
	//memcpy(msgEnv->msg, (char*)buf, count);
	
	isendEnv = (msg *)malloc(sizeof(msg));
	isendEnv->src = 0; isendEnv->dest = 0; isendEnv->tag = *handle;
	isendEnv->sys = ISEND; isendEnv->msg = (void*)msgEnv;

	pthread_mutex_lock(&wmutex);
	//将isendEnv插入到双端队列中
	send_msg_q.push_back(isendEnv);	
	act_isends++;
	pthread_mutex_unlock(&wmutex);
    printf("[pid:%d] end isend env=> s:%d d:%d t:%d sys:%d sz:%d actisend:%d\n", 
            pid, msgEnv->src, msgEnv->dest, msgEnv->tag, msgEnv->sys, msgEnv->size, act_isends);
	return 0;
}

void WorkerNetwork::print_queue(list<msg*> &queue){
    pthread_mutex_lock(&wmutex);
    IT pos = queue.begin();int i = 0;
    for(pos;pos != queue.end();pos++){
        msg * m = (*pos);
        printf("----> element%d. s:%d t:%d d:%d \n",i, m->src, m->tag, m->dest);
        i++;
    }
    pthread_mutex_unlock(&wmutex);
}
int WorkerNetwork::comm_recv(void *buf, int count, int src, int tag, comm_status *status){
	int i; msg tofind;
	IT retpos;
    sleep(2);
	printf("[pid:%d] begin recv from %d, tag %d\n", pid, src, tag);	
	while(rc_list[src] == -1){}
	setmsg(tofind, src, NA, tag, NON_SYS, NA);
    printf("[pid:%d] start find env s:%d d:%d t:%d sys:%d sz:%d.\n", 
            pid, src, NA, tag, NON_SYS, NA);
    printf("========queue size recv_msg_q:%d\n", recv_msg_q.size());
    print_queue(recv_msg_q);
	retpos = queue_find(recv_msg_q, tofind);
    printf("[pid:%d] have finished comm_recv queue_find.\n", pid);
	memmove(buf, (*retpos)->msg, (*retpos)->size);
	if(status != NULL){
		status->count = (*retpos)->size; status->cancelled = 0;
		status->comm_src = (*retpos)->src; status->comm_tag = (*retpos)->tag;
		status->comm_error = COMM_SUCCESS;
	}	
	pthread_mutex_lock(&wmutex);
	//将找到这个msg,删除并释放内存，此处要注意
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!free
	//free 注意一定要有free操作
	msg* delmsg = *retpos;
	recv_msg_q.erase(retpos);
	if(delmsg->msg) free(delmsg->msg);
    if(delmsg) {free(delmsg); delmsg = NULL;}
	pthread_mutex_unlock(&wmutex);
	printf("[pid:%d] finished recv from %d, tag %d\n", pid, src, tag);
}

int WorkerNetwork::comm_irecv(void *buf, int count, int src, int tag, comm_request *handle){
	msg *msgPtr = (msg*)malloc( sizeof(msg));
	msg *infoPtr= (msg*)malloc( sizeof(msg));
	pthread_mutex_lock(&wmutex);
	*handle = req_index;
	req_handles[req_index++] = 0;
	pthread_mutex_unlock(&wmutex);
	
	infoPtr->src = NA; infoPtr->dest = NA;
	infoPtr->tag = *handle; infoPtr->sys = NA;
	infoPtr->msg = buf; infoPtr->size = count;

	msgPtr->src = src; msgPtr->dest = NA;
	msgPtr->tag = tag; msgPtr->sys = IRECV;
	msgPtr->size = count; msgPtr->msg = infoPtr;
	pthread_mutex_lock(&wmutex);
	recv_msg_q.push_back(msgPtr);
	act_irecvs++;
	pthread_mutex_unlock(&wmutex);
	printf("[pid:%d] Irecv function return.\n", pid);		
}

int WorkerNetwork::comm_test(comm_request *rq, int *flag, comm_status* status){
	*flag = 0;
	if(req_handles[*rq] != 0) *flag = 1;
	return *flag;
}

int WorkerNetwork::comm_wait(comm_request *rq){
	printf("[pid:%d] MPI_wait on %d.\n", pid, *rq);
	while(!req_handles[*rq]) {
		sched_yield();
	}
	return 0;
}
void WorkerNetwork::setmsg(msg &tofind, int src, int dest, int tag, int sys, int size){
	tofind.src = src; tofind.dest = dest;
	tofind.tag = tag; tofind.sys = sys; tofind.size = size;
}
//此处可以修改为使用信号量进行控制的查询
//表示的意思是在找到消息后，并删除
IT WorkerNetwork::queue_find(list<msg*> &msg_queue, msg &m){
	IT pos; int flag = 1;
	while(flag){
		pthread_mutex_lock(&wmutex);
		if(msg_queue.size() > 0){
			pos = std::find_if(msg_queue.begin(), msg_queue.end(), std::bind2nd(std::ptr_fun(cmp), &m));
			if(pos != msg_queue.end()) flag = 0;
		}
		pthread_mutex_unlock(&wmutex);
	}
	//printf("[pid:%d] queue_find.\n", pid);
	return pos;
}

int WorkerNetwork::asynch_queue_find(IT &pos, list<msg*> &msg_queue, msg &m){
	IT tmppos;
	int rt = 0;
	pthread_mutex_lock(&wmutex);
	if(msg_queue.size() > 0){
		tmppos = std::find_if(msg_queue.begin(), msg_queue.end(), std::bind2nd(std::ptr_fun(cmp), &m));
		if(tmppos != msg_queue.end()) {
			pos = tmppos;
			rt = 1;
		}
	}	
	pthread_mutex_unlock(&wmutex);
	printf("[pid:%d] asynch_queue_find.\n", pid);
	return rt;
}

void WorkerNetwork::process(int sys){
	if(sys == BARRIER) act_barrier_msg++;
	else if(sys == BARRIER_GO) act_barrier_go++;
}
void WorkerNetwork::dealrecvzero(list<msg*> &msg_queue, int srcrank, void* buff1, void* buff2, int recvsize){
	if(errno != EAGAIN){
        printf("[pid:%d] dealrecvzero errno = %s.\n",pid, strerror(errno));
		msg toFind; IT pos;
		setmsg(toFind, srcrank, NA, NA, NA, NA);
		if(asynch_queue_find(pos, msg_queue, toFind) == 0){
			close(rc_list[srcrank]);
			rc_list[srcrank] == -1;
			printf("[pid:%d] abnormal closing pid %d.\n", pid, srcrank);
		}
		if(buff1 - recvsize) free(buff1 - recvsize);
		if(buff2 != NULL) free(buff2);
	}
}

void WorkerNetwork::setsockdata_s(sockdata* sd, char* buffer_send, int totalsize_s, int restsendsize, int handle_s, int tag_s){
 	sd->buffer_send = buffer_send; sd->totalsize_s = totalsize_s;
    sd->restsendsize = restsendsize; sd->handle_s = handle_s;
    sd->tag_s = tag_s;
}

void WorkerNetwork::setsockdata_r(sockdata* sd, char* totalbuff, char* buffer_recv, int totalsize_r, int haverecvsize){
	sd->buffer_recv_withead = totalbuff; sd->buffer_recv = buffer_recv;
	sd->totalsize_r = totalsize_r; sd->haverecvsize = haverecvsize;
}

void WorkerNetwork::comm_thread(){
    printf("[pid%d] IN THREAD begin thread.\n", pid);
	int i, j, rc, n, destrank, printime = 0, new_st;
	struct epoll_event event, *events;
	efd = epoll_create(FD_SIZE);
	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));
	sockdata default_sd = {-listen_st-1, NULL, NULL, NULL, 0, 0, -1, 0, -1,-1};
	event.data.ptr = &default_sd; event.events = EPOLLIN;
	safecall(epoll_ctl(efd, EPOLL_CTL_ADD, listen_st, &event));
	if(C.pa != -1 && rc_list[C.pa] != -1)
		event_op(efd, rc_list[C.pa], SD[C.pa], EPOLLIN, ADD);
	if(C.lchild != -1 && rc_list[C.lchild] != -1)
		event_op(efd, rc_list[C.lchild], SD[C.lchild], EPOLLIN, ADD);
	if(C.rchild != -1 && rc_list[C.rchild] != -1)
		event_op(efd, rc_list[C.rchild], SD[C.rchild], EPOLLIN, ADD);
	while(!done){
		n = epoll_wait(efd, events, MAXEVENTS, 0);
		for(i = 0;i < n;i++){
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)){
                int index = ((sockdata*)(events[i].data.ptr))->rank;
                event_op(efd, rc_list[index], SD[index], EPOLLOUT|EPOLLIN, DEL);
                printf("[pid:%d] EPOLLERR/EPOLLHUP pid = %d\n",pid, index);
				continue;
			}
			else if(((sockdata*)(events[i].data.ptr))->rank < 0 && (events[i].events & EPOLLIN)){
				msg* hsEnv = (msg*)malloc(sizeof(msg));
				new_st = accept_worker();
				read_env(new_st, hsEnv);
				if(hsEnv->sys == HANDSHAKE){
					if(hsEnv->src == C.rank){
						rc_list[hsEnv->src] = new_st;
						//SD[C.rank] = initsockdata(C.rank);
						event_op(efd, new_st, SD[C.rank], EPOLLIN, ADD);
					}
					else{
						sc_list[hsEnv->src] = rc_list[hsEnv->src] = new_st;
						SD[hsEnv->src] = initsockdata(hsEnv->src);
						event_op(efd, new_st, SD[hsEnv->src], EPOLLIN, ADD);
						send_sys_msg(hsEnv->src, 0, RECV_OK, 0, NULL);
					}
					printf("[pid:%d] complete handshake with pid %d.\n", pid, hsEnv->src);
				}
				else{	
					printf("[pid:%d] ERROR no HS msg recv from pid %d.\n", pid, hsEnv->src);
				}
				safecall(make_socket_non_blocking(new_st));
				if(hsEnv) {free(hsEnv);hsEnv = NULL;}
			}
			else if(events[i].events & EPOLLIN) {
				printf("[pid:%d] now (events[i].events & EPOLLIN): events[%d].events = %d\n", pid, i, events[i].events);
				sockdata* ptr = (sockdata*)(events[i].data.ptr);
				int srcrank = ptr->rank, readsize;
				//首先读取50字节的消息头
				if(ptr->totalsize_r == -1){
					char *head;//!!!!!!!!!!!!!must to free
					int recvheadsize = ptr->haverecvsize;
					if(ptr->buffer_recv == NULL) head = (char*)malloc(50);
					else head = ptr->buffer_recv;
					printf("[pid:%d] try reading (head) from srcrank=%d\n", pid, srcrank);
					readsize = read(rc_list[srcrank], head, 50 - recvheadsize);
                    if(readsize < 0) {
						if (errno == EAGAIN) { continue; }
                        printf("[pid:%d] exit read head readsize<0 error:%s\n", pid, strerror(errno));
                        exit(99);
                    }
						//dealrecvzero(recv_msg_q, srcrank, head, NULL, recvheadsize);
					else if(readsize == 0){
						close(rc_list[srcrank]);
						rc_list[srcrank] = -1;
						if(head - recvheadsize) free(head - recvheadsize);
						printf("[pid:%d] normal closing %d when recv head.\n", pid, srcrank);
					}
					else if(readsize < 50 - recvheadsize){
						setsockdata_r(SD[srcrank], NULL, head + readsize, -1, recvheadsize + readsize);
						printf("[pid:%d] recv part head size %d from %d.\n", pid, recvheadsize + readsize, srcrank);	
					}
					else if(readsize == 50 - recvheadsize){
						msg* envmsg = (msg*)malloc(sizeof(msg));//remember to free!!!!!
						sscanf(head-recvheadsize, "%010d%010d%010d%010d%010d", &envmsg->src, &envmsg->dest,
										           &envmsg->tag, &envmsg->sys, &envmsg->size);
						printf("[pid:%d] recv s:%d d:%d t:%d sys:%d sz:%d\n",pid, envmsg->src, 
                                     envmsg->dest, envmsg->tag, envmsg->sys, envmsg->size);
						process(envmsg->sys);
                        if(head - recvheadsize) free(head - recvheadsize);
						if(envmsg->size > 0){
							envmsg->msg = malloc(envmsg->size);//must to free!!!!
							readsize = read(rc_list[srcrank], envmsg->msg, envmsg->size);
							setsockdata_r(SD[srcrank], (char*)envmsg, (char*)envmsg->msg, 0, 0);
							if(readsize < 0){
                                printf("[pid:%d] exit read body readsize<0 error:%s\n", strerror(errno));
                                exit(99);
								//dealrecvzero(recv_msg_q, srcrank, envmsg->msg, envmsg, 0);
							}
							else if(readsize == 0){
								close(rc_list[srcrank]);
								rc_list[srcrank] = -1;
								if(envmsg->msg) {free(envmsg->msg); envmsg->msg = NULL;}
								if(envmsg) { free(envmsg); envmsg = NULL;}
								printf("[pid:%d] normal closing %d when recv msg.\n", pid, srcrank);
							}
							else if(readsize < envmsg->size){
								setsockdata_r(SD[srcrank], (char*)envmsg, (char*)envmsg->msg, envmsg->size, readsize);
								printf("[pid:%d] have recv the part msg, size = %d.\n", pid, readsize);
							}
							else if(readsize == envmsg->size){
								pthread_mutex_lock(&wmutex);
								recv_msg_q.push_back(envmsg);
								pthread_mutex_unlock(&wmutex);
								setsockdata_r(SD[srcrank], NULL, NULL, -1, 0);
								printf("[pid:%d] have recv the whole msg from %d sz:%d.\n",
															 pid, envmsg->src, envmsg->size);
							}
						}
					}	
				}
				else{
					char* _msg = ptr->buffer_recv;
                    int totalsize_r = ptr->totalsize_r;
                    int recvmsgsize = ptr->haverecvsize;
					printf("[pid:%d] try reading (body) from srcrank=%d\n", pid, srcrank);
                    int readsize = read(rc_list[srcrank], _msg, totalsize_r - recvmsgsize);    
                    if(readsize < 0){
						if (errno == EAGAIN) { continue; }
                        printf("[pid:%d] exit read head readsize<0 error:%s\n", pid, strerror(errno));
                        exit(99);
                    }
                        //dealrecvzero(recv_msg_q, srcrank, _msg, ptr->buffer_recv_withead, recvmsgsize);
                    else if(readsize == 0){
                        close(rc_list[srcrank]);
                        rc_list[srcrank] = -1;
                        if(_msg - recvmsgsize) free(_msg - recvmsgsize);
                        if(ptr->buffer_recv_withead) free(ptr->buffer_recv_withead);
                        printf("[pid:%d] normal closing %d when recv real msg.\n", pid, srcrank);
                    }
                    else if(readsize < totalsize_r - recvmsgsize){
                        setsockdata_r(SD[srcrank], ptr->buffer_recv_withead, _msg + readsize, totalsize_r, recvmsgsize + readsize);
                        printf("[pid:%d] have recv the part msg from %d, size = %d\n",
												pid, srcrank, recvmsgsize + readsize);
                    }
                    else if(readsize == totalsize_r - recvmsgsize){//set the buffer_recv = null & totalsize_r = -1, haverecvsize = 0
                        pthread_mutex_lock(&wmutex);
						recv_msg_q.push_back((msg*)(ptr->buffer_recv_withead));
                        pthread_mutex_unlock(&wmutex);
                        setsockdata_r(SD[srcrank], NULL, NULL, -1, 0);
                        printf("[pid:%d] have recv the whole msg from %d size = %d\n", 
									pid, srcrank, recvmsgsize + readsize);
                    }		
				}
			}
			else if(events[i].events & EPOLLOUT){
				sockdata* ptr = (sockdata*)(events[i].data.ptr);
				if(ptr->buffer_send != NULL)
					send_partmsg(efd, ptr->buffer_send, ptr->totalsize_s, 
							ptr->restsendsize, ptr->rank, ptr->handle_s, ptr->tag_s);
			}
            //可以再开一个线程耶	
		}
        if(act_isends > 0){
            //printf("[pid%d] IN THREAD ready to send.\n", pid);
            msg toFind;
            setmsg(toFind, NA, NA, NA, ISEND, NA);
            IT isendpos = queue_find(send_msg_q, toFind);
            printf("[pid:%d] have found a ISEND env.\n", pid);
            msg* msgPtr = (msg*)((*isendpos)->msg);
            int destofrank = msgPtr->dest;
            if(SD[destofrank]->buffer_send == NULL){
                char *sendmsg = construct_sendmsg(destofrank, msgPtr->tag, NON_SYS, msgPtr->size, (char*)msgPtr->msg);
                //释放isendpos指向的
                int totalsize = 50;
                if(msgPtr->size > 0) totalsize += msgPtr->size; 
                int sendsize = write(sc_list[destofrank], sendmsg, totalsize);
                if(sendsize < 0){
                    if(errno != EAGAIN && errno != EINTR) perror("sendmsg error");
                    else{
                        setsockdata_s(SD[destofrank], sendmsg, totalsize, totalsize, (*isendpos)->tag, msgPtr->tag);
                        if(destofrank != C.rank)
                            event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLIN | EPOLLOUT, MOD);
                        else
                            event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLOUT, ADD);
                    } 	
                }
                else if(sendsize == totalsize){
                    pthread_mutex_lock(&wmutex);
                    req_handles[(*isendpos)->tag] = 1;
                    pthread_mutex_unlock(&wmutex);
                    printf("[pid:%d] Isend complete to %d have sendsize = %d\n", pid, destofrank, sendsize);
                    free(sendmsg);
                }
                else if(sendsize < totalsize){
                    setsockdata_s(SD[destofrank], sendmsg + sendsize, totalsize, totalsize - sendsize, (*isendpos)->tag, msgPtr->tag);
                    if(destofrank != C.rank)
                        event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLIN | EPOLLOUT, MOD);
                    else
                        event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLOUT, ADD);
                    printf("[pid:%d] have send part size = %d\n", pid, sendsize);
                }
                if(msgPtr) {free(msgPtr); msgPtr=NULL;}
                if((*isendpos)) {free((*isendpos)); (*isendpos) = NULL;}
                pthread_mutex_lock(&wmutex);
                send_msg_q.erase(isendpos);
                act_isends--;
                pthread_mutex_unlock(&wmutex);
            }
        }
        if(act_irecvs > 0){
            msg toFind;
            setmsg(toFind, NA, NA, NA, IRECV, NA);
            IT irecvpos = queue_find(recv_msg_q, toFind), irecvpos1;
            msg *infoPtr = (msg*)((*irecvpos)->msg);
            setmsg(toFind, (*irecvpos)->src, NA, (*irecvpos)->tag, NON_SYS, (*irecvpos)->size);
            int ret = asynch_queue_find(irecvpos1, recv_msg_q, toFind);
            if(ret == 1){
                memcpy(infoPtr->msg, (*irecvpos1)->msg, infoPtr->size);
                pthread_mutex_lock(&wmutex);
                req_handles[infoPtr->tag] = 1;
                recv_msg_q.erase(irecvpos);
                recv_msg_q.erase(irecvpos1);
                if(infoPtr) {free(infoPtr); infoPtr = NULL;}
                act_irecvs--;
                pthread_mutex_unlock(&wmutex);
            }
        }
        printime++;
	}	
}

void WorkerNetwork::send_partmsg(int efd, char* sendmsg, int totalsize_s, int restsendsize, int destofrank, int handle_s, int tag_s ){
	int sendsize = write(sc_list[destofrank], sendmsg, restsendsize);
	if(sendsize < 0){
		if(errno == EAGAIN || errno == EINTR) return;
        else perror("sendmsg error");
	}
	else if(sendsize == restsendsize)//set buffer_send null & remove epollout
    {
        setsockdata_s(SD[destofrank], NULL, 0, 0, -1, -1);
        if(destofrank != C.rank)
            event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLIN, MOD);
        else
            event_op(efd, sc_list[destofrank], SD[destofrank], EPOLLOUT, DEL);
        pthread_mutex_lock(&wmutex);
        req_handles[handle_s] = 1;
        pthread_mutex_unlock(&wmutex);
        printf("[pid:%d]  Isend complete for tag %d, have sendsize = %d\n",pid, tag_s, totalsize_s);
        if(sendmsg +sendsize - totalsize_s)free(sendmsg + sendsize - totalsize_s);
    }
    else if(sendsize < restsendsize)//change buufer_send & modify_event epollout
    {
        setsockdata_s(SD[destofrank], sendmsg + sendsize, totalsize_s,  restsendsize - sendsize, handle_s, tag_s);
        printf("[pid:%d] send_partmsg: have sendsize = %d\n",pid, totalsize_s - restsendsize + sendsize);
    }
}
