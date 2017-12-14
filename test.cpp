#include "net.h"
#include <string.h>
#include <stdlib.h>

#define SIZE (1024*1024*1024)
void test_Isend_recv(const char* masterip, int masterport, int maxpid);
void test_Isend_Irecv(const char* masterip, int masterport, int maxpid);
void test_Big(const char* masterip, int masterport, int maxpid);
void test_alltoall_small(const char* masterip, int masterport, int maxpid);

    
int main(int argc, const char * argv[]){
	int masterport = 8800;
	int maxpid = 4;
	const char * masterip = "127.0.0.1";
	for(int i = 1;i < argc;++i){
		if(strcmp(argv[i], "master") == 0){
			MasterNetwork master(masterip, masterport, maxpid, 60);
		}
		else{
            //test_Isend_Irecv(masterip, masterport, maxpid);
			//test_Isend_recv(masterip, masterport, maxpid);
            //test_Big(masterip, masterport, maxpid);
			test_alltoall_small(masterip, masterport, maxpid);
		}

	}
	return 0;
}

void test_Isend_recv(const char* masterip, int masterport, int maxpid){
    WorkerNetwork worker(masterip, masterport, maxpid);
    worker.comm_init();
    int myrank = worker.get_rank(); int size = worker.get_comm_size();
    printf("[pid:%d] myrank is &d in %d process\n", worker.pid, myrank, size);
    comm_request rq_send1, rq_send2;
    comm_status st_recv1, st_recv2;
    char buffer_recv1[20],buffer_recv2[20], buffer_send1[20],buffer_send2[20];
    if(myrank == 0){
        memcpy(buffer_send1, "Hello p1, I am p0.\n", 20);
    }
    else{
        memcpy(buffer_send1, "Hello p1, I am p1.\n", 20);
    }
    
    if(myrank == 0){
        worker.comm_isend(buffer_send1, 20, 1, 123, &rq_send1);
        worker.comm_recv(buffer_recv1, 20, 1, 456, &st_recv1);
    }
    else{
        worker.comm_isend(buffer_send1, 20, 0, 456, &rq_send1);
        worker.comm_recv(buffer_recv1, 20, 0, 123, &st_recv1);
    }
    if(myrank == 1 || myrank == 0 ){
        worker.comm_wait(&rq_send1);
    }
}


void test_Isend_Irecv(const char* masterip, int masterport, int maxpid){ 
    WorkerNetwork worker(masterip, masterport, maxpid);
    worker.comm_init();
    int myrank = worker.get_rank(); int size = worker.get_comm_size();
    printf("[pid:%d] myrank is &d in %d process\n", worker.pid, myrank, size);
    comm_request rq_send1, rq_send2, rq_recv1, rq_recv2;
    char buffer_recv1[20],buffer_recv2[20], buffer_send1[20],buffer_send2[20];
    if(myrank == 0){
        memcpy(buffer_send1, "Hello p1, I am p0.\n", 20);
        memcpy(buffer_send2, "Hello again to p1.\n", 20);
    }
    else{
        memcpy(buffer_send1, "Hello p0, I am p1.\n", 20);
        memcpy(buffer_send2, "Hello again to p0.\n", 20);
    }
    if(myrank == 0){
        worker.comm_irecv(buffer_recv1, 20, 1, 456, &rq_recv1);
        worker.comm_isend(buffer_send1, 20, 1, 123, &rq_send1);
        worker.comm_isend(buffer_send2, 20, 1, 789, &rq_send2);
        worker.comm_irecv(buffer_recv2, 20, 1, 789, &rq_recv2);
    }
    if(myrank == 1){
        worker.comm_irecv(buffer_recv1, 20, 0, 123, &rq_recv1);
        worker.comm_isend(buffer_send1, 20, 0, 456, &rq_send1);
        worker.comm_isend(buffer_send2, 20, 0, 789, &rq_send2);
        worker.comm_irecv(buffer_recv2, 20, 0, 789, &rq_recv2);
    }
    if(myrank == 1 || myrank == 0){
        worker.comm_wait(&rq_send1);
        worker.comm_wait(&rq_recv1);
        printf("[PID:%d] %s", myrank, buffer_recv1);
        worker.comm_wait(&rq_send2);
        worker.comm_wait(&rq_recv2);
        printf("[PID:%d] %s", myrank, buffer_recv2);
    }
}
void test_Big(const char* masterip, int masterport, int maxpid){
    WorkerNetwork worker(masterip, masterport, maxpid);
    worker.comm_init();
    int myrank = worker.get_rank(); int size = worker.get_comm_size();
    printf("[pid:%d] myrank is &d in %d process\n", worker.pid, myrank, size);
    comm_request rq_send1, rq_recv1;
    char* buffer_send = (char*)calloc(SIZE, sizeof(char));
    char* buffer_recv = (char*)malloc(SIZE);
    if(myrank == 0){ 
        worker.comm_irecv(buffer_recv, SIZE, 1, 456, &rq_recv1);
        worker.comm_isend(buffer_send, SIZE, 1, 123, &rq_send1);
    }
    if(myrank == 1){
        worker.comm_irecv(buffer_recv, SIZE, 0, 123, &rq_recv1);
        worker.comm_isend(buffer_send, SIZE, 0, 456, &rq_send1);
    }
    if(myrank == 1 || myrank == 0 ){
        worker.comm_wait(&rq_recv1);
        printf("[PID:%d] have recv the whole data.\n", myrank);
        worker.comm_wait(&rq_send1);
        printf("[PID:%d] have send the whole data.\n", myrank);
    }
}

void test_alltoall_small(const char* masterip, int masterport, int maxpid){	
    WorkerNetwork worker(masterip, masterport, maxpid);
	worker.comm_init();
    int myrank = worker.get_rank(); int size = worker.get_comm_size();
    printf("[pid:%d] myrank is &d in %d process\n", worker.pid, myrank, size);
	char sendmsg[50];
	char *allrecv = (char*)calloc(50*size, 1);
	char *tmprecv = allrecv;
	sprintf(sendmsg, "!!!!!!!!!!!!!! It is rank %d.!!!!!!!!!!!!\n", myrank);
	comm_request *reqs = (comm_request*)malloc(size*sizeof(comm_request));
	for(int i = 0;i < size;i++){
		worker.comm_isend(sendmsg, 50, i, myrank, &reqs[i]);
	}
	for(int i=0;i < size;i++){
		comm_status status;
		worker.comm_recv(allrecv, 50, i, i, &status);
		printf("[pid:%d] recv msg from %d. msg is %s", myrank, i, allrecv);
		allrecv += 50;

	}
	for(int i = 0;i < size;i++){
		worker.comm_wait(&reqs[i]);
	}

	printf("*************************\n");
	allrecv = tmprecv;
	for(int i = 0;i < size;i++){
		printf("%s", allrecv); allrecv += 50;
	}
	sleep(5);
	//free(reqs);
	//free(tmprecv);
}
