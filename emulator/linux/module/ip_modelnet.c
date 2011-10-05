/* 
 * modelnet  ip_modelnet.c
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

#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>

#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/random.h>

#include <linux/vmalloc.h>
#include <linux/workqueue.h>	/* We scheduale tasks here */
#include <linux/sched.h>	/* We need to put ourselves to sleep  */


/* #define _IP_VHL */
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#ifdef MN_TCPDUMP
#include "mn_tcpdump.h"
#endif

#include "ip_modelnet.h"
#include "mn_pathtable.h"


/* keep hopclock from queueing itself */
#define MN_WORKQUEUE_NAME "mnwq"
static int modelnet_die = 0;
static void hopclock(struct work_struct *);
static struct workqueue_struct *modelnet_workqueue;
static DECLARE_DELAYED_WORK(hopclock_task, hopclock);

/* better if calendar_tick were atomic_t, but it needs to be unsigned long */
DEFINE_SPINLOCK(calendar_lock);


/* Hopefully we are doing this right and this is the hook to
   resend our packets */
int (*ip_rcv_finish_hook)(struct sk_buff *) = NULL;


/* packet_calendar is a circular array of packet lists.  each element
 * lists the packets due to start new hops on that timeslice.   The elements
 * correspond to one clock tick (hz).   It was efficient to size it to a
 * power of 2 to enable masking of calendar_tick to determine the
 * corresponding element in packet_calendar.
 * Usually sized to cover one second of clock ticks.
 */
static pktlist *packet_calendar;   /* the packet scheduling array */

/* see ip_modelnet.h for #defines relating to calendar queue */

unsigned long    calendar_tick = 0;

/* static struct callout_handle hopclock_handle; */

/* /\* A second calendar queue helps out with XTQ *\/ */
/* struct hoplist *hop_calendar;              /\* hop scheduling calendar*\/ */

/* stats and debug */
struct mn_error g_error;
u_int32_t       mn_debug_g;
static int      pktcount[60];

/* Random Number Generation */
static int mti;   
static u_int mt[MERS_N];


/* extern int      osreldate; */

/* void version_splash(void); */



#ifdef MODELNET_PROFILE
/* 
   get_hr_time() - reads the rdtsc (read time stamp counter or
   something) register and returns the number of cpu ticks
 */

unsigned long long get_hr_time( void ) 
{
    unsigned long long a;
  
    unsigned int vlo, vhi;
    //asm volatile ("rdtsc":"=A" (a));
  
    asm volatile("rdtsc":"=a"(vlo),"=d"(vhi));
    //a = vhi << 32 + vlo;
    a = vhi;
    a = a << 32 | vlo;
    return a;
}
#endif


/*
 * [forward_packet] called when emulation of all hops in path is complete
 */
static void forward_packet(struct packet *pkt)
{
/*   struct mbuf    *m; */
    //printk("forward_packet: doing [%p]\n", pkt);
    if (pkt->skb == NULL) 
    {
	/*
	 * return remote packet home when cached
	 */
	/* XXX remote not implemented yet */
	//printk("forward_packet: doing remote hop for [%p]\n", pkt);
	remote_hop(pkt, pkt->cachehost);    
	return;
    }
    
    struct  iphdr *ip;
    ip = ip_hdr(pkt->skb);
    //printk("forward_packet: Pkt [%p] IP SRC %d.%d.%d.%d IP DST %d.%d.%d.%d\n", pkt, NIPQUAD(ip->saddr), NIPQUAD(ip->daddr));

    //printk("forward_packet: Calling ip rcv finish hook on pkt [%p]\n", pkt);
    ip_rcv_finish_hook(pkt->skb);

    /*
     * we lose this skb, IP output owns it now
     */
    pkt->skb = NULL;
    pktcount[(jiffies / HZ) % (sizeof(pktcount) / sizeof(*pktcount))]++;
}



/*
 * [update_bwq] Update virtual bandwidth queue in hop.
 * Bring the length and head position of virtual bandwidth queue up to
 * the current time (calendar_tick).
 * *note* we don't keep track of bytedepth here, as we don't know
 * packet lengths.  We decrement that when the packet leaves the
 * queue+delay in emulate_nexthop.
 */
static void update_bwq(struct hop *hop, unsigned long tick)
{
    while (hop->slotdepth && hop->exittick[hop->headslot]<tick) 
    {
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

static int emulate_hop(struct packet *pkt, struct hop *hop, int needlock)
{
    int newslot;
    unsigned long tailexit, curtick;
  
    /* This is tcpdump stuff */
    int willDropPacket_plr = 0;
    int willDropPacket_bw = 0;
    long randomVal = random_bits();

    if (needlock)
        spin_lock_bh(&calendar_lock);
    curtick = calendar_tick;
    if (needlock)
        spin_unlock_bh(&calendar_lock);

    spin_lock_bh(&hop->lock);

    newslot = (hop->headslot + hop->slotdepth) % hop->qsize;

    /* drop packets for link losses */
    /* get_random_bytes returns a value between 0 and
     * (2**32)-1, whereas random() on a bsd machine will return
     * (2**31)-1 ... I zero out the top most bit just in case as it can
     * do no harm.
     */
    if ((hop->plr && ((randomVal & 0x7fffffff) < hop->plr))) {
	/* drop stats? */
	willDropPacket_plr = 1;
    }

    if (hop->KBps) {  /* bw delay of 0 means no bw limit */
        /* pass saved calendar_tick value to avoid another lock/unlock */
	update_bwq(hop, curtick);
	/* drop packets for queue overflows */
	if (hop->slotdepth >= hop->qsize) {
	    willDropPacket_bw = 1;
	    ++hop->qdrops;      /* stats */
	}
    }

#ifdef MN_TCPDUMP
    /* XXX needs lock */
    handle_tcpdump(pkt, hop, willDropPacket_plr, willDropPacket_bw);
#endif

    /* If we already calculated to drop the packet, do it now */
    if (willDropPacket_plr || willDropPacket_bw) {
        spin_unlock_bh(&hop->lock);
	return ENOBUFS;
    }

#if 0
    /* XXX needs to be locked or atomic, but it's not used anyway */
    mn.stats.pkts_queued++;     /* stats */
#endif
    ++hop->pkts;                /* stats */
    hop->bytes += pkt->info.len;        /* stats */

    /* if hop has queued packets, set tailexit to time the tail packet
     * is scheduled to exit hop.
     * if queue is empty, use the current time and reset fragment to 0 
     */
    if (hop->slotdepth)
	tailexit = hop->exittick[(hop->headslot + hop->slotdepth -1)
				 % hop->qsize];
    else {
	tailexit = curtick;
	hop->fragment=0;
    }

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
	    u_int randNum;

	    bwdelay = (len / hop->bytespertick) + fragused;	    
	    remainder = len % hop->bytespertick;
      
	    randNum = randomVal % hop->bytespertick;
	    if (randNum < remainder) {
		bwdelay++;
	    }
	} else {
	    bwdelay = ((u_int) len * (u_int)HZ) / (u_int) (1000*hop->KBps);
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

#ifdef UNIFIED_PKT_SCHEDULE
    /*
     * Only insert packet once for (bw + delay) ticks
     */
    pkt->state = Q_BW | Q_DELAY;
    tailexit += hop->delay;
#else
    /* Do not add hop delay */
    pkt->state = Q_BW;
#endif

    spin_unlock_bh(&hop->lock);

    /* if packet is ready now, send it on its way */
    if (tailexit == curtick)
        forward_packet(pkt);
    else {
        if (needlock)
            spin_lock_bh(&calendar_lock);
        /* put packet on packet_calendar, to be handled by hopclock() */
        list_add_tail(&(pkt->list),
            &packet_calendar[(tailexit & SCHEDMASK)].list);
        if (needlock)
            spin_unlock_bh(&calendar_lock);
    }

    return 0;
}


/*
 * [emulate_nexthop]
 *   Called per hop to advance through the hops in the path of the packet.
 *   This is also called when there are no more hops or reuniting a
 *   hdr with its cached payload.
 *
 *   Note: this function is only called by hopclock and emulate_path.
 *   Interrupts should be disabled when we enter this function.
 */
void emulate_nexthop(struct packet *pkt, int needlock)
{
    int             dropped;
    struct hop     *curhop = *pkt->path;

    //printk("emulate_nexthop: packet is [%p] hop is %d\n", pkt, pkt->info.hop);

/*   struct hop     *prvhop = NULL; */
  
    /* DEBUG */
    if (pkt->info.hop==0){ /* first hop */
	if (curhop->emulator){
	    printk("emulate_nxthp: hop=0, emulator!=NULL, srcip(0x%x) dstip(0x%x)\n",
		   pkt->info.src,
		   pkt->info.dst);
	    printk("        It is likely that the routing table is corrupt.\n");
	    printk("        Or you're receiving stale VN traffic.\n");
      
	    /* drop */
	    MN_FREE_PKT(pkt); /* should never be cached at this point */
	    return;
	}
        else {
	    //printk("emulate_nxthp: first hop, and curhop->emulator is not set for pkt [%p].\n", pkt);
	}
    }
  
  /* XXX XTQ stuff not implemented */

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

    if (!curhop) 
    {
        //printk("emulate_nexthop: no curhop, so call forward_packet for [%p]\n", pkt);
	forward_packet(pkt);
	MN_FREE_PKT(pkt); /* should never be cached at this point */
	return;
    }

    /* Dprintf(("emulate_nexthop: pkt(0x%x) hop(%d)\n", */
/* 	   (int) pkt, (int) pkt->info.hop), MN_P_PKTS); */

#if 0
    if (!MC_HOP_LOCAL(curhop)) {
	printk("curhop->id(%d) emulator(%lu.%lu.%lu.%lu)\n",
	       curhop->id, IP_ADDR_QUAD_N(curhop->emulator));
    }
#endif

    dropped = curhop->emulator ? remote_hop(pkt, curhop->emulator) :
	emulate_hop(pkt, curhop, needlock);

    //printk("Called %s for packet [%p], result dropped=%d\n", (curhop->emulator? "remote hop" : "emulate hop"), pkt, dropped);

    /*
     * if it was a remote_hop, then this is a essentially a no-op
     * if it was a local hop, then it's on the delay queue or dropped
     */
    ++pkt->path;
    ++pkt->info.hop;

    if (!(MC_PKT_HOME(pkt) && MC_PKT_PCACHED(pkt)) && (curhop->emulator)) {
	printk("emulate_nexthop: ERROR!  I should not be here\n");
	/*
	 * if home and cached, then we can't free this pkt, if remote and
	 * not home, then can free
	 */
	MN_FREE_PKT(pkt);
    } else if (dropped) {
        //printk("emulate_nexthop: dropped so free\n");
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
static int emulate_path(struct packet *pkt)
{
    struct  iphdr *ip;

    ip = ip_hdr(pkt->skb);

    //printk("emulate_path: Pkt [%p] IP SRC %d.%d.%d.%d IP DST %d.%d.%d.%d\n", pkt, NIPQUAD(ip->saddr), NIPQUAD(ip->daddr));

    /*
     * set the src with the forcebit, if set on dst
     */
    ip->saddr |= (ip->daddr & MODEL_FORCEBIT);
  
    //printk("emulate_path: Pkt [%p] Did set SRC to %d.%d.%d.%d\n", pkt, NIPQUAD(ip->saddr));

    /*
     * clear the destination of forcebit
     */
    ip->daddr &= ~(MODEL_FORCEBIT);

    //printk("emulate_path: Pkt [%p] Did set DST to %d.%d.%d.%d\n", pkt, NIPQUAD(ip->daddr));

    pkt->path =
	lookup_path(MODEL_FORCEOFF(ip->saddr), ip->daddr);


    if (pkt->path == NULL) {
/*     Dprintf(("emulate_path: no path for packet srcip(0x%x) dstip(0x%x)\n",ip->ip_src.s_addr, ip->ip_dst.s_addr), MN_P_ERR); */
	//printk("emulate_path: no path for packet srcip(0x%x) dstip(0x%x)\n",
	       //ip->saddr, ip->daddr);
	return ENOENT;
    } else {
	/* Dprintf(("emulate_path: beginning path for packet\n"), MN_P_PKTS); */
	//printk("emulate_path: beginning path for packet [%p]\n", pkt);
    }


    pkt->info.hop = 0;

    pkt->info.src = ip->saddr;
    pkt->info.dst = ip->daddr;

    emulate_nexthop(pkt, 1);
    //printk("emulate_path: returning success for packet [%p]\n", pkt);
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
static void hopclock(struct work_struct *irrelevant)
{
    struct packet  *pkt;  
    struct list_head *pos, *q;

#ifdef MN_TCPDUMP 
    /* XXX needs lock */
    /* modelnet tcpdump stuff.  If a second has gone by, then tell
     * tcpdump to swap some buffers*/
    tcpdump_tick_counter++;
    if (tcpdump_tick_counter >= HZ) {
	tcpdump_handle_inactivity();
    }          
#endif

    /* XXX should use per-slot locks */
    spin_lock_bh(&calendar_lock);
    while (time_is_before_jiffies(calendar_tick)) {
	int slot = calendar_tick& SCHEDMASK;
/*     while (!TAILQ_EMPTY(packet_calendar + slot)) { */
/*       pkt = TAILQ_FIRST(packet_calendar + slot); */
/*       TAILQ_REMOVE(packet_calendar + slot, pkt, list); */
/*       mn.stats.pkts_queued--;     /\* stats *\/ */
/*       emulate_nexthop(pkt); */
/*     } */        

	list_for_each_safe (pos, q, &(packet_calendar[slot].list)) {
	    pkt = list_entry(pos, struct packet, list);
	    list_del(pos);
	    mn.stats.pkts_queued--;     /* stats */
	    emulate_nexthop(pkt, 0);
	}
	
	++calendar_tick;
    }
    spin_unlock_bh(&calendar_lock);

    if (!modelnet_die)
	queue_delayed_work(modelnet_workqueue, &hopclock_task, 1);
}



/*
 * filter_ipinput - catch incoming modelnet packets
 *
 * Called from the ipfw section of ip_input for each IP packet entering host.
 * We want any packet with a src and dst on the MODEL_SUBNET or that are
 * coming from a remote emulator via UDP port 'remote_port'
 *
 * Send new packets to emulate_path() and remote to remote_input()
 */

/* static int */
/* filter_ipinput(struct mbuf *m, int pipe_nr, int dir, */
/*                struct ip_fw_args *fwa) */

static unsigned int filter_ipinput(unsigned int hooknum,
				   struct sk_buff *skbuff,
				   const struct net_device *in,
				   const struct net_device *out,
				   int (*okfn)(struct sk_buff *))
{
    struct packet  *pkt = NULL;  
    struct iphdr  *iph;
    unsigned int netfilterResult = NF_ACCEPT;
    int err = 0;

    //printk("modelnet ipfilter: Query Modelnet packet?\n");
  
    if (!skbuff) return NF_ACCEPT;
    iph = ip_hdr(skbuff);
    if (!iph) return NF_ACCEPT;

    //printk("modelnet ipfilter: basic sanity passed\n");
  
    if (((iph->saddr & MODEL_MASK) == MODEL_SUBNET) 
	&& ((iph->daddr & MODEL_MASK) == MODEL_SUBNET)) {
        
	/* XXX this should be locked, but it's hardly critical */
	if (ip_rcv_finish_hook == NULL) 
	    ip_rcv_finish_hook = okfn;
	else if (ip_rcv_finish_hook != okfn)
	{
	    printk("filter_ipinput: SOMETHING IS WEIIIIRD!  differnet okfns\n");
	    ip_rcv_finish_hook = okfn;
	}        

	pkt = kmalloc(sizeof(struct packet), GFP_ATOMIC);


	if (!pkt) {
	    //printk ("filter_ipinput: ERROR: could not allocate packet\n");
	    return NF_ACCEPT;      
	}
        
#if 0
	/* XXX this would be made atomic, if it was ever used anywhere... */
	mn.stats.pkt_alloc++;
#endif
	pkt->skb = skbuff;
	pkt->info.len = skbuff->len;

	pkt->cachehost = 0;
	pkt->info.id = 0;
    
	err = emulate_path(pkt); /* emulate_path returns 0 on success */

	if (err != 0) {
	    //printk ("filter_ipinput: Error with emulate path\n");
	    netfilterResult = NF_ACCEPT;
	} else {
	    //printk("modelnet ipfilter: stolen packet! [%p]\n", pkt);
	    netfilterResult = NF_STOLEN; 
	}
    } else {
	/* No remote stuff */
        //printk("modelnet ipfilter: no remote stuff\n");
    }

    if (netfilterResult == NF_ACCEPT) {
        //printk("modelnet ipfilter: check if packet copy should be freed before washing hands off of it");
	if (pkt != NULL) {
	    pkt->skb = NULL;
	    MN_FREE_PKT(pkt);
	    //printk("modelnet ipfilter: freed packet");
	}
    }

    //printk("modelnet ipfilter: returning %s\n", (netfilterResult == NF_ACCEPT ? "NF_ACCEPT" : "NF_STOLEN"));
    return netfilterResult;
}



/* Random Number Generation Functions */
void random_seed(u_int seed) {
    /* re-seed generator */
    mt[0]= seed;
    for (mti=1; mti < MERS_N; mti++) {
	mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
    }
}

u_int random_bits(void) {
    /* generate 32 random bits */
    u_int y;
  
    if (mti >= MERS_N) {
	// generate MERS_N words at one time
	const u_int LOWER_MASK = (1LU << MERS_R) - 1; // lower MERS_R bits
	const u_int UPPER_MASK = -1L  << MERS_R;      // upper (32 - MERS_R) bits
	static const u_int mag01[2] = {0, MERS_A};
    
	int kk;
	for (kk=0; kk < MERS_N-MERS_M; kk++) {    
	    y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
	    mt[kk] = mt[kk+MERS_M] ^ (y >> 1) ^ mag01[y & 1];}

	for (; kk < MERS_N-1; kk++) {    
	    y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
	    mt[kk] = mt[kk+(MERS_M-MERS_N)] ^ (y >> 1) ^ mag01[y & 1];}      

	y = (mt[MERS_N-1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
	mt[MERS_N-1] = mt[MERS_M-1] ^ (y >> 1) ^ mag01[y & 1];
	mti = 0;}
  
    y = mt[mti++];
  
    // Tempering (May be omitted):
    y ^=  y >> MERS_U;
    y ^= (y << MERS_S) & MERS_B;
    y ^= (y << MERS_T) & MERS_C;
    y ^=  y >> MERS_L;
    return y;
}

/*
 * modelnet_modevent - install modelnet routines, allocate/free structs
 *
 * install hopclock() into softclock with timeout().
 * install filter_ipinput() on the ipfw ip_dn_ip_ptr hook in ip_input().
 *
 * Linux version! - make sure modelnet_load is called BEFORE
 * interrupts are disabled
 */

static
int modelnet_load(void)
{
  
    int i;
  

    /* s = splnet(); */
 
    /* XXX Register with iptables here */
  
    /* XXX Register timer interrupts ...
       We decided to use work queues ... */
/*     bzero(&hopclock_handle, sizeof(struct callout_handle)); */
/*     hopclock_handle = timeout(hopclock, NULL, 1); */

  
    memset(&g_error, 0, sizeof(struct mn_error));  

    mn_debug_g = 0;/* MN_P_PKTS|MN_P_ERR;         printfs we want */

  
    calendar_tick = jiffies;

    packet_calendar = (pktlist *) 
	vmalloc(sizeof(*packet_calendar) * SCHEDLEN);
  
    if (!packet_calendar) {
	printk("Could not allocate packet calendar\n");
	return ENOMEM;
    }
/*     hop_calendar = (struct hoplist *) malloc(sizeof(*hop_calendar) * SCHEDLEN, */
/* 					     M_MN_PKTS, M_NOWAIT); */
    for (i = 0; i < SCHEDLEN; ++i) {
	INIT_LIST_HEAD(&(packet_calendar[i].list));
    }
  
  
/*     version_splash(); */

#ifdef MN_TCPDUMP
    init_mn_tcpdump_buffers();
#endif
    printk(KERN_INFO "Modelnet installed.\n");
    return 0;
}



static int modelnet_unload(void)
{
    uninit_paths();

/*         free(hop_calendar, M_MN_PKTS); */  
/*         free(packet_calendar, M_MN_PKTS); */
    vfree(packet_calendar);
  
    printk(KERN_INFO "Modelnet uninstalled.\n");

    return 0;
}



/* Linux stuff after here */
MODULE_DESCRIPTION("Modelnet for Linux");
MODULE_AUTHOR("Calvin Hubble and Marti Motoyama");
MODULE_LICENSE("GPL");



#define MODELNET_CTL_ID 500


static ctl_table modelnet_inner_table[] = { 
  
    /* Add hopcount (see mn_pathtable.h) */
    {
	.procname = "hopcount",
	.data = &hopcount,
	.maxlen = sizeof(int),
	.mode = 0444, /* Read-only */
	.child = NULL,
	.proc_handler = &proc_dointvec,
    },
    {
	.procname = "nodetable",
	.data = NULL,
	.maxlen = 0,
	.mode = 0222, /* Write-only */
	.child = NULL,
	/* .proc_handler = &proc_dointvec, */
	.proc_handler = &proc_nodetable, 
    },
    {
	.procname = "nodecount",
	.data = &nodecount,
	.maxlen = sizeof(nodecount),
	.mode = 0666, /* read/write */
	.child = NULL,
	.proc_handler = &proc_nodecount,
    },
    {  /* should only write to hoptable, not read */
	.procname = "hoptable",
	.data = NULL,
	.maxlen = 0,
	.mode = 0222, /* write only */
	.child = NULL,
	.proc_handler = &proc_hophandle,
    },
    {
	.procname = "pathentry",
	.data = NULL,
	.maxlen = 0,
	.mode = 0222, /* write only */
	.child = NULL,
	.proc_handler = &proc_pathentry,
    },
    {
	.procname = "hopstats",
	.data = NULL,
	.maxlen = 0,
	.mode = 0444, /* read only */
	.child = NULL,
	.proc_handler = &proc_hopstats,
    },  
#ifdef MODELNET_PROFILE
    {
	.procname = "profile_data",
	.data = profile_string,
	.maxlen = 4096,
	.mode = 0444, /* read only */
	.child = NULL,
	.proc_handler = &proc_profiledata,
    },
    {
	.procname = "hz_samples",
	.data = hz_samples,
	.maxlen = NUM_HZ_SAMPLES * sizeof(unsigned int),
	.mode = 0444, /* read only */
	.child = NULL,
	.proc_handler = &proc_hzsamples,
    },
#endif
#ifdef MN_TCPDUMP
    {
	.procname = "tcpdump",
	.data = 0,
	.maxlen = 0,
	.mode = 0,
	.child = mn_tcpdump_table,
    },
#endif
    {0}
};

static ctl_table modelnet_ctl_table[] = {
    {
	.procname = "modelnet", 
	.data = 0,
	.maxlen = 0,
	.mode = 0555, 
	.child = modelnet_inner_table, 
    }, 
    {0}
};

static struct ctl_table_header *my_table_header = NULL;


static struct nf_hook_ops nfho = {
    .hook = filter_ipinput,
    .owner = THIS_MODULE,
    .pf = PF_INET,
    .hooknum = NF_INET_PRE_ROUTING,
/*   .hooknum = NF_IP_LOCAL_IN, */
    .priority = NF_IP_PRI_FIRST 
};


static int __init modelnet_init(void)
{

    int ret;

    /* Seed our random number generator */
    unsigned int randomVal;
    get_random_bytes(&randomVal, 4);
    random_seed(randomVal);

  
    /* Register sysctl table */
    if (!(my_table_header = register_sysctl_table(modelnet_ctl_table)))
	return -EPERM;
  
    /* load modelnet */
    if (modelnet_load()) 
    {
	printk ("Error loading Modelnet\n");
	return -EPERM;
    }
  
    /* create work queue */
    modelnet_workqueue = create_workqueue(MN_WORKQUEUE_NAME);
    queue_delayed_work(modelnet_workqueue, &hopclock_task, 1);


    /* register netfilter hook */
    if ((ret = nf_register_hook(&nfho)) < 0)
	printk ("Modelnet unable to register with netfilter, check kernel config\n");
    else
	printk ("Modelnet registered with netfilter\n");

    return ret;
}

static void __exit modelnet_cleanup(void)
{
    if (my_table_header)
	unregister_sysctl_table(my_table_header);
  
    nf_unregister_hook(&nfho);
  
    /* keep intrp_routine from queueing itself */
    modelnet_die = 1;		

    /* cancel hopclock */
    cancel_delayed_work(&hopclock_task);	/* no "new ones" */
    flush_workqueue(modelnet_workqueue);	/* wait till all "old ones" finished */
    destroy_workqueue(modelnet_workqueue);
  
    /* unload modelnet */
    modelnet_unload();
}

module_init(modelnet_init);
module_exit(modelnet_cleanup);
