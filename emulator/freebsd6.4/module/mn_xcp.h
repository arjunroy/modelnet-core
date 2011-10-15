/*
 * modelnet  mn_xcp.h
 *
 * eXplicit Congestion control for ModelNet
 *
 * Duke University copyright(c) 2003
 * See COPYING for license statement.
 * 
 * Adapted from original XCP NS implementation Dina Katabi (dina@ai.mit.edu) 
 * Issues:
 * We use u_int64's everywhere. This is slow. 
 * There appear to be overflow issues with high-capacity links.
 */

#ifndef _MN_XCP_H
#define _MN_XCP_H

/* ASSUME droptail queues */

#define MULTI_FAC   1000  /* 1000 or 1, a division factor, 1000 puts
			     things in KB */
#define FP_SHIFT 10  /* almost 3 decimal digits of precision */
/* in some cases avg delay is large, packet size is small, inv rate
 * psize/cwnd becomes a small figure, so we need more accuracy.
 * This gives us more accuracy, but we may roll over even with 64bit
 * uints for large bandwidths!
 */
#define FP_EXTRA 1  /* for increased accuracy change from 1 to 10 */

#define noSMALL_PKT_REDEMPTION

#define PRINT_FREQUENCY 100 /* for To and Te */

extern int xcp_initialized;




/* XXX ACP/TUF/XCP protocol headers and constants XXX 
 * IPPROTO_TUF is defined in 
 * in.h (but only ours)
 * so we redefine it here, so that
 * people can compile this MN w/o
 * TUF
 */
#ifdef _KERNEL

#define IPPROTO_TUF 132 

/* from acp.h and acp_var.h */

struct acphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */

#if BYTE_ORDER == LITTLE_ENDIAN
		u_int	th_x2:4,		/* (unused) */
			th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN
		u_int	th_off:4,		/* data offset */
			th_x2:4;		/* (unused) */
#endif

	u_char	th_flags;
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */

	/* XCP header */
	u_int  acp_cwnd;                /* current window */
	int    acp_src_fdbk;            /* network router feedback  */
	int    acp_rflct_fdbk;            /* bounced from rcvr */ 
	u_int  acp_rtt;                 /* round trip time sample */
	u_int  acp_rpatch;              /* redirect patch */
};

/*
 * Tcp+ip header, after ip options removed.
 */
struct acpiphdr {
	struct 	ipovly ti_i;		/* overlaid ip structure */
	struct	acphdr ti_t;		/* acp header */
};


/* From mbuf.h: */
#define CSUM_ACP                0x1000          /* will csum ACP */

#endif //_KERNEL
/* XXX END ACP/TUF/XCP  */


/*
 * XCP support per-hop in the modelnet core
 */
typedef struct _xcp_pipe_{
	
    /* ------------ Variables ----------------- */
    u_int32_t          routerId_;
    u_int64_t          link_capacity_Kbytes_;
    
    /*----- Estimates ---------*/
    u_int64_t          Te_; /* Timeout for  estimator decisions*/
    u_int64_t          Tq_; /* Timeout for control decisions    */
    u_int64_t          avg_rtt_;
    u_int64_t          xi_pos_;  /* Can these 4 be negative? NO. */
    u_int64_t          xi_neg_;
    u_int64_t          BTA_;
    u_int64_t          BTF_;
    u_int64_t          Queue_Kbytes_; /* our estimate of the fluid model queue*/
    
    /*----- General Helping Vars -*/
    /* An interval over which a fluid model queue is computed*/
    u_int64_t          input_traffic_Kbytes_;       /* traffic in Te */
    u_int64_t          sum_rtt_by_cwnd_; 
    u_int64_t          sum_rtt_square_by_cwnd_;
    u_int64_t          running_min_queue_Kbytes_;
    u_int64_t          num_cc_packets_in_Te_;	
    u_int64_t          num_Kbytes_in_Te_;	 /* replaces num_cc_ */
} *xcp_pipe_t;


#define XCP_NOW() { return ticks; }
#define XCP_MAX(d1,d2) ((d1 > d2) ? d1 : d2 ) 
#define XCP_MIN(d1,d2) ((d1 < d2) ? d1 : d2 )
#define ABSOLUTE(d)  ((d >= 0) ? d : (-1*d))

#if 0 /* this doesn't look likely */
#define TE_BY_RTT(_p, _value) ((((int64_t)_value)*((int64_t)(_p->Te_<<FP_SHIFT) / hz)) / rtt)
#else
#define TE_BY_RTT(_p, _value) (((_value)*((_p->Te_<<FP_SHIFT) / hz)) / rtt)
#endif


/* Prototypes */
void xcp_enqueue(struct hop *h,mn_pkt_t pkt);
void xcp_dequeue(struct hop *h,mn_pkt_t pkt);
void fill_in_feedback(struct hop *h, mn_pkt_t pkt);  
void xcp_init(void);
void xcp_uninit(void);
int xcp_alloc_pipe(struct hop *h);
void xcp_free_pipe(struct hop *h);
void Tq_timeout(struct hop *h);
void Te_timeout(struct hop *h);
u_int32_t running_avg(u_int32_t var_sample, u_int32_t var_last_avg,
		      u_int32_t gain);
void xcp_get_xcpinfo(mn_pkt_t pkt);
void xcp_set_xcpinfo(mn_pkt_t pkt);





#endif // _MN_XCP_H
