/*
 * modelnet  mn_xtq.h
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

#ifndef _MN_XTQ_H_
#define _MN_XTQ_H_

#define NUM_HOP_SCHEDS 5000 /* allocation block size of hops scheds*/
#define MAX_ALLOCATIONS 10  /* max 50,000 pipe timeouts scheduled concurrently*/


extern struct hoplist   free_hop_scheds;
extern struct extended_q qsw[];


/* --- Xtended queue disciplines --- */
#define XTQ_NOXTQ 0
#define XTQ_XCP 1
#define XTQ_MAX 256


/* Allocation/Deallocation macros */
#define XTQ_GET_THING(new_thing, free_thing_list, create_func,thing_entry) { \
    while(1){\
        if (TAILQ_EMPTY(&free_thing_list)) { \
            create_func(); \
            if (TAILQ_EMPTY(&free_thing_list)) { \
               new_thing = NULL;\
               break;\
            }\
        }\
        new_thing = TAILQ_FIRST(&free_thing_list); \
        TAILQ_REMOVE(&free_thing_list, new_thing, thing_entry); \
        break;\
    }\
}

#define XTQ_GET_HS(new_hs) {\
    XTQ_GET_THING(new_hs, free_hop_scheds, xtq_create_hop_scheds, free); \
}

#define XTQ_FREE_HS(new_hs) {\
    bzero(new_hs,sizeof(struct hop_scheduler)); \
    TAILQ_INSERT_TAIL(&free_hop_scheds, new_hs, free); \
}

/* Hop scheduling macros */ 
#define XTQ_SCHEDULE_HOP(local_hop,func,delay) {\
     hop_scheduler_t hs;\
     XTQ_GET_HS(hs);\
     if (!hs) {\
          printf("XTQ_SCHEDULE_HOP: unable to schedule!\n");\
     } else {\
          mn.stats.hop_scheduled++;\
          hs->timeout = func;\
          hs->hop = local_hop;\
     TAILQ_INSERT_TAIL(hop_calendar+((calendar_tick+delay)&SCHEDMASK),hs,list);\
     }\
} 


void xtq_init(void);
void xtq_uninit(void);
void xtq_create_hop_scheds(void);
void xtq_unschedule_hop(struct hop *h);
void xtq_install(struct hop *hop, int xtq_type);
void xtq_uninstall(struct hop *hop, int xtq_type);
void L4_tcp_cksum(struct mbuf *m);
void L4_acp_cksum(struct mbuf *m);








#endif // _MN_XTQ_H_
