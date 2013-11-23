#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include "sock.h"
#include "config.h"


extern struct config_t config;

extern void switchMemory(int *currentMemory, void **writeMemory, void *memory1,
		    void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr);

extern void SwitchMemory(int *currentMemory, void **writeMemory, void *memory1,
		    void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr);


extern char *fileBuf;
extern struct resources *res;



void SendBuf(void *memory1, void *memory2, int bufSize, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr) {
	long int remainMemory = bufSize;
	int currentMemory = -1;	//标志当前内存
	void *p = memory1;
	pthread_mutex_lock(m1lockr);
	while (1) {
		memcpy(fileBuf, p, bufSize);
		if (sock_sync_ready(res->sock, !config.server_name)) { printf("sync failed\n"); resources_destroy(res); exit(1); }
		if (sock_sync_ready(res->sock, !config.server_name)) { printf("sync failed 2\n"); resources_destroy(res); exit(1); }
		if ((*(int *)p) == 1) break;
		switchMemory(&currentMemory, &p, memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
		p -= sizeof(int);
	}
	if (currentMemory == -1) { pthread_mutex_unlock(m1lockw);  pthread_mutex_unlock(m1lockr); }
	else { pthread_mutex_unlock(m2lockw); pthread_mutex_unlock(m2lockr); }
}


void RecvBuf(struct fileStruct *list, long bufSize, void *memory1, void *memory2, pthread_mutex_t *m1lockw, pthread_mutex_t *m1lockr, pthread_mutex_t *m2lockw, pthread_mutex_t *m2lockr) {
	int currentMemory = -1;
	void *writeMemory = memory1;
	while (1) {
		if (sock_sync_ready(res->sock, !config.server_name)) { printf("sync failed\n"); resources_destroy(res); exit(1); }
		char *buf = recieve_ib(res, bufSize);
		memcpy(writeMemory, buf, bufSize);
		if (*(int*)writeMemory == 1)
			break;
		else {
			SwitchMemory(&currentMemory, &writeMemory, memory1, memory2, m1lockw, m1lockr, m2lockw, m2lockr);
			writeMemory -= sizeof(int);
		}
	}
	if (currentMemory == -1) { pthread_mutex_unlock(m1lockr); }
	else { pthread_mutex_unlock(m2lockr); }
}

