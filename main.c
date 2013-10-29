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

#define DEFAULT_PORT 19875

extern struct added_config_t added_config ;
extern struct config_t config;
pthread_mutex_t m1lockw, m1lockr, m2lockw, m2lockr;
pthread_t tRead, tWrite;
int is_client = 0;
struct resources *res;
void *fileBuf;
int verbose_mode = 0;

extern struct f_linkedlist* head_node;


void ReadFileWrapper(void *vargs) {
	unsigned long long *args = vargs;
	ReadFile(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
}

void RecvBufWrapper(void *vargs) {
	unsigned long long *args = vargs;
	RecvBuf(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
	
}

void WriteFileWrapper(void *vargs) {
	unsigned long long *args = vargs;
	WriteFile(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
}


void SendBufWrapper(void *vargs) {
	unsigned long long *args = vargs;
	SendBuf(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
}

int main(int argc, char *argv[])
{
	srand(time(0));
	int nprocs = 1;
	config.dev_name=strdup("mlx4_0");
	config.tcp_port=19879;
	config.ib_port=1;
	while (1) {
		int c;
		static struct option long_options[] = {
			{.name = "port",.has_arg = 1,.val = 'p'},
			{.name = "ib-dev",.has_arg = 1,.val = 'd'},
			{.name = "windowsize",.has_arg = 1,.val = 'w'},
			{.name = "file",.has_arg = 1,.val = 'f'},
			{.name = "list",.has_arg = 1,.val = 'l'},
			{.name = "reserve",.has_arg = 0,.val = 'r'},
			{.name = "cover",.has_arg = 0,.val = 'c'},
			{.name = "verbose",.has_arg = 0,.val = 'v'},
			{.name = "ib-port",.has_arg = 1,.val = 'i'},
			{.name = "server", .has_arg = 1, .val = 's'},
			{.name = "nprocs", .has_arg = 1, .val = 'n'},
			{.name = NULL, .has_arg = 0, .val = '\0'}
		};

		c = getopt_long(argc, argv, "p:d:w:f:l:r:cvi:s:n:", long_options,
				NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			config.tcp_port = strtoul(optarg, NULL, 0);
			break;

		case 'd':
			config.dev_name = strdup(optarg);
			break;

		case 'i':
			config.ib_port = strtoul(optarg, NULL, 0);
			if (config.ib_port < 0) {
				printf("error tcp port");
				return 1;
			}
			break;

		case 'w':
			added_config.windowsize = strtoul(optarg, NULL, 0);
			/* other error control can added here */
			break;

		case 'f':
			added_config.p_filename = strdup(optarg);
			break;

		case 'l':
			added_config.p_listname = strdup(optarg);
			break;

		case 's':
			config.server_name = strdup(optarg);
			is_client = 1;
			break;

		case 'n':
			nprocs = strtoul(optarg, NULL, 0);
			break;


		case 'r':
			added_config.p_dir=strdup(optarg);
			break;

		case 'v':
			verbose_mode = 1;
			break;
		}
	}
	pid_t server_conn_pid = 0;
	if (is_client) {
		server_conn_pid = fork();
		if (server_conn_pid == 0) {
			char cmd[256];
			sprintf(cmd, "ssh %s ibcp -n %d -w %d >/dev/null 2>/dev/null", config.server_name, nprocs, added_config.windowsize);
			system(cmd);
			exit(0);
		} else if (server_conn_pid < 0) {
			printf("Failed to fork\n");
		}
	}
	struct f_linkedlist **head_nodes = malloc(sizeof(struct f_linkedlist*) * nprocs);
	struct f_linkedlist **nows = malloc(sizeof(struct f_linkedlist*) * nprocs);
	memset(head_nodes, 0, sizeof(struct f_linkedlist*) * nprocs);
	memset(nows, 0, sizeof(struct f_linkedlist*) * nprocs);
	file_linkedlist_creat();
	/*
	struct f_linkedlist *ff = head_node;
	while (ff) {
		printf("File: %s\n", ff->f_name);
		ff = ff->next;
	}
	*/
	pid_t *pids = malloc(sizeof(pid_t) * nprocs);
	if (nprocs >= 1) {
		struct f_linkedlist *now = head_node;
		int id = 0;
		while (now != NULL) {
			int random = id % nprocs;
			id += 1;
			if (nows[random] == NULL) {
				nows[random] = now;
				head_nodes[random] = now;
			} else {
				nows[random]->next = now;
				nows[random] = now;
			}
			struct f_linkedlist *newnow = now->next;
			nows[random]->next = NULL;
			now = newnow;
		}
		int k = 0;
		/*
		for (k = 0; k < nprocs; k++) {
			printf("Process %d\n", k);
			struct f_linkedlist *f = head_nodes[k];
			while (f) {
				printf("File: %s\n", f->f_name);
				f = f->next;
			}
		}
		*/
		int child = 0;
		for (k = 0; k < nprocs; k++) {
			pid_t pid = fork();
			if (pid > 0) {
				*(pids + k) = pid;
			} else if (pid == 0) {
				child = 1;
				break;
			} else {
				printf("Failed to fork\n");
			}
		}
		if (child) {
			config.tcp_port += k;
			head_node = head_nodes[k];
			goto core;
		} else {
			for (k = 0; k < nprocs; k++) waitpid(pids[k], NULL, 0);
			printf("-------------------------------------------\nClient send data end, waiting for remote to finish writing\n--------------------------------------------\n");
			waitpid(server_conn_pid, NULL, 0);
			printf("Remote write finished\n");
			exit(0);
		}
	}
core:
	printf("IS Client: %d\n", is_client);
	long int bufSize = added_config.windowsize;
	void *buf1 = malloc(bufSize*1024);
	void *buf2 = malloc(bufSize*1024);
	pthread_mutex_init(&m1lockw, NULL);
	pthread_mutex_init(&m1lockr, NULL);
	pthread_mutex_init(&m2lockw, NULL);
	pthread_mutex_init(&m2lockr, NULL);
	printf("start to read\n");
	//ReadFile(head_node, bufSize*1024, buf1, buf2, &m1lock, &m2lock);
	fileBuf = malloc(bufSize * 1024);
	memset(fileBuf, 0, bufSize * 1024);
	int tt = 50000;
	while (tt--) {
		res = init_ib(fileBuf, bufSize * 1024);
		if (res) break;
		usleep(100);
	}
	if (!res) {
		printf("fail to init the ib");
		return 0;
	}
	printf("Connection established\n");
	unsigned long long *argsRead = malloc(sizeof(unsigned long long) * 8);
	argsRead[0] = (unsigned long long) (head_node);
	argsRead[1] = bufSize * 1024;
	argsRead[2] = buf1;
	argsRead[3] = buf2;
	argsRead[4] = &m1lockw;
	argsRead[5] = &m1lockr;
	argsRead[6] = &m2lockw;
	argsRead[7] = &m2lockr;
	pthread_mutex_lock(&m1lockw);
	pthread_mutex_lock(&m1lockr);
	if (is_client)
		pthread_create(&tRead, NULL, &ReadFileWrapper, argsRead);
	else
		pthread_create(&tRead, NULL, &RecvBufWrapper, argsRead);
	unsigned long long *argsWrite = malloc(sizeof(unsigned long long) * 7);
	argsWrite[0] = buf1;
	argsWrite[1] = buf2;
	argsWrite[2] = bufSize * 1024;
	argsWrite[3] = &m1lockw;
	argsWrite[4] = &m1lockr;
	argsWrite[5] = &m2lockw;
	argsWrite[6] = &m2lockr;
	if (is_client)
		pthread_create(&tWrite, NULL, &SendBufWrapper, argsWrite);
	else
		pthread_create(&tWrite, NULL, *WriteFileWrapper, argsWrite);
	pthread_join(tRead, NULL);
	pthread_join(tWrite, NULL);
	//WriteFile(buf1, buf2, bufSize * 1024, &m1lock, &m2lock);
	if (verbose_mode) {
		printf("-----------SUMMARY-------------------\n");
		printf("head_node\tfilename:%s\n", head_node->f_name);
		struct f_linkedlist *temp;
		temp = head_node;
		while (temp != NULL) {
			printf("name:%s\n", temp->f_name);
			printf("size:%ld\n", temp->f_size);
			printf("mode:%d\n", temp->f_mode);
			temp = temp->next;
		}
	}
	resources_destroy(res);

	return 0;
}
