#include "net.h"
#include "string.h"

void runtestworker(const char* masterip, int masterport, int maxpid){
    WorkerNetwork worker(masterip, masterport, maxpid);
    worker.comm_init();
    int myrank = worker.get_rank(); int size = worker.get_comm_size();
    printf("[pid:%d] myrank is &d in %d process\n", worker.pid, myrank, size);
    comm_request rq_send1, rq_send2;
    comm_status st_recv1, st_recv2;
    char buffer_recv1[20],buffer_recv2[20], buffer_send1[20],buffer_send2[20];
    if(myrank == 0){
        memcpy(buffer_send1, "Hello p1, I am p0.\n", 20);
        memcpy(buffer_send2, "Hello again to p1.\n", 20);
    }
    else{
        memcpy(buffer_send1, "Hello p1, I am p1.\n", 20);
        memcpy(buffer_send2, "Hello again to p0.\n", 20);
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
        worker.comm_wait(&rq_send1);
    }
}

int main(int argc, const char * argv[]){
	int masterport = 8800;
	int maxpid = 2;
	const char * masterip = "127.0.0.1";
	for(int i = 1;i < argc;++i){
		if(strcmp(argv[i], "master") == 0){
			MasterNetwork master(masterip, masterport, maxpid, 60);
		}
		else{
			runtestworker(masterip, masterport, maxpid);
		}
	}
	return 0;
}
//int comm_wait(comm_request *rq, comm_status* status);
//int comm_isend(void *buf, int count, int dest, int tag, comm_request *handle);
//int comm_recv(void *buf, int count, int src, int tag, comm_status *status);
