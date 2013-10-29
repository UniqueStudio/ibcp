/*
 * Copyright (c) 2011 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: basic_test_flow.c 3720 2011-07-23 12:35:42Z gsivan $ 
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>
#include <byteswap.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include "sock.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
char MSG[2048] = "hehe";
/* poll CQ timeout in milisec */
#define MAX_POLL_CQ_TIMEOUT 2000
#define SEND 0
#define RECV 0
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x)
{
	return bswap_64(x);
}

static inline uint64_t ntohll(uint64_t x)
{
	return bswap_64(x);
}
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x)
{
	return x;
}

static inline uint64_t ntohll(uint64_t x)
{
	return x;
}
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

/* structure of test parameters */
struct config_t {
	const char *dev_name;	/* IB device name */
	char *server_name;	/* daemon host name */
	u_int32_t tcp_port;	/* daemon TCP port */
	int ib_port;		/* local IB port to work with */
};

/* structure to exchange data which is needed to connect the QPs */
struct cm_con_data_t {
	uint64_t addr;		/* Buffer address */
	uint32_t rkey;		/* Remote key */
	uint32_t qp_num;	/* QP number */
	uint16_t lid;		/* LID of the IB port */
} __attribute__ ((packed));

/* structure of needed test resources */
struct resources {
	struct ibv_device_attr device_attr;	/* Device attributes */
	struct ibv_port_attr port_attr;	/* IB port attributes */
	struct cm_con_data_t remote_props;	/* values to connect to remote side */
	struct ibv_device **dev_list;	/* device list */
	struct ibv_context *ib_ctx;	/* device handle */
	struct ibv_pd *pd;	/* PD handle */
	struct ibv_cq *cq;	/* CQ handle */
	struct ibv_qp *qp;	/* QP handle */
	struct ibv_mr *mr;	/* MR handle */
	char *buf;		/* memory buffer pointer */
	int sock;		/* TCP socket file descriptor */
};

struct config_t config = {
	"mlx4_0",		/* dev_name */
	NULL,			/* server_name */
	19875,			/* tcp_port */
	1			/* ib_port */
};

/*****************************************
* Function: poll_completion
*****************************************/
static int poll_completion(struct resources *res)
{
	struct ibv_wc wc;
	unsigned long start_time_msec, cur_time_msec;
	struct timeval cur_time;
	int rc;

	/* poll the completion for a while before giving up of doing it .. */
	gettimeofday(&cur_time, NULL);
	start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);

	do {
		rc = ibv_poll_cq(res->cq, 1, &wc);
		if (rc < 0) {
			fprintf(stderr, "poll CQ failed\n");
			return 1;
		}
		gettimeofday(&cur_time, NULL);
		cur_time_msec =
		    (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
	}
	while ((rc == 0)
	       && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));

	/* if the CQ is empty */
	if (rc == 0) {
		fprintf(stderr,
			"completion wasn't found in the CQ after timeout\n");
		return 1;
	}

	fprintf(stdout, "completion was found in CQ with status 0x%x\n",
		wc.status);

	/* check the completion status (here we don't care about the completion opcode */
	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr,
			"got bad completion with status: 0x%x, vendor syndrome: 0x%x\n",
			wc.status, wc.vendor_err);
		return 1;
	}

	return 0;
}

/*****************************************
* Function: post_send
*****************************************/
static int post_send(struct resources *res,int MSG_SIZE)
{
	struct ibv_send_wr sr;
	struct ibv_sge sge;
	struct ibv_send_wr *bad_wr;
	int rc;

	/* prepare the scatter/gather entry */
	memset(&sge, 0, sizeof(sge));

	sge.addr = (uintptr_t) res->buf;
	sge.length = MSG_SIZE;
	sge.lkey = res->mr->lkey;

	/* prepare the SR */
	memset(&sr, 0, sizeof(sr));

	sr.next = NULL;
	sr.wr_id = 0;
	sr.sg_list = &sge;
	sr.num_sge = 1;
	sr.opcode = IBV_WR_RDMA_READ;
	sr.send_flags = IBV_SEND_SIGNALED;

	sr.wr.rdma.remote_addr = res->remote_props.addr;
	sr.wr.rdma.rkey = res->remote_props.rkey;

	/* there is a Receive Request in the responder side, so we won't get any into RNR flow */
	rc = ibv_post_send(res->qp, &sr, &bad_wr);
	if (rc) {
		fprintf(stderr, "failed to post SR\n");
		return 1;
	}
	fprintf(stdout, "Send Request was posted\n");

	return 0;
}

/*****************************************
* Function: resources_init
*****************************************/
static void resources_init(struct resources *res)
{
	res->dev_list = NULL;
	res->ib_ctx = NULL;
	res->cq = NULL;
	res->qp = NULL;
	res->pd = NULL;
	res->mr = NULL;
	res->buf = NULL;
	res->sock = -1;
}

/*****************************************
* Function: resources_create
*****************************************/
static int resources_create(struct resources *res,void* fileBuf,int MSG_SIZE)
{
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_device *ib_dev = NULL;
	size_t size;
	int i;
	int mr_flags = 0;
	int cq_size = 0;
	int num_devices;

	/* if client side */
	//printf("the server name is:%s\n", config.server_name);
	if (config.server_name) {
		res->sock =
		    sock_client_connect(config.server_name, config.tcp_port);
		if (res->sock < 0) {
	//		fprintf(stderr,
//				"failed to establish TCP connection to server %s, port %d\n",
//				config.server_name, config.tcp_port);
			return -1;
		}
	} else {
//		fprintf(stdout, "waiting on port %d for TCP connection\n",
//			config.tcp_port);

		res->sock = sock_daemon_connect(config.tcp_port);
		if (res->sock < 0) {
//			fprintf(stderr,
//				"failed to establish TCP connection with client on port %d\n",
//				config.tcp_port);
			return -1;
		}
	}

	fprintf(stdout, "TCP connection was established\n");

	fprintf(stdout, "searching for IB devices in host\n");

	/* get device names in the system */
	res->dev_list = ibv_get_device_list(&num_devices);
	if (!res->dev_list) {
		fprintf(stderr, "failed to get IB devices list\n");
		return 1;
	}

	/* if there isn't any IB device in host */
	if (!num_devices) {
		fprintf(stderr, "found %d device(s)\n", num_devices);
		return 1;
	}

	fprintf(stdout, "found %d device(s)\n", num_devices);

	/* search for the specific device we want to work with */
	for (i = 0; i < num_devices; i++) {
		if (!strcmp
		    (ibv_get_device_name(res->dev_list[i]), config.dev_name)) {
			ib_dev = res->dev_list[i];
			break;
		}
	}

	/* if the device wasn't found in host */
	if (!ib_dev) {
		fprintf(stderr, "IB device %s wasn't found\n", config.dev_name);
		return 1;
	}

	/* get device handle */
	res->ib_ctx = ibv_open_device(ib_dev);
	if (!res->ib_ctx) {
		fprintf(stderr, "failed to open device %s\n", config.dev_name);
		return 1;
	}

	/* query port properties  */
	if (ibv_query_port(res->ib_ctx, config.ib_port, &res->port_attr)) {
		fprintf(stderr, "ibv_query_port on port %u failed\n",
			config.ib_port);
		return 1;
	}

	/* allocate Protection Domain */
	res->pd = ibv_alloc_pd(res->ib_ctx);
	if (!res->pd) {
		fprintf(stderr, "ibv_alloc_pd failed\n");
		return 1;
	}

	/* each side will send only one WR, so Completion Queue with 1 entry is enough */
	cq_size = 1;
	res->cq = ibv_create_cq(res->ib_ctx, cq_size, NULL, NULL, 0);
	if (!res->cq) {
		fprintf(stderr, "failed to create CQ with %u entries\n",
			cq_size);
		return 1;
	}

	/* allocate the memory buffer that will hold the data */
	size = MSG_SIZE;
	res->buf = fileBuf;
	if (!res->buf) {
		fprintf(stderr, "failed to malloc %Zu bytes to memory buffer\n",
			size);
		return 1;
	}

	/* only in the client side put the message in the memory buffer */
	/*if (config.server_name) {
		strcpy(res->buf, MSG);
		fprintf(stdout, "going to send the message: '%s'\n", res->buf);
	} else
	*///	memset(res->buf, 0, size);

	/* register this memory buffer */
	/* only the client expect to get incoming RDMA Read operation */
	mr_flags =
	    (config.
	     server_name) ? IBV_ACCESS_REMOTE_READ : IBV_ACCESS_LOCAL_WRITE;

	res->mr = ibv_reg_mr(res->pd, res->buf, size, mr_flags);
	if (!res->mr) {
		fprintf(stderr, "ibv_reg_mr failed with mr_flags=0x%x\n",
			mr_flags);
		return 1;
	}

	fprintf(stdout,
		"MR was registered with addr=%p, lkey=0x%x, rkey=0x%x, flags=0x%x\n",
		res->buf, res->mr->lkey, res->mr->rkey, mr_flags);

	/* create the Queue Pair */
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));

	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.sq_sig_all = 1;
	qp_init_attr.send_cq = res->cq;
	qp_init_attr.recv_cq = res->cq;
	qp_init_attr.cap.max_send_wr = 1;
	qp_init_attr.cap.max_recv_wr = 1;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 1;

	res->qp = ibv_create_qp(res->pd, &qp_init_attr);
	if (!res->qp) {
		fprintf(stderr, "failed to create QP\n");
		return 1;
	}
	fprintf(stdout, "QP was created, QP number=0x%x\n", res->qp->qp_num);

	return 0;
}

/*****************************************
* Function: modify_qp_to_init
*****************************************/
static int modify_qp_to_init(struct ibv_qp *qp)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	/* do the following QP transition: RESET -> INIT */
	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_INIT;
	attr.port_num = config.ib_port;
	attr.pkey_index = 0;
	/* only the client expects to get incoming RDMA READ opertaion */
	attr.qp_access_flags =
	    (config.server_name) ? IBV_ACCESS_REMOTE_READ : 0;

	flags =
	    IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
	    IBV_QP_ACCESS_FLAGS;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc) {
		fprintf(stderr, "failed to modify QP state to INIT\n");
		return rc;
	}

	return 0;
}

/*****************************************
* Function: modify_qp_to_rtr
*****************************************/
static int modify_qp_to_rtr(struct ibv_qp *qp,
			    uint32_t remote_qpn, uint16_t dlid)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	/* do the following QP transition: INIT -> RTR */
	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_256;
	attr.dest_qp_num = remote_qpn;
	attr.rq_psn = 0;
	/* the client need to be responder to incoming RDMA Read */
	attr.max_dest_rd_atomic = (config.server_name) ? 1 : 0;
	attr.min_rnr_timer = 0x12;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.dlid = dlid;
	attr.ah_attr.sl = 0;
	attr.ah_attr.src_path_bits = 0;
	attr.ah_attr.port_num = config.ib_port;

	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
	    IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc) {
		fprintf(stderr, "failed to modify QP state to RTR\n");
		return rc;
	}

	return 0;
}

/*****************************************
* Function: modify_qp_to_rts
*****************************************/
static int modify_qp_to_rts(struct ibv_qp *qp)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;

	/* do the following QP transition: RTR -> RTS */
	memset(&attr, 0, sizeof(attr));

	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 0x12;
	attr.retry_cnt = 6;
	attr.rnr_retry = 0;
	attr.sq_psn = 0;
	/* the daemon need to be initiator of incoming RDMA Read */
	attr.max_rd_atomic = (!config.server_name) ? 1 : 0;

	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
	    IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;

	rc = ibv_modify_qp(qp, &attr, flags);
	if (rc) {
		fprintf(stderr, "failed to modify QP state to RTS\n");
		return rc;
	}

	return 0;
}

/*****************************************
* Function: connect_qp
*****************************************/
static int connect_qp(struct resources *res)
{
	struct cm_con_data_t local_con_data, remote_con_data, tmp_con_data;
	int rc;

	/* modify the QP to init */
	rc = modify_qp_to_init(res->qp);
	if (rc) {
		fprintf(stderr, "change QP state to INIT failed\n");
		return rc;
	}

	/* exchange using TCP sockets info required to connect QPs */
	local_con_data.addr = htonll((uintptr_t) res->buf);
	local_con_data.rkey = htonl(res->mr->rkey);
	local_con_data.qp_num = htonl(res->qp->qp_num);
	local_con_data.lid = htons(res->port_attr.lid);

	fprintf(stdout, "\nLocal LID        = 0x%x\n", res->port_attr.lid);

	if (sock_sync_data
	    (res->sock, !config.server_name, sizeof(struct cm_con_data_t),
	     &local_con_data, &tmp_con_data) < 0) {
		fprintf(stderr,
			"failed to exchange connection data between sides\n");
		return 1;
	}

	remote_con_data.addr = ntohll(tmp_con_data.addr);
	remote_con_data.rkey = ntohl(tmp_con_data.rkey);
	remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
	remote_con_data.lid = ntohs(tmp_con_data.lid);

	/* save the remote side attributes, we will need it for the post SR */
	res->remote_props = remote_con_data;

	fprintf(stdout, "Remote address   = 0x%" PRIx64 "\n",
		remote_con_data.addr);
	fprintf(stdout, "Remote rkey      = 0x%x\n", remote_con_data.rkey);
	fprintf(stdout, "Remote QP number = 0x%x\n", remote_con_data.qp_num);
	fprintf(stdout, "Remote LID       = 0x%x\n", remote_con_data.lid);

	/* modify the QP to RTR */
	rc = modify_qp_to_rtr(res->qp, remote_con_data.qp_num,
			      remote_con_data.lid);
	if (rc) {
		fprintf(stderr,
			"failed to modify QP state from RESET to RTS\n");
		return rc;
	}

	/* only the daemon post SR, so only he should be in RTS
	   (the client can be moved to RTS as well)
	 */
	if (config.server_name)
		fprintf(stdout, "QP state was change to RTR\n");
	else {
		rc = modify_qp_to_rts(res->qp);
		if (rc) {
			fprintf(stderr,
				"failed to modify QP state from RESET to RTS\n");
			return rc;
		}

		fprintf(stdout, "QP state was change to RTS\n");
	}

	/* sync to make sure that both sides are in states that they can connect to prevent packet loose */
	if (sock_sync_ready(res->sock, !config.server_name)) {
		fprintf(stderr, "sync after QPs are were moved to RTS\n");
		return 1;
	}

	return 0;
}

/*****************************************
* Function: resources_destroy
*****************************************/
int resources_destroy(struct resources *res)
{
	int test_result = 0;

	if (res->qp) {
		if (ibv_destroy_qp(res->qp)) {
			fprintf(stderr, "failed to destroy QP\n");
			test_result = 1;
		}
	}

	if (res->mr) {
		if (ibv_dereg_mr(res->mr)) {
			fprintf(stderr, "failed to deregister MR\n");
			test_result = 1;
		}
	}

	if (res->buf)
		free(res->buf);

	if (res->cq) {
		if (ibv_destroy_cq(res->cq)) {
			fprintf(stderr, "failed to destroy CQ\n");
			test_result = 1;
		}
	}

	if (res->pd) {
		if (ibv_dealloc_pd(res->pd)) {
			fprintf(stderr, "failed to deallocate PD\n");
			test_result = 1;
		}
	}

	if (res->ib_ctx) {
		if (ibv_close_device(res->ib_ctx)) {
			fprintf(stderr, "failed to close device context\n");
			test_result = 1;
		}
	}

	if (res->dev_list)
		ibv_free_device_list(res->dev_list);

	if (res->sock >= 0) {
		if (close(res->sock)) {
			fprintf(stderr, "failed to close socket\n");
			test_result = 1;
		}
	}
	return test_result;

}

/*
int cleanup_ib(res)
{
	resources_destroy(res);
	printf("will exit with eroor");
	exit(1);
}*/
/*****************************************
* Function: print_config
*****************************************/
static void print_config(void)
{
	fprintf(stdout, " ------------------------------------------------\n");
	fprintf(stdout, " Device name                  : \"%s\"\n",
		config.dev_name);
	fprintf(stdout, " IB port                      : %u\n", config.ib_port);
	if (config.server_name)
		fprintf(stdout, " IP                           : %s\n",
			config.server_name);
	fprintf(stdout, " TCP port                     : %u\n",
		config.tcp_port);
	fprintf(stdout,
		" ------------------------------------------------\n\n");
}

/*****************************************
* Function: usage
*****************************************/
/*static void usage(const char *argv0)
{
	fprintf(stdout, "Usage:\n");
	fprintf(stdout, "  %s            start a server and wait for connection\n", argv0);
	fprintf(stdout, "  %s <host>     connect to server at <host>\n", argv0);
	fprintf(stdout, "\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	fprintf(stdout, "  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	fprintf(stdout, "  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
}
*/
/*
int clean_ib(res){
	int test_result=0;
	if (resources_destroyv(res)) {
		fprintf(stderr, "failed to destroy resources\n");
		test_result = 1;
	}

	fprintf(stdout, "\ntest status is %d\n", test_result);
	return test_result;
}
*/
/*****************************************
******************************************
* Function: init_ib
******************************************
*****************************************/
struct resources *init_ib(void *fileBuf,int MSG_SIZE)
{
	struct resources *res =
	    (struct resources *)malloc(sizeof(struct resources));
	/* print the used parameters for info */
	//print_config();

	/* init all of the resources, so cleanup will be easy */
	resources_init(res);

	/* create resources before using them */
	if (resources_create(res,fileBuf,MSG_SIZE)) {
		//fprintf(stderr, "failed to create resources\n");
		resources_destroy(res);
		return NULL;
	}

	/* connect the QPs */
	if (connect_qp(res)) {
		fprintf(stderr, "failed to connect QPs\n");
		resources_destroy(res);
		return NULL;
	}
	return res;
	/* after polling the completion we have the message in the daemon buffer too */
}

void *recieve_ib(struct resources *res,int MSG_SIZE)
{
	/* let the daemon post the SR */
	if (post_send(res,MSG_SIZE)) {
		fprintf(stderr, "failed to post SR\n");
		resources_destroy(res);

	}

	/* we expect to get completion only in the daemon */
	if (poll_completion(res)) {
		fprintf(stderr, "poll completion failed\n");
		resources_destroy(res);

	}

	printf("filebuf:%s", res->buf);
	/* sync to make sure that:
	   the client won't close the resources before the daemon read the data */
	if (sock_sync_ready(res->sock, !config.server_name)) {
		fprintf(stderr, "sync before end of test\n");
		resources_destroy(res);

	}

	return res->buf;
}

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

/*
int main(int argc, char *argv[])
{
	int isclient = 0;
	if (argc > 1) {
		printf("client\n");
		isclient = 1;
		config.server_name = strdup(argv[1]);
	}
	int MSG_SIZE=1024;
	void *fileBuf=(void*)malloc(MSG_SIZE);
	if(isclient){
		char a[]="hebin";
		memcpy(fileBuf,a,strlen(a)+1);
		sleep(1);
	}
	struct resources *res = init_ib(fileBuf,MSG_SIZE);
	if (!res) {
		printf("fail to init the ib");
		return 0;
	}

	if (!isclient) {
		char *filebuf = NULL;
		filebuf = recieve_ib(res,MSG_SIZE);
		if (!filebuf) {
			printf("filebuf is NULL");
		} else
			printf("recieve:%s", res->buf);
	}
	if (isclient) {
		if (sock_sync_ready(res->sock, !config.server_name)) {
			fprintf(stderr, "sync before end of test\n");
			resources_destroy(res);
		}
	}


	if(isclient){
		char a[]="hehe";
		memcpy(fileBuf,a,strlen(a)+1);
		sleep(1);
	}
	sleep(1);
	if (!isclient) {
		char *filebuf = NULL;
		filebuf = recieve_ib(res,MSG_SIZE);
		if (!filebuf) {
			printf("filebuf is NULL");
		} else
			printf("recieve:%s", res->buf);
	}
	if (isclient) {
		if (sock_sync_ready(res->sock, !config.server_name)) {
			fprintf(stderr, "sync before end of test\n");
			resources_destroy(res);
		}
	}


	resources_destroy(res);
	return 0;
}
*/
