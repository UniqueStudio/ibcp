#include <infiniband/verbs.h>

struct f_linkedlist{
    char f_name[256];
    long int f_size;
    int f_mode;
    struct f_linkedlist *next;
};

struct added_config_t {
    int windowsize;
    const char *p_filename;
    const char *p_dir;
    const char *p_listname;
    int reserve_flag;
    int cover_flag;
    int verbose_flag;
};


struct config_t {
	const char			*dev_name;	/* IB device name */
	char				*server_name;	/* daemon host name */
	int			tcp_port;	/* daemon TCP port */
	int				ib_port;	/* local IB port to work with */
};



struct cm_con_data_t {
	uint64_t addr;		/* Buffer address */
	uint32_t rkey;		/* Remote key */
	uint32_t qp_num;	/* QP number */
	uint16_t lid;		/* LID of the IB port */
} __attribute__ ((packed));

/* structure of needed test resources */

/*void ReadFile(struct f_linkedlist  * list, long bufSize, void *memory1, void *memory2);

void WriteFile(void *memory1, void *memory2, int bufSize); */


struct resources {
	struct ibv_device_attr		device_attr;	/* Device attributes */
	struct ibv_port_attr		port_attr;	/* IB port attributes */
	struct cm_con_data_t		remote_props;	/* values to connect to remote side */
	struct ibv_device		**dev_list;	/* device list */
	struct ibv_context		*ib_ctx;	/* device handle */
	struct ibv_pd			*pd;		/* PD handle */
	struct ibv_cq			*cq;		/* CQ handle */
	struct ibv_qp			*qp;		/* QP handle */
	struct ibv_mr			*mr;		/* MR handle */
	char				*buf;		/* memory buffer pointer */
	int				sock;		/* TCP socket file descriptor */
};
