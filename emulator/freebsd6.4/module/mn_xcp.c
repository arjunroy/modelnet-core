/*
 * modelnet  mn_xcp.c
 * Duke University copyright 2003 
 * See COPYING for license statement.
 * Kenneth G. Yocum
 * 
 *
 *     emulate eXplicit Congestion Control perhop
 *     This code is based on Dina Katabi's (dina@ai.mit.edu) NS
 *     implementation (http://www.ana.lcs.mit.edu/dina/XCP/) of XCP
 *     
 *
 *
 * Original Description: all quantities are measured in Kbytes rather than
 * bytes.  
 * Code Assumptions:  
 *
 *     + packets size in a flow is constant
 *     + capacity, BTA, BTF are measured in KB, times are measured in seconds
 *     + cwnd and H_feedback are measured in packets (this inconsitency 
 *       is not necessary but makes code easier since TCP code already
 *       measures cwnd in packets)
 *     + The FC controller converges to a state in which all flows
 *       have same BW in packets/sec
 *
 * ModelNet Assumptions
 * The original code everything is done in bytes.   In other
 * words cwnd and feedback are measured in bytes.  And because we're
 * using a framing protocol, we need to measure the feedback to a
 * packet not in terms of just per-packet feedback but per-byte
 * feedback.  We then use individual packet size to apportion
 * feedback.  
 *
 * There is no floating point in the kernel, everything is in fixed point.
 * pkt->xcp_cwnd is in BYTES
 * pkt->rtt is in seconds shifted by FP_SHIFT (ms granularity)
 * pkt->xcp_cwnd is zero for ack packets
 *
 * When packets arrive we check to see if they are using TUF/ACP
 * if not then we set xcp_cwnd=0 which should keep this from running
 * 
 * TODO: Lower the overhead of timeouts for lots of XCP links (only
 * run it when the queue is non empty) 
 * 
 * Also using different frame sizes appears to be unfair.
 * Verify/fix. 
 *
 */

#ifndef _KERNEL
/* user-level includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/errno.h>
#ifdef Intel_FREEBSD
#include <sys/poll.h>
#include <sys/timers.h>
#endif
#include "acp_queue.h"
/* end user-level includes */
#else  
/* kernel includes */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_var.h>
#include <netinet/ip_dummynet.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
/* end kernel includes */
#endif

#include "ip_modelnet.h"
#include "mn_xtq.h"
#include "mn_xcp.h"


#define  INITIAL_Te_VALUE   (hz/20) /* 0.05 in ticks assume hz=10000 */

/* eXtended queues allocate from this pool */
MALLOC_DEFINE(M_MN_XCP, "MN/XTQ", "Modelnet/eXtended Queues");


static u_int32_t INITIAL_RTT_ESTIMATE;
static unsigned int next_router = 0;
static  int alpha_g_ = 0;
static  int beta_g_ = 0; 
static  int gamma_g_ = 0;
int xcp_initialized = 0;
static int xcp_pipe_count=0;

/* SHIFTED CONSTANTS - C002 = 0.002<<FP_SHIFT */
#define C1  1024  /* 1 << FP_SHIFT */
#define C1D1  1126  /* 1.1 << FP_SHIFT */
#define CD8   819
#define CD6   614
#define CD4   409
#define CD226 231
#define CD1   102
#define CD002 2

/* SHIFTED CONSTANTS in ticks */
#define TICKS_CD002 (hz/500) /* 0.002*hz = 1/500*hz */

#define ALPHA_G (CD4) /* 0.4<<FP_SHIFT */
#define BETA_G (CD226) /* 0.226<<FP_SHIFT */
#define GAMMA_G (CD1) /* 0.1<<FP_SHIFT */

/*
 * [xcp_init]
 *
 * Initialize ModelNet for XCP.
 * Automatic with first xcp_alloc_pipe() call
 */
void
xcp_init(void)
{
	alpha_g_ = ALPHA_G; /* pos fdbk */
	beta_g_ = BETA_G; /* neg fdbk */
	gamma_g_ = GAMMA_G; /* shuffle percentage */
	xcp_pipe_count = 0;
	if (hz < 1000){
		printf("xcp_init: system hz(%d) should be >= 1000\n",hz);
	}
	INITIAL_RTT_ESTIMATE = (hz/1000) /* 0.001 * 1024 */;
	if (FP_EXTRA == 1){
		printf("xcp_init: running with reduced accuracy.\n");
	} else if (FP_EXTRA == 10){	
		printf("xcp_init: running with increased accuracy.\n");
	} else {
		printf("xcp_init: ERROR, FP_EXTRA not 1 or 10!\n");
	}
	
	if (MULTI_FAC != 1000 && MULTI_FAC != 1){
		printf("xcp_init: BAD MULTI_FAC (%d), use 1000 or 1!!\n",
		       MULTI_FAC);
	}
	

	if (!(CSUM_DELAY_DATA & CSUM_ACP)){
		printf("xcp_init: This kernel doesn't support cksum offload for ACP,");
		printf("your mileage may vary!\n");
	}
	
	xcp_initialized = 1;
	
	printf("xcp_init: ModelNet XCP initialized. \n");
}

/*
 * [xcp_uniinit]
 *
 * Teardown XCP within a modelnet core.
 * Automatic with last xcp_free_pipe() call.
 */
void
xcp_uninit(void)
{
	xcp_initialized = 0;
	printf("xcp_uninit: completed.\n");
}



/*
 * [xcp_free_pipe]
 *
 * Unschedule timers if present. 
 * 
 */
void
xcp_free_pipe(struct hop *h)
{
	xcp_pipe_count--;
	if (xcp_pipe_count == 0){
		/* last pipe, free state */
		xcp_uninit();
	}
}



/*
 * [xcp_get_xcpinfo]
 *
 * Packet has arrived, make sure it's a TUF packet
 * then grab the XCP info and install in remote packet
 * 
 */
void
xcp_get_xcpinfo(mn_pkt_t pkt)
{
#ifdef _KERNEL
	struct acpiphdr *acpip = mtod(pkt->m, struct acpiphdr *);
	struct acphdr *acp = &acpip->ti_t;
	struct ip *ip = (struct ip *)&(acpip->ti_i);
/*	static u_int32_t xcp_pkt_cnt=0;*/

	if (!(ip->ip_p == IPPROTO_TUF)){
		Dprintf(("xcp_get_pktinfo: Emulation has XCP links, but "),P_XCP);
		Dprintf(("you're not using ACP endpoints!\n"),P_XCP);
		pkt->info.xcp_cwnd =  0;
		pkt->info.xcp_src_fdbk = 0;
		pkt->info.xcp_rflct_fdbk = 0;
		pkt->info.xcp_rtt = 100;
	} else {
		pkt->info.xcp_cwnd = ntohl(acp->acp_cwnd);
		pkt->info.xcp_src_fdbk = ntohl(acp->acp_src_fdbk);
		pkt->info.xcp_rflct_fdbk = ntohl(acp->acp_rflct_fdbk);
		pkt->info.xcp_rtt = ntohl(acp->acp_rtt);
		Dprintf(("xcp_get_xcpinfo: cwnd(%u) src_fdbk(%d) rflct_fdbk(%d) rtt(%u)\n",
		       pkt->info.xcp_cwnd,
		       pkt->info.xcp_src_fdbk,
		       pkt->info.xcp_rflct_fdbk,
		       pkt->info.xcp_rtt), P_XCP);
	}
#endif //_KERNEL
}


/*
 * [xcp_set_pktinfo]
 *
 * Packet is leaving, make sure it's a TUF packet
 * Install the updated XCP information in the outgoing 
 * potentially cached packet.
 * 
 */
void
xcp_set_xcpinfo(mn_pkt_t pkt)
{
#ifdef _KERNEL
	struct acpiphdr *acpip;
	struct acphdr *acp;
	struct ip *ip;

	ip = mtod(pkt->m, struct ip *);

	if (!(ip->ip_p == IPPROTO_TUF)){
		Dprintf(("xcp_set_pktinfo: On exit packet no longer "),P_XCP);
		Dprintf(("from an ACP endpoint??\n"),P_XCP);
	} else {
		pkt->m = m_pullup(pkt->m, sizeof(struct acpiphdr));
		acpip = mtod(pkt->m, struct acpiphdr *);
		acp = &acpip->ti_t;		
		acp->acp_src_fdbk = htonl(pkt->info.xcp_src_fdbk);
		L4_acp_cksum(pkt->m);
	}
#endif // _KERNEL
}



/*
 * [xcp_alloc_pipe]
 * Allocate the pipe to emulate an XCP router
 * each pipe registers it's own timers.
 * this may become expensive with thousands of active pipes trying to
 * schedule themselves. Only pipes that are active should be
 * scheduling.  If no packets of flows arrive in 2 epochs, then we
 * don't schedule these functions.   They get rescheduled when packets 
 * are requeued. 
 * 
 */
int
xcp_alloc_pipe(struct hop *h)
{
	xcp_pipe_t p = NULL;
	u_int32_t T, Trange;
	
	
	/* first init XCP if necessary */
	if (!xcp_initialized){ xcp_init(); }
	xcp_pipe_count++;
	/* allocate xcp pipe structure */
	p = h->xtq.xt_hop = (void*)malloc(sizeof(struct _xcp_pipe_),
					  M_MN_XCP, M_NOWAIT);
	if (!h->xtq.xt_hop){
		printf("xtq xcp alloc failed\n");
		return ENOMEM;
	}
	if (h->delay == 1){
		printf("xcp_alloc_pipe: Lower bound one-way delay is 2ms!\n");
	}
	


	/* ------------- Estimation & Control Helpers */
	p->routerId_             = next_router++; 

	/* already in kilobytes " /8/MULTI_FAC"
	 * If MULTI_FAC == 1, then this should be in Bps
	 * If MULTI_FAC == 1000, then this should be in KBps 
	 */
	p->link_capacity_Kbytes_ = h->KBps * (1000/MULTI_FAC); 
	p->avg_rtt_              = INITIAL_Te_VALUE;
	
	p->Te_      = INITIAL_Te_VALUE;
	p->Tq_      = INITIAL_Te_VALUE; 
	p->xi_pos_  = 1; 
	p->xi_neg_  = 0; 
	/* limit() is qsize in pkts, according to NS queue object */
	
	Dprintf(("mn_xcp_install: routerId (%d) linkcapKBps(%llu) \n",
	       p->routerId_, p->link_capacity_Kbytes_),P_XTQ);
	
	p->BTA_ = XCP_MIN(((u_int64_t)(h->qsize*(MN_MTU / MULTI_FAC))/2),
			  (((alpha_g_ * p->link_capacity_Kbytes_ *
			     p->Te_)>>FP_SHIFT)/hz));
	

	p->BTF_     = 0;     
	p->Queue_Kbytes_ = 0; /* our estimate of the fluid model queue
			       */
	p->input_traffic_Kbytes_    = 0; 
	p->sum_rtt_by_cwnd_        = 0; 
	p->sum_rtt_square_by_cwnd_ = 0;
	p->running_min_queue_Kbytes_= 0;
	p->num_cc_packets_in_Te_   = 0;
	p->num_Kbytes_in_Te_   = 0;
	
	
	/* Scheduling timers randommly so that routers are not
	   synchronized */
	T = random(); /* btwn 0 and (2^31)-1 == 2147483647 == 0x7fffffff*/
	Dprintf(("xcp_alloc_pipe: T (%u) ", T), P_XCP);
	T = ((((u_int64_t)T<<FP_SHIFT)) /  0x7fffffff );
	Trange = (p->Tq_ - (p->Tq_ / 5));
	Dprintf(("scaled T (%u) Trange (%u)\n", T, Trange),P_XCP);
	/* time is in seconds so convert to ticks */
	T = (((T * Trange)>>FP_SHIFT) + (p->Tq_/5));
	Dprintf(("Tq_ Timeout (in ticks) %u \n", T),P_XCP);
	if (T < hz/250) { T = hz/250; }
	XTQ_SCHEDULE_HOP(h,Tq_timeout,T);

	T = random(); /* btwn 0 and (2^31)-1 == 2147483647 == 0x7fffffff*/
	T = ((((u_int64_t)T<<FP_SHIFT)) / 0x7fffffff );
	Trange = (p->Te_ - (p->Te_ / 5));
	T = (((T * Trange)>>FP_SHIFT) + (p->Te_/5));
	Dprintf(("Te_ Timeout (in ticks) %u \n", T), P_XCP);
	if (T < hz/250) { T = hz/250; }
	XTQ_SCHEDULE_HOP(h,Te_timeout,T);
	
/* XXX Watch out!!! I think + binds harder than >> or << !!!!*/
	return(0);
}


/*----------------------- Timeouts  -----------------------------------------*/

/*
 * [Tq_timeout]
 * 
 * Sample the queue.  This timeout may be unnecessary.
 */
void Tq_timeout(struct hop *h){
	xcp_pipe_t p = (xcp_pipe_t)(h->xtq.xt_hop);
	static int pr_fr = PRINT_FREQUENCY;
	

	p->Queue_Kbytes_ = p->running_min_queue_Kbytes_;
	p->running_min_queue_Kbytes_ = h->bytedepth/MULTI_FAC;
	/* NS byteLength() ==> MN h->bytedepth */
	p->Tq_ = XCP_MAX((TICKS_CD002),
			 (int64_t)(p->avg_rtt_ - /* avg_rtt in hz */
			   (u_int64_t)(h->bytedepth*hz)/(MULTI_FAC*p->link_capacity_Kbytes_))/2);

	if (--pr_fr <= 0){
		Dprintf(("Tq_timeout: router(%d) p->Tq_ (%llu) ticks \n",
			 p->routerId_,p->Tq_),P_XCP_TO);
		Dprintf(("            running_minq(%llu) Queue_Kbytes_(%llu)  \n",
			 p->running_min_queue_Kbytes_, p->Queue_Kbytes_),P_XCP_TO);
		Dprintf(("            avg_rtt(%llu) bytedepth(%d) linkcap(%llu)\n",
			 p->avg_rtt_, h->bytedepth, p->link_capacity_Kbytes_),P_XCP_TO);
		pr_fr = PRINT_FREQUENCY;
	}
	
	XTQ_SCHEDULE_HOP(h,Tq_timeout,p->Tq_); 
}

/*
 * [Te_timeout]
 * 
 * Every interval, recalculate the amount of feedback to appropriate
 * to the flows.  Also reshuffle bandwidth.  
 */
void Te_timeout(struct hop *h)
{
	xcp_pipe_t p = (xcp_pipe_t)(h->xtq.xt_hop);
	int64_t phi_Kbytes = 0;
	int rate_diff;
	static int pr_fr1 = PRINT_FREQUENCY, pr_fr2 = PRINT_FREQUENCY;
	u_int64_t shuffled_traffic_Kbytes = 0;
	
	/* RTT is in ticks, but here it's in seconds<<FP
	 * we restore to ticks at exit.  
	 * BTA/BTF are in KB.
	 * alpha_g, gamma_g, beta_g are in FP.
	 */
	if(p->sum_rtt_by_cwnd_ > 0){  /* check we received packets */
		p->avg_rtt_ = /* FP * rtt_seconds */
			(u_int64_t)(p->sum_rtt_square_by_cwnd_)/
			(u_int64_t)p->sum_rtt_by_cwnd_;
		/* must do (int) casting to preserve negative values */
		/* phi is in FP*KB, One factor FixedPoint in KBytes */
		rate_diff = (int)(p->link_capacity_Kbytes_- 
			     (p->input_traffic_Kbytes_*hz) /p->Te_);

/* ARGH, int64_t's suck! I couldn't get this to work.
 * 5MB/s 20RTT -> original failed b/c it wouldn't be a negative number 
 * 10MB/s 60RTT -> failed b/c a plain int would roll over.
 * So now we do this.  I think it failed b/c the shift forced the bit
 * off the rate_diff.  
 */		
		phi_Kbytes = (  (rate_diff *  ((alpha_g_ * p->avg_rtt_)>>FP_SHIFT) ) - 
			      (int)(beta_g_ * p->Queue_Kbytes_));

		
		/* leave comparison in FP*KB > FP*KB */
		if( ABSOLUTE(phi_Kbytes) >  
		    ((gamma_g_ * p->input_traffic_Kbytes_)) ) {
			shuffled_traffic_Kbytes = 0;
		} else {
			shuffled_traffic_Kbytes = (u_int64_t)((gamma_g_ *
						    p->input_traffic_Kbytes_)) 
				- (u_int64_t)ABSOLUTE(phi_Kbytes);
		}
		/* both shuffle and phi are in FP */

		
		p->BTA_    =  XCP_MAX(0, phi_Kbytes)   + shuffled_traffic_Kbytes;


/* I think this is to keep it from overshooting, not to make sure we
 * ramp up fast enough - kgy
 */
/* Qsizes will affect the performance here.  They affect the max
 * increase, with a small queue sometimes you can't get an increase 
 * that is big enough to be spread across cwnd_i/s_i packets!
 */ 
/* Does this happen in the opposite direction???? */
		if (p->BTA_ > /* put comparison in FP, too */
		    ((((CD6 * h->qsize)>>FP_SHIFT) - h->slotdepth)*(MN_MTU / MULTI_FAC))<<FP_SHIFT) {
			Dprintf(("XCP_MAXIMUM INCREASE: from p->BTA_(%llu) to ", 
				 p->BTA_),P_XCP);  
			p->BTA_ = XCP_MAX(0,((( ((CD6 * h->qsize)>>FP_SHIFT) - h->slotdepth)
					  * (MN_MTU / MULTI_FAC))<<FP_SHIFT));
			Dprintf(("(%llu)\n", p->BTA_),P_XCP);

		}

		p->BTF_    =  XCP_MAX(0, -1*phi_Kbytes)+ shuffled_traffic_Kbytes;

		/* XXX Fixed Point Factors XXX
		 * xi_pos_ has NO FP 
		 * xi_neg_ has 1 FP 
		 *
		 * xi_pos = FP*FP*FP / 2FP*FP = KB
		 * xi_neg = FP*FP*FP / FP / FP = FP
		 */
		/* Add extra FP, overflow on numerator */
/* Multiplication by 1.1 is experimental */
		p->xi_pos_ =  (((u_int64_t)(C1D1 * p->BTA_)<<FP_SHIFT<<FP_EXTRA) / (p->sum_rtt_by_cwnd_ *
					    p->avg_rtt_)); 
/* Multiplication by 1.1 is experimental */
		p->xi_neg_ =  (((u_int64_t)(C1D1 * p->BTF_)<<FP_SHIFT) / (p->avg_rtt_ *
					    (u_int64_t)(p->num_Kbytes_in_Te_)))>>FP_SHIFT;

		if (--pr_fr1 <= 0){			
			Dprintf(("------>Te_timeout: router(%d) avg_rtt(%llu) phi_Kbytes(%llu) capacity(%llu)KB/s sampled(%llu)KB/s Qeueu_KB(%llu)\n",
				 p->routerId_,
				 p->avg_rtt_, phi_Kbytes, p->link_capacity_Kbytes_,
				 (p->input_traffic_Kbytes_*hz) /p->Te_, p->Queue_Kbytes_), P_XCP_TO);
			
			Dprintf(("max(0,%lld)=%lld + shuffle(%llu) = %llu\n", 
				 phi_Kbytes, XCP_MAX(0,phi_Kbytes),
				 shuffled_traffic_Kbytes,
				 p->BTA_),P_XCP_TO);
			
			Dprintf(("                   N(%llu) BTA(%llu) BTF(%llu) xi_pos(%llu) xi_neg(%llu) sum_rtt_by_cwnd_(%llu) shuffled(%llu)\n",
				 p->sum_rtt_by_cwnd_/p->avg_rtt_,
				 p->BTA_, p->BTF_, p->xi_pos_, p->xi_neg_, p->sum_rtt_by_cwnd_,
				 shuffled_traffic_Kbytes), P_XCP_TO);
			pr_fr1 = PRINT_FREQUENCY;
		}
		
		/* back to ticks! */
		p->avg_rtt_ = (p->avg_rtt_ * hz) >> FP_SHIFT; 
	}
#if 0
	if (debug_xcp) {
		printf("Q%d: %g estimation_control_timer TIMEOUT Te_ = %g\n", p->routerId_, ticks, p->Te_);
		printf("Q%d: %g RTT %g\t N %g\t y %g\t Q %g\t phi %g\t H %g\t BTA %g\t BTF %g\t xi_p %g\t xi_n %g\n", 
		       p->routerId_, ticks, p->avg_rtt_, p->sum_rtt_by_cwnd_/p->avg_rtt_, p->input_traffic_Kbytes_/p->Te_, p->Queue_Kbytes_,
		       phi_Kbytes/p->avg_rtt_, shuffled_traffic_Kbytes/p->avg_rtt_, 
		       p->BTA_, p->BTF_, p->xi_pos_, p->xi_neg_);
	}
#endif 
	p->input_traffic_Kbytes_ = 0;
	p->num_cc_packets_in_Te_ = 0;
	p->num_Kbytes_in_Te_ = 0;
	p->sum_rtt_by_cwnd_ = 0;
	p->sum_rtt_square_by_cwnd_ = 0;
	p->Te_ = p->avg_rtt_;
	if (--pr_fr2 <= 0){
		Dprintf(("Te_timeout: router(%d) ticks(%u) routerId(%d) p->Te_(%llu) and avg_rtt (ticks) %llu\n", 
			 p->routerId_,ticks, p->routerId_, p->Te_, p->avg_rtt_),P_XCP_TO);
		pr_fr2 = PRINT_FREQUENCY;
	}
	
	XTQ_SCHEDULE_HOP(h,Te_timeout,p->Te_);
}


/*--------------------- Utility Code -------------------------------------*/

/* fill_in_feedback[]
 *
 * before a packet leaves a router, update it's feedback
 * math in the comments is before FP_EXTRA (an extra shift for more precision)
 */
void fill_in_feedback(struct hop *h, mn_pkt_t pkt){
	u_int64_t inv_rate=0, rtt=0;
	int64_t feedback_FP = 0,pos_feedback_Kbytes=0, neg_feedback_Kbytes=0;
	int feedback_Kbytes =0, feedback_bytes =0;	
	xcp_pipe_t p = (xcp_pipe_t)(h->xtq.xt_hop);
#ifdef SMALL_PKT_REDEMPTION
	int max_redemption = 2;
#endif
	static int pr_fr = PRINT_FREQUENCY;
	


	/* filter out ack-only packets or packets with 0 rtt */
	if (pkt->info.xcp_cwnd > 0 && pkt->info.xcp_rtt > 0) { 
		if (pkt->info.len <=0){
			printf("fill_in_feedback: info.len <=0\n");
			goto OUT;
		}
		/* assuming xcp_rtt is in FP seconds */ 
		rtt      = pkt->info.xcp_rtt;
#ifdef SMALL_PKT_REDEMPTION
	REDEMPTION:
#endif
		/* inv_rate = FP*FP*RTT / KB */
	        inv_rate = ((u_int64_t)
			    ((rtt*(pkt->info.len/MULTI_FAC))<<FP_SHIFT<<FP_EXTRA) / 
			    (pkt->info.xcp_cwnd/MULTI_FAC));

		/* noFP: xi_pos
		 * FPx1: BTA, BTF, rtt, xi_neg
		 * FPx2: inv_rate, sum_rtt_by_cwnd_
		 * FPx3: sum_rtt_square_by_cwnd_
		 */
		/* 1 * 2FP * FP * 1 / 2FP = FP */
		/* *pkt_size is in inv_rate already */
		pos_feedback_Kbytes = XCP_MIN(p->BTA_, 
					      (((((u_int64_t)p->xi_pos_ *
						 inv_rate * rtt  
						  ))>>FP_SHIFT)>>FP_SHIFT>>FP_EXTRA));  
		/* FP * FP * 1 / FP = FP */
		neg_feedback_Kbytes = XCP_MIN(p->BTF_, 
					      ((((u_int64_t)p->xi_neg_
						 * rtt * (pkt->info.len/MULTI_FAC)))>>FP_SHIFT));


		feedback_FP = ((pos_feedback_Kbytes -
				    neg_feedback_Kbytes));
	
		/* Convert everything OUT of FP 
		 * NOTE: leave {pos,neg}_feedback_Kbytes in FP
		 * as we continue to keep them as residues
		 * NOTE: Careful! cast to int is the *last* thing you do!
		 */	
		feedback_Kbytes = (int)((pos_feedback_Kbytes  -
					neg_feedback_Kbytes) >> FP_SHIFT);
		
		feedback_bytes = (int)(((pos_feedback_Kbytes  -
					neg_feedback_Kbytes) * MULTI_FAC) >> FP_SHIFT);
		

		if (feedback_Kbytes < 0 && feedback_FP > 0){
			printf("------>feedback_Kbytes(%d) <0 , feedback_FP(%lld) > 0\n",
			       feedback_Kbytes, feedback_FP);
		}
		

		/* KGY 
		 * If pktsize is too small, small changes can get
		 * lost, say you up the cwnd by 1000 bytes, but you get
		 * 100K packets in that interval.  IN that case our
		 * control interval is too long!
		 * 
		 */	
#ifdef SMALL_PKT_REDEMPTION
		if (max_redemption>0 && 
		    (pos_feedback_Kbytes && neg_feedback_Kbytes==0 && feedback_bytes==0) || 
		    (neg_feedback_Kbytes && pos_feedback_Kbytes==0 && feedback_bytes==0)
		    ){
			Dprintf(("FEEDBACK REDEMPTION: rtt(%llu) to rtt(%llu) rd(%d)\n",
			       rtt, rtt*2, max_redemption),P_XCP_TO);
			rtt *=2;
			max_redemption -= 1;
			goto REDEMPTION;
		}
#endif
#if 0
		Dprintf(("fill_in_feedback: inv_rate(%llu) rtt(%llu) pos_FB_KB(%lld) neg_FB_KB(%lld) ",
		       inv_rate, rtt,
		       pos_feedback_Kbytes, neg_feedback_Kbytes), P_XCP_FB);
		
		Dprintf(("fb_Kbytes(%d) fb_bytes(%d)\n", 
		       feedback_Kbytes, feedback_bytes), P_XCP_FB);
		
		Dprintf(("                 BTA_(%llu) BTF_(%llu) cwnd(%d)\n",
			p->BTA_, p->BTF_, pkt->info.xcp_cwnd), P_XCP_FB);
#endif
		
		if (pkt->info.xcp_src_fdbk >= feedback_bytes) {
			if (--pr_fr <= 0){			
				Dprintf(("fill_in_feedback: Updating pkt->xcp_cwnd(%u) pkt->xcp_fdbk(%d) feedback_bytes(%d) rtt(%llu) avg_rtt_ticks(%llu)\n",
					 pkt->info.xcp_cwnd,
					 pkt->info.xcp_src_fdbk, feedback_bytes, 
					 rtt, p->avg_rtt_), P_XCP_FB);
				pr_fr = PRINT_FREQUENCY;
			}
			



			/* A Less congested upstream router */
			pkt->info.xcp_src_fdbk = feedback_bytes;
			/* XXX NOT YET XXX
			   pkt->info.controlling_hop_ = p->routerId_;
			*/
			/* TE_BY_RTT is FP-less */
			p->BTA_ = XCP_MAX(0, p->BTA_ - TE_BY_RTT(p,pos_feedback_Kbytes));
			p->BTF_ = XCP_MAX(0, p->BTF_ - TE_BY_RTT(p,neg_feedback_Kbytes));
		} else {
			/* a more congested upstream
			 * router has changed the
			 * congestion info. The code
			 * below might be changed to
			 * balance the spreading in
			 * all quatities
			 */
			/* everything here is SCALED by << FP_SHIFT */
			/* scale to avoid missing < 1KB packets */
			int fwd_feedback_Kbytes = (pkt->info.xcp_src_fdbk<<FP_SHIFT)/(MULTI_FAC);
			if ( fwd_feedback_Kbytes > 0){ 
				p->BTA_  -= XCP_MIN(TE_BY_RTT(p,fwd_feedback_Kbytes),
						    p->BTA_);
				p->BTF_  -= XCP_MIN(p->BTF_ ,
						    TE_BY_RTT(p,((pos_feedback_Kbytes -
								  neg_feedback_Kbytes) - 
								 fwd_feedback_Kbytes)));
			} else {
				p->BTF_  -=
					XCP_MIN(TE_BY_RTT(p,(-1*fwd_feedback_Kbytes)),p->BTF_);
				if (feedback_FP >0){
					p->BTF_ -=
						XCP_MIN(TE_BY_RTT(p,(pos_feedback_Kbytes - 
								     neg_feedback_Kbytes)), 
							p->BTF_);
				}
			}
		}
		
#if 0
		if (debug_xcp){
			printf("Q%d: %g, BTA %g, BTF %g, pos_fbk %g, neg_fbk %g, H_Feedback %g, cwnd %g, rtt %g, R %g \n",
			       p->routerId_, ticks, p->BTA_,
			       p->BTF_, pos_feedback_Kbytes, 
			       neg_feedback_Kbytes, pkt->info.xcp_src_fdbk, 
			       pkt->info.xcp_cwnd,
			       pkt->info.xcp_rtt, 
			       pkt->info.xcp_cwnd/pkt->info.xcp_rtt); 
		}
#endif
	} else {
		/* there may be a bogus value in src_fdbk
		 * e.g., the source wants everything
		 * so make sure we don't change his window
		 * when we can't make an adjustment.
		 */		
		pkt->info.xcp_src_fdbk = 0;
	}
	
	
	
 OUT:
	return;
}




/* running_avg[]
 *
 * Fixed point version of taking ewma
 * gain in FP, [0,1000] = [0.0,1.0] 
 */
u_int32_t 
running_avg(u_int32_t var_sample, u_int32_t var_last_avg, u_int32_t gain)
{
	u_int32_t avg;
	if (gain < 0 || gain > 1000)
		printf("running_avg: exit(3)\n");	
	avg = gain * var_sample + (1000 - gain) * var_last_avg;
	avg = avg / 1000;
	return avg;
}



/*-------------------- Switch Callbacks -----------------------------*/


/* xcp_dequeue[]
 *
 * 
 * called in deque, before packet leaves used for writing the feedback
 * in the packet used for writing the feedback in the packet
 *
 */
void xcp_dequeue(struct hop *h, mn_pkt_t pkt)
{
	xcp_pipe_t p = (xcp_pipe_t)(h->xtq.xt_hop);
	
	if (h->bytedepth/MULTI_FAC  < p->running_min_queue_Kbytes_){
		p->running_min_queue_Kbytes_= h->bytedepth /MULTI_FAC;
	}
	fill_in_feedback(h,pkt);
	return;
}

/* xcp_enqueue[]
 *
 * do_on_packet_arrival(pkt)
 * enqueue,but packet may be dropped used for updating the
 * estimation helping vars such as counting the offered_load_,
 * sum_rtt_by_cwnd_
 */
void xcp_enqueue(struct hop *h, mn_pkt_t pkt)
{
	xcp_pipe_t p = (xcp_pipe_t)(h->xtq.xt_hop);
	u_int64_t rtt_ticks;

	p->input_traffic_Kbytes_ +=
		((u_int64_t)(pkt->info.len/MULTI_FAC));


	/* data packets (forward traffic) carry h_cwnd, h_rtt, h_fdbk 
	 * and in the reverse direction (which may be piggy backed) 
	 * you have the reflected feedback which we do not touch) 
	 */
	if (pkt->info.xcp_cwnd){  /* 0 on ack-only packets */
		p->num_cc_packets_in_Te_ += 1;
		p->num_Kbytes_in_Te_ += ((u_int64_t)(pkt->info.len/MULTI_FAC));
		rtt_ticks = ((pkt->info.xcp_rtt*hz)>>FP_SHIFT);
		if (rtt_ticks != INITIAL_RTT_ESTIMATE){
			u_int64_t test = p->sum_rtt_square_by_cwnd_;
			u_int64_t test2 = p->sum_rtt_by_cwnd_;
			u_int64_t inv_rate = 0;


			if ((pkt->info.xcp_cwnd/MULTI_FAC)){
				inv_rate =
					(((pkt->info.xcp_rtt * (pkt->info.len/MULTI_FAC))
					  <<FP_SHIFT<<FP_EXTRA) /* another scale */ 
					 / (pkt->info.xcp_cwnd/MULTI_FAC));
			}
			
			/* If FP_EXTRA = 10, then 
			 * sum_rtt_by_cwnd_ has 3FP
			 * sum_rtt_square_by_cwnd_ has 4FP
			 * 
			 * Otherwise
			 * 
			 * sum_rtt_by_cwnd_ has 2FP
			 * sum_rtt_square_by_cwnd_ has 3FP
			 */
			
			p->sum_rtt_by_cwnd_ += inv_rate;
			p->sum_rtt_square_by_cwnd_ +=
				(pkt->info.xcp_rtt * inv_rate);
			if (test > p->sum_rtt_square_by_cwnd_){
				printf("-----xcp_enqueue: test(%llu) > srSbcwnd(%llu)\n",
				       test,
				       p->sum_rtt_square_by_cwnd_);
			}
			if (test2 > p->sum_rtt_by_cwnd_){
				printf("-----xcp_enqueue: test(%llu) > srbcwnd(%llu)\n",
				       test,
				       p->sum_rtt_by_cwnd_);
			}
		}
	}
	return; /* mn will enqueue pkt */
}




