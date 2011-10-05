/* 
 * modelnet  ip_modelnet.h
 *
 *     modelnet emulation pkts between subnet 10.0.0.0/8 addrs
 * 
 * Copyright (c) 2006
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the names of Duke University nor The University of
 *      California, San Diego, nor the names of the authors or contributors
 *      may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef _IP_MODELNET_H
#define _IP_MODELNET_H

#include <linux/spinlock.h>

#define MODEL_SUBNET htonl(0x0a000000)  /* 10.0.0.0/8 */
#define MODEL_MASK   htonl(0xff000000)  /* 10.0.0.0/8 */
#define MODEL_FORCEBIT htonl(0x00800000)
#define MODEL_PORT 5347         /* udp port for remote hop service */
#define MN_MAX_CORES 32         /* pcache+aggregated xcore traffic */
#define MN_MTU  1500            /* something reasonable */
#define SCHEDBITS 14                /* power of two size of sched array */
#define SCHEDLEN  (1<<SCHEDBITS)    /* number of slots in scheduling array */
#define SCHEDMASK (SCHEDLEN-1)  


/* Addition for Linux port.  Could not find this typedef in linux
 * code, so I just added it.  There is a similar typedef in bsd code :
     src/sys/types.h:typedef   u_int32_t       in_addr_t;
*/   
typedef u_int32_t in_addr_t;


/* Define whether or not to use tcpdump code */

/* compile-time options */
#define UNIFIED_PKT_SCHEDULE    /* queue bandwidth and delay together */

/* XXX I cheaped out and packed the structs so 64-bit systems will work */

struct sysctl_hop {
    int             bandwidth;  /* bits/s */
    int             delay;      /* ms */
    int             plr;        /* pkt loss rate (2^31-1 means 100%) */
    int             qsize;      /* queue size in slots */
    in_addr_t       emulator;   /* ip of emulator hosting hop, or 0 */
    int             xtq_type;      /* fifo, red, xcp */
  
#ifdef MN_TCPDUMP
  int             traceLink;
#endif
} __attribute__((packed));


struct sysctl_hopstats {
    int             pkts,
                    bytes,
                    qdrops;     /* stats */
} __attribute__((packed));

struct sysctl_hopmod {
    int             hopidx;
    struct sysctl_hop hop;
} __attribute__((packed));

struct sysctl_hoptable {
    struct sysctl_hop *hops;
    int             hopcount;
} __attribute__((packed));

struct sysctl_pathentry {
    int             src_node;
    int             dst_node;
    int            *hops;
    int             pathlen;
} __attribute__((packed));


/* #ifdef _KERNEL */



/*
 * We exchange remote_packet structs between cores.
 */
struct remote_packet {
    unsigned long   id;         /* opaque handle to orig. packet */
    unsigned long   expire;     /* expiration time (in ticks) */
    unsigned long   len;        /* length of packet (bytes) */
    unsigned long   wait_time;  /* total accumulated wait time for pkt */
    in_addr_t       src;
    in_addr_t       dst;
    unsigned long   hop;        /* not real source route */

    signed char     ttl;        /* time-to-live in multi-core system */

	/* ------ XCP pkt hdr information ------ */

/*     u_int  xcp_cwnd;            /\* current window *\/ */
/*     int    xcp_src_fdbk;        /\* network router feedback  *\/ */
/*     int    xcp_rflct_fdbk;      /\* bounced from rcvr *\/ */
/*     u_int  xcp_rtt;             /\* round trip time sample *\/ */

};


/* /\* */
/*  * One of these per active core destination  */
/*  *\/ */
/* struct remote_aggregate {    */
/* 	struct mbuf *m;    /\* current pending packet *\/ */
/* 	u_int32_t id;      /\* ip address of core *\/ */
/* 	u_int32_t state;   */
/* #define MN_AGG_FULL  0x1 */
/* #define MN_AGG_FREE  0x2 */
/* #define MN_AGG_BUSY  0x4 */
/* 	u_int32_t num_digests;  /\* number of digests assembled *\/ */
/* 	int bytes_left;  /\* target size -- *\/ */
/* 	TAILQ_ENTRY(remote_aggregate) list,free; */
/* }; */


/* typedef TAILQ_HEAD(mn_pending_head, remote_aggregate) *mn_pending_head_t; */



typedef struct packet {
    struct hop    **path;       /* source route of hops packet traverses */
/*     struct mbuf    *m;          /\* pkt payload *\/ */
  struct sk_buff  *skb;
    in_addr_t       cachehost;  /* address of core caching the packet or 0 */
  
  /* BSD data structure */
  /* TAILQ_ENTRY(packet) list; */
    struct list_head list; 
  
/* Packet State */
#define Q_BW       0x1          /* queue for bandwidth delay */
#define Q_DELAY    0x2          /* queue for propagation delay */
    int             state;
    struct remote_packet info;  /* state passed to remote cores */
} *mn_pkt_t;

/* B/c of how linux kernel lists work, the packet data object is both
 * a packet and a packet list
 */

typedef struct packet pktlist;
/* BSD data structure */
/*TAILQ_HEAD(pktlist, packet);*/
/* struct pktlist { */
/*   struct packet *head; */
/* }; */

/* #define INSERT_TAIL(list, node) if (list.head == NULL) list.head = node \ */
/* else list_add_tail(node, list.head) */

/* struct extended_q { */
/* 	int	type;		/\* socket type used for *\/ */
/* 	char    *name;          /\* extended queue name *\/ */
/* 	short	flags;		/\* see below *\/ */
/*     /\*------ Xtended per-hop processing ---- *\/	 */
/* 	int (*alloc)    (struct hop *h); */
/* 	void (*free)     (struct hop *h); */
/* 	void (*enqueue) (struct hop *h, mn_pkt_t pkt); */
/* 	void (*dequeue) (struct hop *h, mn_pkt_t pkt); */
/* 	void *xt_hop; */
/*     /\*------ Xtended pipe definition ---- *\/	 */
/* }; */



struct hop {
    spinlock_t      lock;
    int             KBps;       /* kilobytes/s */
    int             delay;      /* ms */
    int             plr;        /* pkt loss rate (2^31-1 means 100% loss) */
    int             qsize;      /* queue size in slots */
    in_addr_t       emulator;   /* ip of remote emulator, or 0 */

#ifdef MN_TCPDUMP
  int             traceLink;    /* Do we do a tcpdump on this link */
#endif

    u_int           bytespertick;  /* only a hint. it is 0 for slow links */
    int             id;
	/* --- Standard FIFO queue --- */
    int             slotdepth;  /* slots currently occupied by packets */
    int             bytedepth;  /* total bytes queued */
    int             headslot;   /* next slot to dequeue */
    int             fragment;   /* available bytes in tick next pkt can use */
    unsigned long  *exittick;   /* array holding the time each queued
                                 * packet will exit the queue
				 */
    int            *slotlen;    /* array holding length of each queued
				     * packet */

    int             pkts,
                    bytes,
                    qdrops;     /* stats */

    /* --- Xtended queueing disciplines --- */
  /* struct extended_q    xtq; */

};



typedef struct hop_scheduler {
	void (*timeout) (struct hop *h);  /* call this */
	struct hop *hop;                  /* with this */
  /* TAILQ_ENTRY(hop_scheduler) list,free; */
  struct list_head list;
} *hop_scheduler_t;

/* typedef TAILQ_HEAD(hoplist, hop_scheduler) *hoplist_t; */



/*
 * We keep some historical statistics in the kernel.
 * XXX not all of these are used now.
 */
struct mn_stats {
    u_int64_t       time;       /* copy of curr_time */

    unsigned int    pkts_seen;  /* total packets seen by modelnet */
    unsigned int    pkts_accepted;      /* total packets accepted */
    unsigned int    pkts_refused;       /* total packets dropped
                                         * immediately */

    unsigned int    pkt_sends;  /* accepted pkts sent */
    unsigned int    pkt_hops;   /* accepted pkts total hop count */
    unsigned int    pkt_plrdrops;       /* accepted pkts dropped due to
                                         * plr */
    unsigned int    pkt_reddrops;       /* accepted pkts dropped due to
                                         * red */
    unsigned int    pkt_qdrops; /* accepted pkts dropped due to qsize */
    unsigned int    pkt_errs;   /* accepted pkts dropped due to errors */

    unsigned int    hop_pkts;   /* recent pkt count */
    unsigned int    hop_bytes;  /* recent pkt byte count */
    unsigned int    hop_toterr; /* recent pkt sum of errors */
    unsigned int    hop_toterrsq;       /* recent pkt sum of squares of
                                         * errors */

    unsigned int    pkt_pkts;   /* recent pkt count */
    unsigned int    pkt_bytes;  /* recent pkt byte count */
    unsigned int    pkt_toterr; /* recent pkt sum of errors */
    unsigned int    pkt_toterrsq;       /* recent pkt sum of squares of
                                         * errors */
  unsigned int    hop_scheduled;  /* current # hops scheduled */

    unsigned int    mc_send;    /* multi-core pkt sends */
    unsigned int    mc_recv;    /* multi-core pkt recvs */
    unsigned int    mc_exdrops; /* multi-core pkt expiration drops */
    unsigned int    mc_ttldrops;        /* multi-core pkt ttl drops */
    unsigned int    mc_errs;    /* multi-core pkt errors */

    unsigned int    mbuf_alloc; /* XXX mbuf statistics */
    unsigned int    mbuf_free;  /* XXX not currently used */
    unsigned int    mbuf_arrive;        /* XXX */

    unsigned int    pkt_alloc;  /* */
    unsigned int    pkt_free;   /* */
    u_int32_t       pcache_occupancy;
    u_int32_t       pkts_queued;        /* packets in the switch */
};

/* typedef TAILQ_HEAD(mn_pkt_head, packet) *mn_pkt_head_t; */
struct mn_pkt_head {
  struct packet *head;
};

struct mn_error {
	u_int32_t       last_hop_tick; /* bookkeeping */
	u_int32_t       missed_ticks;  /* total missed ticks due to
					* processing */
	u_int32_t       num_missed_ticks;      /* instances of missed
						* ticks */
	u_int32_t       delayzero;     /* zero delays calculated */
};


struct mn_config {
	/*
	 * modelnet core state
	 */
#define MN_PCACHE      0x1     /* Enable caching packet payloads */
#define MN_AGGREGATE   0x2     /* Enable aggregation of cross-core pkts */
#define MN_KERNEL      0x4     /* Modelnet running in the kernel */
	u_int32_t       state; /* configured options */
	unsigned long   ip_home;       /* ip addr (in net order) of this
					* node */
	int             mc_expire;     /* milliseconds to hold mc pkts */
	int             mc_ttl;        /* time-to-live for mc pkts */
	u_int64_t       tick_count;    /* soft-tick count */
	struct mn_pkt_head mc_pcache;  /* multi-core packet cache */
	struct mn_stats stats;
};

#define MC_HOP_LOCAL(hop)  (hop->emulator==0)

#define MC_PKT_HOME(pkt)   (pkt->cachehost==0)

#define MC_PKT_PCACHED(pkt) (pkt->info.id)

#define MODEL_FORCEOFF(addr) ((addr)&(~MODEL_FORCEBIT))


#define MN_FREE_PKT(pkt)	{	\
        if (pkt->skb) kfree_skb(pkt->skb);	\
	pkt->skb = NULL; \
        kfree(pkt);	\
        mn.stats.pkt_free++;}

/* #define MN_UNCACHE_PKT(pkt)	{	\ */
/*         TAILQ_REMOVE(&(mn.mc_pcache),pkt,list);\ */
/*         pkt->info.id = NULL; \ */
/*         mn.stats.pcache_occupancy--;} */

/* #if 0 */
/* #define MN_DUMP_PKT(pkt) {	\ */
/*         struct ip *ip;\ */
/* 	printf("MN_DUMP_PKT: mbuf(0x%x), info.len(%d) m_len(%d) m_data(%x) m_nxtpkt(%x)\n",pkt->m, pkt->info.len,m->m_len,m->m_data,m->m_nextpkt);\ */
/*         if(pkt->m){\ */
/*              ip = mtod(pkt->m, struct ip*);\ */
/*              printf("> ip_src = %s\n", inet_ntoa(ip->ip_src));\ */
/*              printf("> ip_dst = %s\n", inet_ntoa(ip->ip_dst));\ */
/*              printf("> ip_prot = %d\n", ip->ip_p);\ */
/*              printf("> ip_len = %d\n", ip->ip_len);\ */
/*              printf("> ip_ttl = %d\n", ip->ip_ttl);\ */
/*          }} */
/* #else */
/* #define MN_DUMP_PKT(pkt) */
/* #endif */



/* #define DUMP_HOP(hop){\ */
/*     printf("dump_hop: line %d, hop->resid(%d), hop->currslot(%d), len(%d), hop->KBps(%d), hop(0x%x) ", __LINE__,hop->residual,hop->currslot, len, hop->KBps, (u_int32_t) hop);\ */
/*     printf(" popticks(%u) tick(%u) hop->stamp(%u)\n",popticks, ticks, hop->stamp);\ */
/* } */



/*
 * silly helper for printing net-order ip addresses in 32bit ints
 */
/*
 * use printf("... %lu.%lu.%lu.%lu ...", ..., IP_ADDR_QUAD_N(n_addr),
 * ...);
 */
#define IP_ADDR_QUAD_N(x) IP_ADDR_QUAD_H(ntohl(x))

/*
 * silly helper for printing host-order ip addresses in 32bit ints
 */
/*
 * use printf("... %lu.%lu.%lu.%lu ...", ..., IP_ADDR_QUAD_H(n_addr),
 * ...);
 */
#define IP_ADDR_QUAD_H(x) (((x)>>24)&0xff), (((x)>>16)&0xff), \
			    (((x)>>8)&0xff), ((x)&0xff)



/* /\* */
/*  * print levels  */
/*  *\/ */
/* #define MN_P_PKTS    0x1 */
/* #define MN_P_ERR     0x2 */
/* #define MN_P_DOUBLEQ 0x4 */
/* #define MN_P_AGG     0x8 */

/* #define P_XCP        0x10 */
/* #define P_XCP_TO     0x20  /\* Te To timeouts *\/ */
/* #define P_XCP_FB     0x40  /\* feedback *\/ */
/* #define P_XTQ        0x80  /\* eXtended queueing info *\/ */



/* /\* */
/*  * supports multiple flags  */
/*  *\/ */
/* #if 1 */
/* #define Dprintf(x,level) {\ */
/*         if (level == (mn_debug_g & level))\ */
/*             printf x;\ */
/* } */
/* #else */
/* #warning "DPRINTF UNDEFINED (in tuf_var.h)" */
/* #define Dprintf(x,level) */
/* #endif */



/*
 * ASSERT macro
 */

#ifndef ASSERT
#define XSTR(x) # x
#if 1
#define ASSERT(x) if (!(x)) \
panic("assert failed \"%s\" %s line %d", XSTR(x), __FILE__, __LINE__);
#else
#warning "ASSERTS ARE NOT COMPILED IN!!!"
#define ASSERT(x)
#endif
#endif


struct hop    **lookup_path(in_addr_t src, in_addr_t dst);
void            uninit_paths(void);
int             remote_hop(struct packet *, in_addr_t);
void            emulate_nexthop(struct packet *pkt, int needlock);




/* extern u_short  remote_port; */
extern struct mn_config mn;
extern struct mn_error g_error;
extern u_int32_t mn_debug_g;
/* extern struct hoplist *hop_calendar;                  */

/* #endif                          /\* _KERNEL *\/ */



/* Random Number Generation */
#define MERS_N   624
#define MERS_M   397
#define MERS_R   31
#define MERS_U   11
#define MERS_S   7
#define MERS_T   15
#define MERS_L   18
#define MERS_A   0x9908B0DF
#define MERS_B   0x9D2C5680
#define MERS_C   0xEFC60000

extern void random_seed(u_int seed);
extern u_int random_bits(void);     

#endif                          /* _IP_MODELNET_H */
