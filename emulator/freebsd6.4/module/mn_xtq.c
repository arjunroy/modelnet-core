/*
 * modelnet  mn_xtq.c
 * Duke University copyright(c) 2003
 * See COPYING for license statement.
 * Kenneth G. Yocum
 * 
 * modelnet eXTended queues
 *
 * The basic modelnet queue is a FIFO.  In ModelNet packets 
 * 1.) arrive, may be dropped, are "enqueued", and then scheduled
 * 2.) packets depart
 * 3.) when packets are enqueued the queue is updated, and all packets
 *     which have left in the intervening time are "dequeued."
 * Thus currently packets are not dequeued at the time at which they
 * actually exit the core node.
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
#include <netinet/tcpip.h>
#include <machine/in_cksum.h>
 /* end kernel includes */
#endif

#include "ip_modelnet.h"
#include "mn_xtq.h"
#include "mn_xcp.h"


static u_int32_t total_hop_scheds=0;
struct hoplist   free_hop_scheds;
static hop_scheduler_t  allocated_blocks[MAX_ALLOCATIONS];
static int num_qsw_entries = 0;



/* the queue switch structure contains all the definitions for the
 * currently supported queueing disciplines.  It is similar to the
 * protocol switches found in bsd-style systems.  
 * At the moment though the index into this array is hard coded. In
 * the future we could initialize, discover the index, place in a
 * seperate array indexed by queue_id, which gives us the index into
 * this array.
 */
static struct extended_q qsw[] = {
	{ XTQ_NOXTQ, "NO_XTQ", 0, 
	  NULL, NULL, NULL, NULL,
	  NULL
	},
	{ XTQ_XCP, "XCP", 0, 
	  xcp_alloc_pipe, 
	  xcp_free_pipe, 
	  xcp_enqueue, 
	  xcp_dequeue,
	  NULL
	},
	{ XTQ_MAX, "NULL", 0, 
	  NULL, NULL, NULL, NULL,
	  NULL
	}
};



/* [xtq_init]
 * allocate some hop schedulers
 */
void 
xtq_init(void)
{
	int i;
	
	bzero(allocated_blocks,sizeof(hop_scheduler_t)*MAX_ALLOCATIONS);
	xtq_create_hop_scheds();
	for (i = 0; i < XTQ_MAX; i++){
		if (qsw[i].type == XTQ_MAX){
			break;
		}
		num_qsw_entries+=1;
	}
}

/* [xtq_uninit]
 * clean up hop scheduler
 */
void
xtq_uninit(void)
{
	int i;
	for(i=0;i<MAX_ALLOCATIONS;i++){
		if (allocated_blocks[i]){
			free(allocated_blocks[i], M_MN_PKTS);
		}
	}
}


/* [xtq_create_hop_scheds]
 * Allocate a block of hop schedulers
 */
void
xtq_create_hop_scheds(void)
{
	int i;
	hop_scheduler_t new_hs;
	static int allocate_idx=0;
	
	if(allocate_idx==MAX_ALLOCATIONS){
		printf("xtq_create_hop_scheds: maximum allocated!\n");
		return;
	}
	if (!(MALLOC(new_hs,hop_scheduler_t,
		sizeof(struct hop_scheduler)*NUM_HOP_SCHEDS, 
		M_MN_PKTS, M_NOWAIT)))
	{
		printf("xtq_create_hop_scheds: failed malloc\n");
		return;
	}
	allocated_blocks[allocate_idx] = new_hs;
	allocate_idx++;
	total_hop_scheds+= NUM_HOP_SCHEDS;
	bzero(new_hs, sizeof(struct hop_scheduler) * NUM_HOP_SCHEDS);
	for (i = 0; i < NUM_HOP_SCHEDS; i++){
		TAILQ_INSERT_HEAD(&free_hop_scheds, &(new_hs[i]),
				  free);
	}
}


/* [xtq_install]
 * Allocate the extended queue hop.
 */
void 
xtq_install(struct hop *hop, int xtq_type)
{
	/* struct copy */
	if (xtq_type < 0  || xtq_type > num_qsw_entries){
		printf("xtq_install: xtq_type(%d) unknown, %d installed\n", 
		       xtq_type, num_qsw_entries);
		hop->xtq = qsw[num_qsw_entries-1]; /* XTQ_NOXTQ */
		return;
	}
	hop->xtq = qsw[xtq_type];
	Dprintf(("xtq_install: installing of type %s\n", qsw[xtq_type].name),P_XTQ);
	if(hop->xtq.alloc){			   
		hop->xtq.alloc(hop); /* may init xtq pkg too */
	}
	return;
}

/* [xtq_uninstall]
 * Free or reinstall
 */
void 
xtq_uninstall(struct hop *hop, int xtq_type)
{
	if(hop->xtq.free){ /* throw out the old */
		hop->xtq.free(hop); 
	}
	if (xtq_type >= 0){
		xtq_install(hop,xtq_type);
	}
	return;
}

/* [xtq_unschedule_hop]
 * Expensive unregister from the 
 * hopCQ scheduler calendar queue.
 * XXX linked list of hop_scheds per pipe would make this faster.
 * 
 */
void
xtq_unschedule_hop(struct hop *h)
{
	int i;
	hop_scheduler_t hs, nxt_hs;
	
	for (i = 0; i < SCHEDLEN; i++){
		TAILQ_FOREACH(nxt_hs, (hop_calendar+i), list){
			if ((u_int32_t)nxt_hs->hop == (u_int32_t)h){
				hs = nxt_hs; /* current */
				nxt_hs = TAILQ_NEXT(nxt_hs,list);
				TAILQ_REMOVE((hop_calendar+i), hs, list);	
				mn.stats.hop_scheduled--;
				XTQ_FREE_HS(hs);
				if (nxt_hs){continue;} else {break;}
				
			}
		}
	}
}

/* [L4_tcp_cksum]
 *
 * Utility function to rewrite TCP checksum
 * for updates.   
 */
void
L4_tcp_cksum(struct mbuf *m)
{
        struct tcpiphdr *tcp = mtod(m, struct tcpiphdr *);
	int len;
	
	if (m->m_pkthdr.csum_flags != CSUM_TCP) {
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
	}
	len = m->m_pkthdr.len - sizeof(struct tcpiphdr);
	tcp->ti_sum = in_pseudo(
		tcp->ti_src.s_addr, tcp->ti_dst.s_addr,
		htons((u_short)len + sizeof(struct tcphdr) + IPPROTO_TCP));
}

/* [L4_acp_cksum
 *
 * Utility function to rewrite ACP checksum
 * This depends on the driver supporting checksum offload.
 * in particular it depends on the driver supporting ACP cksum
 * offload!
 * 
 * in ip_output.c
 * So the interface sets up what protocols it will offload the cksum
 * for. And then ip_output will clear those bits:
 *  
 sw_csum = m->m_pkthdr.csum_flags & ~ifp->if_hwassist;
 if (sw_csum & CSUM_DELAY_DATA) 
      in_delayed_cksum(m);
 *
 * So b/c the modelnet core is unmodified, CSUM_DELAY_DATA only
 * contains  CSUM_TCP|CSUM_UDP, not CSUM_ACP, so 
 * this doesn't get called.  Instead I'll try to do it here.
 */
void
L4_acp_cksum(struct mbuf *m)
{
        struct acpiphdr *acp = mtod(m, struct acpiphdr *);
	int len;
	
	if (m->m_pkthdr.csum_flags != CSUM_ACP) {
		m->m_pkthdr.csum_flags = CSUM_ACP;
		m->m_pkthdr.csum_data = offsetof(struct acphdr, th_sum);
	}
	len = m->m_pkthdr.len - sizeof(struct acpiphdr);
	acp->ti_sum = in_pseudo(
		acp->ti_src.s_addr, acp->ti_dst.s_addr,
		htons((u_short)len + sizeof(struct acphdr) + IPPROTO_TUF));

	if (!(CSUM_DELAY_DATA & CSUM_ACP)){
		/*printf("L4_acp_cksum: looks like a kernel without ACP support!\n");*/
		in_delayed_cksum(m);
	}
}

