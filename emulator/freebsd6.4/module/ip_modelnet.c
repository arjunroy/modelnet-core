/*
 * modelnet  ip_modelnet.c
 *
 *     modelnet emulation pkts between subnet 10.0.0.0/8 addrs
 *
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 *
 */

#define _IP_VHL

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
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


#include "ip_modelnet.h"
#include "mn_xtq.h"
#include "mn_xcp.h"


/* packet_calendar is a circular array of packet lists.  each element
 * lists the packets due to start new hops on that timeslice.   The elements
 * correspond to one clock tick (hz).   It was efficient to size it to a
 * power of 2 to enable masking of calendar_tick to determine the
 * corresponding element in packet_calendar.
 * Usually sized to cover one second of clock ticks.
 */
static struct pktlist *packet_calendar;   /* the packet scheduling array */
/* see ip_modelnet.h for #defines relating to calendar queue */

u_int    calendar_tick = 0;
static struct callout hopclock_callout;

/* A second calendar queue helps out with XTQ */
struct hoplist *hop_calendar;              /* hop scheduling calendar*/

/* Mutex used to protect packet queues */
struct mtx *mn_mtx;

/* stats and debug */
struct mn_error g_error;
u_int32_t       mn_debug_g;
static int      pktcount[60];


extern int      osreldate;
void ip_forward __P((struct mbuf * m, int srcrt,
			struct sockaddr_in * next_hop));

void version_splash(void);

MALLOC_DEFINE(M_MN_PKTS, "MN/Pkts", "Modelnet/Packets");
MALLOC_DEFINE(M_MN_CONF, "MN/Conf", "Modelnet/Pipe Config");
MALLOC_DEFINE(M_MN_ROUTE, "MN/Route", "Modelnet/Pipe Routing");
MALLOC_DEFINE(M_MN_MUTEX, "MN/Mutex", "Modelnet/Mutex");

SYSCTL_NODE(_net_inet_ip, OID_AUTO, modelnet, CTLFLAG_RW, 0, "Modelnet");

SYSCTL_OPAQUE(_net_inet_ip_modelnet, OID_AUTO, pktcount,
              CTLFLAG_RW, &pktcount, sizeof pktcount,
              "LU", "Modelnet stats: packets/second");

SYSCTL_OPAQUE(_net_inet_ip_modelnet, OID_AUTO, mn_error,
              CTLFLAG_RW, &g_error, sizeof(struct mn_error),
              "LU", "Modelnet error stats");



/*
 * [update_bwq] Update virtual bandwidth queue in hop.
 * Bring the length and head position of virtual bandwidth queue up to
 * the current time (calendar_tick).  
 * *note* we don't keep track of bytedepth here, as we don't know
 * packet lengths.  We decrement that when the packet leaves the
 * queue+delay in emulate_nexthop. 
 */
static void
update_bwq(struct hop *hop)
{
    while (hop->slotdepth && hop->exittick[hop->headslot]<calendar_tick) {
        hop->exittick[hop->headslot] = 0;
        hop->slotdepth--;
	hop->bytedepth -= hop->slotlen[hop->headslot];
	hop->slotlen[hop->headslot] = 0;
        if (++hop->headslot == hop->qsize)
            hop->headslot = 0;
    }
}


/*
 * [emulate_hop] Emulate the crossing of a single network link hop.
 * return ENOBUFS if hop drops pkt, 0 otherwise.
 *
 * Emulate link loss rate hop->plr [packet loss rate]
 * Emulate bandwidth queuing:
 *    - update virtual bw queue
 *    - drop packet when bw queue is full
 *    - calculate when packet will exit bw queue (tailexit)
 *    - put packet on tail of notional bw queue by
 *      updating slotdepth and exittick
 * Emulate link latency hop->delay
 * Insert pkt into calendar queue for time (tailexit + hop->delay)
 */
static int
emulate_hop(struct packet *pkt, struct hop *hop)
{
    int newslot = (hop->headslot + hop->slotdepth) % hop->qsize;
    u_int tailexit = calendar_tick;


    /* drop packets for link losses */
    if ((hop->plr && random() < hop->plr)) {
	    /* drop stats? */
        return ENOBUFS;
    }

    if (hop->KBps) {  /* bw delay of 0 means no bw limit */
        update_bwq(hop);
        /* drop packets for queue overflows */
        if (hop->slotdepth >= hop->qsize) {
            ++hop->qdrops;      /* stats */
            return ENOBUFS;
        }
    }

    /* at this point pkt will not be dropped and will cross hop OK */
    mn.stats.pkts_queued++;     /* stats */
    ++hop->pkts;                /* stats */
    hop->bytes += pkt->info.len;        /* stats */


    /* XTQ */
    if (hop->xtq.enqueue) { 
	    Dprintf(("emulate_hop: xtq_enqueue\n"),P_XTQ);
	    hop->xtq.enqueue(hop,pkt);
    }


    /* if hop has queued packets, set tailexit to time the tail packet
     * is scheduled to exit hop.
     * if queue is empty, reset fragment to 0 */
    if (hop->slotdepth)
	    tailexit = hop->exittick[(hop->headslot + hop->slotdepth -1)
		    % hop->qsize];
    else
	    hop->fragment=0;

    /* for hops with bw limits, calculate with integer math how many
     * ticks it will take to transmit the pkt(bwdelay).
     * Use 'fragment' to store how many bytes went unused in the final tick. 
     * These bytes can be used by the next packet.  'fragment' is
     * important for small packets on fast links.
     */
    if (hop->KBps) {
	u_int len = pkt->info.len;
	u_int bwdelay = 0;
	int fragused=0;

	if (hop->fragment>len) {
		hop->fragment -= len;
		len = 0;
	} else {
		if (hop->fragment) {
			len -= hop->fragment;
			fragused=1;
			hop->fragment = 0;
		}
	}

	if (len && hop->bytespertick>50) {
	    int remainder;
            bwdelay = (len / hop->bytespertick) + fragused;
	    remainder = len % hop->bytespertick;
	    if (remainder) {
		hop->fragment = hop->bytespertick - remainder;
	    }
	} else {
            bwdelay = ((u_int) len * (u_int)hz) / (u_int) (1000*hop->KBps);
	}

        if (!bwdelay)
            g_error.delayzero++;

        tailexit += bwdelay;
    }

    /* add packet to virtual queue of this hop */
    ++hop->slotdepth;
    hop->slotlen[newslot] = pkt->info.len;
    hop->bytedepth += pkt->info.len;
    hop->exittick[newslot] = tailexit;

    /* put packet on packet_calendar, to be handled by hopclock() */
#ifdef UNIFIED_PKT_SCHEDULE
    /*
     * Only insert packet once for (bw + delay) ticks 
     */
    pkt->state = Q_BW | Q_DELAY;
    TAILQ_INSERT_TAIL(packet_calendar + ((tailexit + hop->delay) & SCHEDMASK),
		    pkt, list);
#else
    /* Do not add hop delay */
    pkt->state = Q_BW;
    TAILQ_INSERT_TAIL(packet_calendar + ((tailexit) & SCHEDMASK),
		    pkt, list);
#endif

    return 0;
}

/*
 * [forward_packet] called when emulation of all hops in path is complete 
 */
static void
forward_packet(struct packet *pkt)
{
    struct mbuf    *m;

    if (pkt->m == NULL) {
	    /*
	     * return remote packet home when cached 
	     */
	    remote_hop(pkt, pkt->cachehost);	    
	    return;
    }

    if (pkt->cachehost) { /* re-mbuf-headerizing packet */
        MGETHDR(m, M_DONTWAIT, MT_DATA);
        if (!m) {
            /*
             * pkt and pkt->m freed on return 
             */
            printf("forward_packet: unable to allocate mhdr!\n");
            return;
        }
        MN_DUMP_PKT(pkt);
        m->m_pkthdr.rcvif = NULL;       /* unknown */
        m->m_nextpkt = 0;
        m->m_pkthdr.len = pkt->info.len;
        m->m_len = 0;
        m_cat(m, pkt->m);
        m = m_pullup(m, sizeof(struct ip));
        pkt->m = m;
    }

    if (xcp_initialized){  /* see code in filter_ipinput */
	    xcp_set_xcpinfo(pkt);
    }

    Dprintf(("forward_packet: calling ip_forward\n"), MN_P_PKTS);

    ip_forward(pkt->m, 0, NULL);
    /*
     * we lose this mbuf, IP output owns it now 
     */
    pkt->m = NULL;
    pktcount[(ticks / hz) % (sizeof(pktcount) / sizeof(*pktcount))]++;
}



/*
 * [emulate_nexthop]
 *   Called per hop to advance through the hops in the path of the packet.
 *   This is also called when there are no more hops or reuniting a
 *   hdr with it's cached payload.
 */
void
emulate_nexthop(struct packet *pkt)
{
    int             dropped;
    struct hop     *curhop = *pkt->path;
    struct hop     *prvhop = NULL;

    /* DEBUG */
    if (pkt->info.hop==0){ /* first hop */
	    if (curhop->emulator){
		    printf("emulate_nxthop: hop=0, emulator!=NULL, srcip(0x%x) dstip(0x%x)\n", 
			   pkt->info.src,
			   pkt->info.dst);
		    printf("        It is likely that the routing table is corrupt.\n");
		    printf("        Or you're receiving stale VN traffic.\n");
		    /* drop */
		    MN_FREE_PKT(pkt); /* should never be cached at this point */
		    return;
	    }
    }

    /* XTQ: if not on first hop && we're queueing bandwidth */
    if (pkt->info.hop && (pkt->state & Q_BW)){ /* may have Q_DELAY set too */
	    prvhop  = *(pkt->path - 1); /* ptr arithmetic */

	    if (!(pkt->state & Q_DELAY)){ /* Queueing BW & DELAY seperately */
		    if (prvhop->KBps) {
			    update_bwq(prvhop);/* Update picture of the queue */
		    }
		    /*
		     * XTQ dequeue: Do I own the previous hop?  
		     */
		    if (MC_HOP_LOCAL(prvhop) && prvhop->xtq.dequeue){
			    Dprintf(("emulate_nexthop: xtq_dequeue\n"),P_XTQ);
			    prvhop->xtq.dequeue(prvhop,pkt);		    
		    }
		    /* Schedule the delay */
		    pkt->state = Q_DELAY;
		    TAILQ_INSERT_TAIL(packet_calendar + ((calendar_tick + prvhop->delay) & SCHEDMASK),
				      pkt, list);
		    return;
	    } else {                      /* Queueing BW & DELAY together */
		    /* Fudged dequeue behavior.
		     * XTQ dequeue: Do I own the previous hop?  
		     */
		    if (MC_HOP_LOCAL(prvhop) && prvhop->xtq.dequeue){
			    Dprintf(("emulate_nexthop: xtq_dequeue\n"),P_XTQ);
			    prvhop->xtq.dequeue(prvhop,pkt);		    
		    }
	    }    
    }

    /* At this point we're in one of 3 states
     * 1.) first hop, pkt->state = 0
     * 2.) not first hop, pkt->state = Q_BW|Q_DELAY
     *     Finished all delay.
     * 3.) not first hop, pkt->state = Q_DELAY
     *     Finished all delay.
     * Do not need to reset pkt->state.
     */


    /* forward_packet()
     * no more hops -> fwd to edge node
     * no more hops + pcache -> fwd to home core
     * reunited locally -> fwd to edge node
     */

    if (!curhop) {
        forward_packet(pkt);
        MN_FREE_PKT(pkt); /* should never be cached at this point */
        return;
    }

    Dprintf(("emulate_nexthop: pkt(0x%x) hop(%d)\n",
             (int) pkt, (int) pkt->info.hop), MN_P_PKTS);

#if 0
    if (!MC_HOP_LOCAL(curhop)) {
        printf("curhop->id(%d) emulator(%lu.%lu.%lu.%lu)\n",
               curhop->id, IP_ADDR_QUAD_N(curhop->emulator));
    }
#endif


    dropped = curhop->emulator ? remote_hop(pkt, curhop->emulator) :
        emulate_hop(pkt, curhop);

    /* 
     * if it was a remote_hop, then this is a essentially a no-op 
     * if it was a local hop, then it's on the delay queue or dropped
     */ 
    ++pkt->path;
    ++pkt->info.hop;

    if (!(MC_PKT_HOME(pkt) && MC_PKT_PCACHED(pkt)) && (curhop->emulator)) {
        /*
         * if home and cached, then we can't free this pkt, if remote and 
         * not home, then can free 
         */
        MN_FREE_PKT(pkt);
    } else if (dropped) {
        MN_FREE_PKT(pkt);
    }
    return;
}

/*
 * emulate_path
 *   Lookup a path of hops and start
 *   emulated the hops using emulate_nexthop().
 *   
 *   Returns 0 for success, errno otherwise.
 */
static int
emulate_path(struct packet *pkt)
{
    struct ip      *ip;

    ip = mtod(pkt->m, struct ip *);

    /*
     * set the src with the forcebit, if set on dst 
     */
    ip->ip_src.s_addr |= (ip->ip_dst.s_addr & MODEL_FORCEBIT);
    /*
     * clear the destination of forcebit
     */
    ip->ip_dst.s_addr &= ~(MODEL_FORCEBIT);

    pkt->path =
        lookup_path(MODEL_FORCEOFF(ip->ip_src.s_addr), ip->ip_dst.s_addr);


    if (pkt->path == NULL) {
        Dprintf(("emulate_path: no path for packet srcip(0x%x) dstip(0x%x)\n",
		 ip->ip_src.s_addr, ip->ip_dst.s_addr), MN_P_ERR);
        return ENOENT;
    } else {
        Dprintf(("emulate_path: beginning path for packet\n"), MN_P_PKTS);
    }

    if (xcp_initialized){
	    /* There are xcp links in this emulation.
	     * Record XCP info for remote 
	     * packets.  XCP is a special XTQ policy,
	     * b/c it's a different protocol too.
	     */
	    xcp_get_xcpinfo(pkt);
    }

    pkt->info.hop = 0;

    pkt->info.src = ip->ip_src.s_addr;
    pkt->info.dst = ip->ip_dst.s_addr;

    emulate_nexthop(pkt);

    return 0;
}


/* 
 * hopclock - handle all pkts due to start new hops on each quantum
 * This is called from softclock() every hz.  'ticks' is advanced by
 * hardclock so we use calendar_tick which is advanced in this func.
 *
 * Dequeue every packet in list packet_calendar[calendar_tick&SCHEDMASK]
 * and sent it to emulate_nexthop().
 * 
 * The hop_calendar schedules hops that need to maintain timeouts.  
 *
 */
static void
hopclock(void *__unused unused)
{
    struct packet  *pkt;
    struct hop_scheduler *hop_sched;

    mtx_lock(mn_mtx);

    while (calendar_tick != ticks)  {
	int slot = calendar_tick& SCHEDMASK;
        while (!TAILQ_EMPTY(packet_calendar + slot)) {
            pkt = TAILQ_FIRST(packet_calendar + slot);
            TAILQ_REMOVE(packet_calendar + slot, pkt, list);
            mn.stats.pkts_queued--;     /* stats */
            emulate_nexthop(pkt);
        }
        while (!TAILQ_EMPTY(hop_calendar + slot)) {
            hop_sched = TAILQ_FIRST(hop_calendar + slot);
            TAILQ_REMOVE(hop_calendar + slot, hop_sched, list);
            mn.stats.hop_scheduled--;     /* stats */
            hop_sched->timeout(hop_sched->hop);
	    XTQ_FREE_HS(hop_sched);
        }
        ++calendar_tick;
    }

    if (mn.state & MN_AGGREGATE){ 
	    remote_aggregate_clean();
    }
    

    if (ticks % REMOTE_FLUSH_RATE == 0)
        remote_flush();
    
    mtx_unlock(mn_mtx);
    
    //Danny: The timeout/untimeout functions are deprecated in FreeBSD 6.4. They cause kernel panics in VMware
    // hopclock_handle = timeout(hopclock, NULL, 1);
    callout_reset(&hopclock_callout, 1, hopclock, NULL);
}



/*
 * filter_ipinput - catch incoming modelnet packets
 *
 * Called from the ipfw section of ip_input for each IP packet entering host.
 * We want any packet with a src and dst on the MODEL_SUBNET or that are
 * coming from a remote emulator via UDP port 'remote_port'
 * 
 * Danny: New in FreeBSD 6.4: Takes a pointer to the handle of mbuf, instead
 * of just a handle of mbuf (as in 6.4). When DummyNet is done with the mbuf,
 * it frees the mbuf and sets its handle to NULL.
 *
 * Send new packets to emulate_path() and remote to remote_input()
 */
static int
filter_ipinput(struct mbuf **mbuf_handle, int pipe_nr, int dir,
               struct ip_fw_args *fwa)
{
    struct ip      *ip;
    struct packet  *pkt = NULL;
    int             err;
    struct mbuf    *m = *mbuf_handle;

    mtx_lock(mn_mtx);

    ip = mtod(m, struct ip *);
    Dprintf(("filter_ipinput: packet arrived\n"), MN_P_PKTS);

    if (((ip->ip_src.s_addr & MODEL_MASK) == MODEL_SUBNET)
        && ((ip->ip_dst.s_addr & MODEL_MASK) == MODEL_SUBNET)) {
	    pkt = malloc(sizeof(struct packet), M_MN_PKTS, M_NOWAIT);
	    if (!pkt){
		    mtx_unlock(mn_mtx);
		    return ENOBUFS;
	    }
	    mn.stats.pkt_alloc++;
	    pkt->m = m;
	    pkt->m_handle = mbuf_handle;
	    pkt->info.len = m->m_pkthdr.len;
	    m = NULL;
	    pkt->cachehost = 0;
	    pkt->info.id = (unsigned long) NULL;
	    err = emulate_path(pkt);
    } else {
        struct udphdr  *uh;
//        int             iphlen = IP_VHL_HL(ip->ip_vhl) << 2;
        int             iphlen = ip->ip_hl << 2;
        if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct udphdr))) == 0) {
			mtx_unlock(mn_mtx);
			return ENOBUFS;
		}
            ip = mtod(m, struct ip *);
        }
        uh = (struct udphdr *) ((caddr_t) ip + iphlen);
        if (ntohs(uh->uh_dport) == remote_port
            || ntohs(uh->uh_dport) == remote_port) {
            err = remote_input(m);
	    /* remote_input destroys mbuf */
        } else {
            err = EINVAL;
        }
    }

    if (err && pkt) {
	    MN_FREE_PKT(pkt);
    }

	// Danny: DummyNet requires any freed mbufs to have a NULL mbuf handle.
    *mbuf_handle = NULL;

    mtx_unlock(mn_mtx);
    return err;
}



static int
null(void)
{
    return ENOENT;
}


/*
 * modelnet_modevent - install modelnet routines, allocate/free structs
 *
 * install hopclock() into softclock with timeout().
 * install filter_ipinput() on the ipfw ip_dn_ip_ptr hook in ip_input().
 *
 */
static int
modelnet_modevent(module_t mod, int type, void *data)
{
    int i;
    
    switch (type) {
    case MOD_LOAD:
        mn_mtx = (struct mtx *) malloc(sizeof(struct mtx), M_MN_MUTEX, M_NOWAIT);
	
	if (!mn_mtx) {
	    printf("Out of memory. Can't allocate mutex.\n");
	    return ENOMEM;
	}
	mtx_init(mn_mtx, "ModelNet Mutex", NULL, MTX_DEF);
	
        mtx_lock(mn_mtx);
        if (ip_dn_io_ptr) {
    	    mtx_unlock(mn_mtx);
            printf("Modelnet already loaded\n");
            return EEXIST;
        }
        ip_dn_io_ptr = (void *) filter_ipinput;

        ip_dn_ctl_ptr = (void *) null;
        ip_dn_ruledel_ptr = (void *) null;

		callout_init(&hopclock_callout, 0);
        bzero(&g_error, sizeof(struct mn_error));
        mn_debug_g = 0;/* MN_P_PKTS|MN_P_ERR;         printfs we want */


	calendar_tick = ticks;
		callout_reset(&hopclock_callout, 1, hopclock, NULL);
        packet_calendar = (struct pktlist *) malloc(sizeof(*packet_calendar) * SCHEDLEN,
                                              M_MN_PKTS, M_NOWAIT);
        hop_calendar = (struct hoplist *) malloc(sizeof(*hop_calendar) * SCHEDLEN,
                                              M_MN_PKTS, M_NOWAIT);
        for (i = 0; i < SCHEDLEN; ++i) {
            TAILQ_INIT(packet_calendar + i);
        }
        for (i = 0; i < SCHEDLEN; ++i) {
            TAILQ_INIT(hop_calendar + i);
        }

        remote_init();
	xtq_init(); 
	
        mtx_unlock(mn_mtx);

	version_splash();

        break;

    case MOD_UNLOAD:
#if !defined(KLD_MODULE)
        printf("modelnet statically compiled, cannot unload\n");
        return EINVAL;
#else
        mtx_lock(mn_mtx);

		callout_stop(&hopclock_callout);
        xtq_uninit();
	remote_uninit();
        uninit_paths();
        free(hop_calendar, M_MN_PKTS);
        free(packet_calendar, M_MN_PKTS);
        printf("Modelnet uninstalled.\n");
        ip_dn_ctl_ptr = NULL;
        ip_dn_io_ptr = NULL;

        mtx_unlock(mn_mtx);
	mtx_destroy(mn_mtx);
	free(mn_mtx, M_MN_MUTEX);
#endif
        break;
    default:
        break;
    }
    return 0;
}

static moduledata_t modelnet_mod = {
    "modelnet",
    modelnet_modevent,
    NULL
};
DECLARE_MODULE(modelnet, modelnet_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(modelnet, ipfw, 1, 1, 2);
MODULE_VERSION(modelnet, 1);
