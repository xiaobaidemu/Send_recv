/*************************************************************************
 > File Name: check.h
 > Author: He Boxin
 > Mail: heboxin@pku.edu.cn
 > Created Time: 2017年11月24日 星期五 16时39分42秒
 ************************************************************************/
#ifndef _CHECK_H
#define _CHECK_H
#include <errno.h>
#include<sys/time.h>
#include<string.h>
#define errmsg(format...) do { \
    const char* msg = sys_errlist[errno]; \
    printf("[FAIL] %s:%d (errno=%d): %s -- ", __FILE__, __LINE__, errno, msg); \
    printf(format); \
} while (0)

#define safecall(call) do { \
    if ((call) == -1) { \
        errmsg("safecall fail\n"); \
        /*abort();*/ \
    } \
} while (0)

#define assertcall(call, rrcc) do { \
    int rc = (int)(call); \
    if (rc != (rrcc)) { \
        errmsg("safecall return %d\n", rc); \
        /*abort();*/ \
    } \
} while (0)

void error_check(int val,  char *str){
	if (val < 0){
		printf("%s :%d: %s error\n", str, val, strerror(errno));
		exit(1);
	}
}

double now(void) {
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}

#endif
