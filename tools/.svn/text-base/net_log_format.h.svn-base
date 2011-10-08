struct net_log_msg {
	int node;			/* sending node's IP address */
};

enum net_log_record {
        NET_LOG_RECORD_EVENT,		/* something happened */
	NET_LOG_RECORD_REGISTRY,	/* symbol logging registry */
	NET_LOG_RECORD_DATASET,		/* symbol logging dataset */
	NET_LOG_RECORD_ID,		/* ID and pad */
	NET_LOG_RECORD_TIMESTAMP,	
	NET_LOG_RECORD_COMMENT,	
	NET_LOG_RECORD_PROC_ENTRY,	/* procedure entry */
	NET_LOG_RECORD_PROC_EXIT	/* procedure exit */
};
#define NET_LOG_NUM_TYPES NET_LOG_RECORD_PROC_EXIT+1

#define NET_LOG_SENTINEL 0xcafe

/*
 * The netlog module guarantees that no record can exceed NET_LOG_MAX_RECORD bytes.
 */
#define NET_LOG_MAX_RECORD_SIZE 16*1024

struct net_log_header {
	unsigned short sentinel; /* should match NET_LOG_SENTINEL */
	enum net_log_record type;/* record type */
	int length;		/* total number of bytes in this record */
	uint64_t pcc;     	/* fine-grained timestamp (e.g., CPU cycle counter) */
	uint64_t pclock;	/* coarse timestamp */
	/* length-sizeof(net_log_header) bytes of type-specific data goes here */
};

/* 
 * These should show up once every four minutes (256 seconds) by default.
 */
struct net_log_timestamp_record {	/* NET_LOG_RECORD_TIMESTAMP */
	struct net_log_header head;
	int hz;			/* fine-grained clock ticks/sec (e.g., CPU cycle hz) */
        int pclock_interval;	/* pclock granularity in microseconds */
	/* the following fields are a struct timeval */
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

struct net_log_event_record {	/* NET_LOG_RECORD_EVENT */
	struct net_log_header head;
	unsigned long lclock;	/* logical clock */
	int	type;		/* event type */
	int	node;  		/* peer node (sender or receiver) */
	int	arg;		/* event-dependent arg (I/O: size) */
	unsigned int vid;       /* I/O: volume ID (dev or fsid) */
  	unsigned int obj;	/* I/O: object (fileid or VM obj ID) */
	unsigned int pgoff;	/* I/O: page or block offset */
};

struct net_log_proc_record {	/* NET_LOG_RECORD_PROC_ENTRY and PROC_EXIT */
	struct net_log_header head;
	long	paddr;		/* procedure address */
};

struct net_log_id_record {	/* NET_LOG_RECORD_ID */
	struct net_log_header head;
	unsigned int lsn;	/* log sequence number */
	int closing;		/* 1 <==> last page of log */
	int ipaddr;		/* sending IP address */
};


/*
 * event log subtypes
 */
#define IOLOG_UNUSED		0
#define IOLOG_BIOREAD		1
#define IOLOG_BIOWRITE		2
#define IOLOG_BIOPREF		3
#define IOLOG_BIOHIT		4
#define IOLOG_BIOHIT_VMIO	5
#define IOLOG_BIOREADSTALL	6
#define IOLOG_BIOWRITESTALL	7
#define IOLOG_BIODONE		8
#define IOLOG_BIODIRTY		9
#define IOLOG_BIOBUFWAIT	10
#define IOLOG_BIOBUFWAKE	11
#define IOLOG_BIOKVAFREE	12
#define IOLOG_BIO_RELVMIO	13
#define IOLOG_BIOVISIT		14
#define IOLOG_BIODAEMON_OFF	15
#define IOLOG_BIODAEMON_ON	16
#define IOLOG_DISKREAD		17
#define IOLOG_DISKWRITE		18
#define IOLOG_PAGEDAEMON_START	19
#define IOLOG_PAGEDAEMON_STOP	20
#define IOLOG_METAOP            21
#define IOLOG_METAOP_DONE       22
/*
 * IP send of vm/fs block, completion, and mbuf release.
 */
#define IOLOG_IPSEND            23
#define IOLOG_IPSEND_COMPLETE   24
#define IOLOG_IP_RELEASE        25
/*
 * VM stuff
 */
#define IOLOG_WIRE	        26
#define IOLOG_UNWIRE	        27
#define IOLOG_ACTIVATE		28
#define IOLOG_DEACTIVATE	29
#define IOLOG_CLEAN		30
#define IOLOG_ALLOC		31
#define IOLOG_FREE		32
/* 
 * Anypoint stuff
 */
#define IOLOG_ACP_CMD_BEGIN     33
#define IOLOG_ACP_CMD_END       34


#define IOLOG_NUM_TYPES		35
#define NET_LOG_NUM_EVENT_TYPES	IOLOG_NUM_TYPES

/* see net_log_table.h */
struct net_log_event_entry {
	char* name;
	char* sname;
	char* expl;
	long enabled;
	int complete;
};
extern struct net_log_event_entry net_log_event_table[];

/* XXX Must touch duplicate definition in net_log_stub.c if you change this.*/

/*
 * A temporary hardwired port in case we're using TCP.
 */
#define NET_LOG_PORT 2827

