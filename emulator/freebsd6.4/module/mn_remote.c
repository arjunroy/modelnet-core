/*
 * modelnet  mn_remote.c
 *
 *     emulation of hops on remote machines
 *
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 *
 */

#define _IP_VHL

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
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/in_cksum.h>
#include "udp_send.h"

#include "ip_modelnet.h"


/*
 * multi-core interface 
 */
static void     mc_pcache(struct packet * pkt);
static int      mc_reunite(struct packet * * pkt);

/*
 * multi-core pcaching: With mc_pcache enabled, each packet's data is
 * cached at the entry to the core.  When the last hop is emulated, the
 * mn_pkt returns home, reunites with it's data, and is sent to its edge
 * node.  When a packet is tunnelled, we send only the mn_pkt struct. W/O 
 * pcache: The mn_pkt struct plus the entire packet are tunnelled btwn
 * cores.  This includes headers (which could be reconstituted at the exit 
 * core. 
 */

struct mn_config mn;

SYSCTL_NODE(_net_inet_ip_modelnet, OID_AUTO, mc, CTLFLAG_RW, 0,
            "Multi-Core");

SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, state,
           CTLFLAG_RW, &mn.state, 0, "modelnet core state");

SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, pkt_alloc,
           CTLFLAG_RW, &mn.stats.pkt_alloc, 0, "modelnet core state");
SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, pkt_free,
           CTLFLAG_RW, &mn.stats.pkt_free, 0, "modelnet core state");

SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, pcache,
           CTLFLAG_RW, &mn.stats.pcache_occupancy, 0, "pcache occupancy");


SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, port,
           CTLFLAG_RW, &remote_port, 0, "udp port (host order)");
SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, ip,
           CTLFLAG_RW, &mn.ip_home, 0, "udp ip (host order)");
SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, expire,
           CTLFLAG_RW, &mn.mc_expire, 0, "expiration (ms)");
SYSCTL_INT(_net_inet_ip_modelnet_mc, OID_AUTO, ttl,
           CTLFLAG_RW, &mn.mc_ttl, 0, "ttl (hops)");



#define MCORE_EXPIRE  (hz)      /* 1 seconds */
#define MCORE_TTL 40            /* 10 inter-core hops */

#define MN_LOG_PACKET(e_fromip, e_toip, e_pipenr, e_drop) do { } while(0)

#define DDB(x)

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


static struct mn_pending_head pending_xc;   /* pending aggregated
					     * cross-core xmits  */
static struct mn_pending_head free_pending_xc;   /* pending aggregated
					     * cross-core xmits  */



/*
 * [udp_validate]
 *
 * Validates UDP length and checksum
 * Almost verbatim from udp_input
 * returns: 1 -> good, 0 -> bad
 */
static int
udp_validate(register struct mbuf *m)
{
	register struct ip *ip = mtod(m, struct ip *);
	register struct udphdr *uh;
	int len;
//	int iphlen = IP_VHL_HL(ip->ip_vhl) << 2;
	int iphlen = ip->ip_hl << 2;

	/*
	 * Can't check cksum with options present
	 */
	if (iphlen > sizeof (struct ip)) {
		printf("udp_cksum_validate: ip options present, skipping checks\n");
#ifdef NOTYET
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof(struct ip);
#endif
		return(1);
	}

	/*
	 * Get IP and UDP header together in first mbuf.
	 */
	if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct udphdr))) == 0) {
			return(0);
		}
		ip = mtod(m, struct ip *);
	}
	uh = (struct udphdr *)((caddr_t)ip + iphlen);

	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_short)uh->uh_ulen);
	if (ip->ip_len != len) {
		if (len > ip->ip_len || len < sizeof(struct udphdr)) {
			printf("udp_validate: bad udp length\n");
			m_freem(m);
			return(0);			
		}
	}

	/*
	 * Checksum extended UDP header and data.
	 */
	if (uh->uh_sum) {
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
				uh->uh_sum = m->m_pkthdr.csum_data;
			else
	                	uh->uh_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htonl((u_short)len +
				    m->m_pkthdr.csum_data + IPPROTO_UDP));
			uh->uh_sum ^= 0xffff;
		} else {
			char b[9];
			bcopy(((struct ipovly *)ip)->ih_x1, b, 9);
			bzero(((struct ipovly *)ip)->ih_x1, 9);
			((struct ipovly *)ip)->ih_len = uh->uh_ulen;
			uh->uh_sum = in_cksum(m, len + sizeof (struct ip));
			bcopy(b, ((struct ipovly *)ip)->ih_x1, 9);
		}
		if (uh->uh_sum) {
			printf("udp_validate: failed, bad checksum\n");
			m_freem(m);
			return(0);
		}
	} 
	return(1);
}


#define DEBUG_REMOTE_INPUT

/*
 * [remote_input] Packet, without data, from peer.  Restore pkt->info,
 * return 0 on success 
 */
int
remote_input(struct mbuf *m)
{
    u_int32_t       ip_len;
    struct remote_packet *info;
    struct packet * pkt = NULL;
    int err = 0, once = 1;
    
    mn.stats.mc_recv++;
    Dprintf(("remote_input: received foreign packet\n"), MN_P_PKTS);
    ASSERT(m);
    if (udp_validate(m)==0) {
	    printf("UDP fail\n");
	    return EINVAL;
    }
    ip_len = m->m_pkthdr.len;
    m_adj(m, sizeof(struct udpiphdr));

    while(m->m_pkthdr.len && once){
	    if (once > 46){
		    printf("remote_input: pkthdrlen(%d) once (%d)\n",m->m_pkthdr.len,once);
		    err = 1; /* too many 32 byte remote pkts in
				 1500-28 byte packet */
		    goto error;
	    }
	    once++;
	    if (m->m_pkthdr.len - sizeof(struct remote_packet) < 0){
		    printf("remote_input: m->m_pkthdr.len(%d) < sizeof(remote_packet)(%d)\n",
			   m->m_pkthdr.len, sizeof(struct remote_packet));
		    err = 1;
		    goto error;
	    }
	    info = mtod(m, struct remote_packet *);
	    pkt = malloc(sizeof(struct packet), M_MN_PKTS, M_NOWAIT);
	    if (!pkt) {
		    err = ENOBUFS;
		    goto error;
	    }
	    mn.stats.pkt_alloc++;
	    pkt->cachehost = 0;
	    pkt->info.id = (unsigned long) NULL;
	    pkt->m = NULL;
	    m_copydata(m,0,sizeof(struct remote_packet),(caddr_t)&(pkt->info));

	    m_adj(m, sizeof(struct remote_packet));
	    if (MC_PKT_PCACHED(pkt)) {
		    /* with aggregation we could have many */
		    if (!(mn.state & MN_AGGREGATE) && (ip_len !=
			sizeof(struct remote_packet) + sizeof(struct udpiphdr))) {
			    printf("modelnet: got udp of wrong size (%u != %d!)\n", ip_len,
				   sizeof(struct remote_packet) + sizeof(struct udpiphdr));
			    err = EINVAL;
			    goto error;
		    }
	    } else {                    /* unencapsulate non-cached packets */
		    /* XXX untested ! */
		    /*pkt->m = m_split(pkt->m, sizeof(struct remote_packet), M_DONTWAIT);*/
		    printf("remote_input: aggregation with non pcached pkts\n");
		    pkt->m = m;
		    m = NULL;
		    once = 0;  /* we take the rest */
	    }
	    if (info->ttl-- <= 0) {
		    DDB(printf("modelnet: ttl expired on pkt 0x%x\n", (int) info->id);
			    )
			    mn.stats.mc_ttldrops++;
		    MN_LOG_PACKET(info->src, info->dst, info->hop, 1);
		    err = EINVAL;
#ifdef DEBUG_REMOTE_INPUT
		    goto error;
#else
		    MN_FREE_PKT(pkt);
		    continue;
#endif
	    }

	    /*
	     * XXX lookup_path happens for each new core in path 
	     */
	    pkt->path = lookup_path(MODEL_FORCEOFF(pkt->info.src), pkt->info.dst);
	    if (pkt->path == NULL){
		    err = ENOENT;
		    goto error;
	    }
	    pkt->cachehost = pkt->path[0]->emulator;
	    pkt->path = pkt->path + pkt->info.hop;
	    pkt->state = 0;

	    /*
	     * if home and we have a pointer, means pcache was enabled when we
	     * left, then even if pcache is now off, it should still be in the
	     * cache 
	     */
	    if (MC_PKT_HOME(pkt) && MC_PKT_PCACHED(pkt)) {
		    Dprintf(("mc_reunite: on pkt(0x%x) id(0x%x)\n",(u_int32_t)pkt,(u_int32_t)pkt->info.id),MN_P_PKTS);
		    err = mc_reunite(&pkt);
		    if (err){
#ifdef DEBUG_REMOTE_INPUT
			    goto error;
#else
			    printf("remote_input: mc_reunite error\n");
			    if (pkt) MN_FREE_PKT(pkt);
			    continue;
#endif
		    }
	    }	    
	    emulate_nexthop(pkt);
    }

    if (m)
	    m_freem(m);

    return (0);
 error:
    printf("remote_input: error %d\n", err);
    if (m)
	    m_freem(m);
    if (pkt)
	    MN_FREE_PKT(pkt);
    return(err);
}

/*
 * [remote_aggregate_clean] 
 * 
 * Anything left on the pending list needs to be sent out.
 */
void
remote_aggregate_clean(void)
{
    struct remote_aggregate *ragg;
    while(!(TAILQ_EMPTY(&pending_xc))){
	    ragg = TAILQ_FIRST(&pending_xc);
	    remote_aggregate_send(ragg);
    }
    return;
}


/*
 * [mn_aggregate_send] Send the aggregate, uncache all the entries if
 * we fail.
 */
int
remote_aggregate_send(struct remote_aggregate *ragg)
{
    struct in_addr  to;
    int             err=0;

    to.s_addr = ragg->id;
    ASSERT(ragg->id);

    Dprintf(("remote_aggregate_send: sending agg (0x%x) to (0x%x)\n",
	   (u_int32_t)ragg, (u_int32_t)ragg->id),MN_P_AGG);
    

    err = udp_send_out(to, htons(remote_port), htons(remote_port), ragg->m);
    if (err) {
        mn.stats.mc_errs++;
	printf("mn_aggregate_send: udp_send_out error(%d)\n",err);
	/* unlike ordinary sends, we can't guarantee that pkt hasn't
	   been freed, it may have been freed after we aggregate.  So
	   we can't even keep a linked list of packets to free from the
	   pcache, instead we have to rely more on the cleaner.	  
	*/
    }
    ragg->m = NULL;
    ragg->id = 0;  /* put on free list */
    TAILQ_REMOVE(&pending_xc, ragg, list);
    ragg->state = MN_AGG_FREE;
    TAILQ_INSERT_TAIL(&free_pending_xc, ragg, free);
    return err;
}
	    


/*
 * [mn_aggregate] Collect a bunch of pkt->info's bound for the same
 * destination and send as one UDP frame. 
 */
int
remote_aggregate(struct packet * pkt, in_addr_t owner_ip)
{
    struct mbuf    *m;	
    int             err=0;
    struct remote_aggregate *ragg;
        

    ragg = TAILQ_FIRST(&(pending_xc));	
    while(ragg){  /* dumb search */
	    if (ragg->state & MN_AGG_FREE){
		    printf("remote_aggregate: ragg state is FREE!\n");
	    }
	    if (ragg->id == owner_ip){
		    break;
	    }
	    ragg = TAILQ_NEXT(ragg, list);
    }
    if (!ragg){  /* no pending found, grab one */
	    if (TAILQ_EMPTY(&free_pending_xc)){
		    printf("mn_aggregate: empty free queue increase MN_MAX_CORES!\n");
	    }
	    ragg = TAILQ_FIRST(&free_pending_xc);
	    TAILQ_REMOVE(&free_pending_xc, ragg, free);
	    ragg->state = MN_AGG_BUSY;
	    TAILQ_INSERT_TAIL(&pending_xc, ragg, list);
	    ragg->id = owner_ip;
	    ragg->num_digests = 0;
	    ragg->bytes_left = MN_MTU - sizeof(struct udpiphdr);	    
	    ragg->m = NULL;
    }

    /* room left? */
    if (ragg->bytes_left >= sizeof(struct remote_packet)){
	    /* started? */
	    if (ragg->num_digests == 0){
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		MCLGET(m, M_DONTWAIT);
		m->m_len = sizeof(struct remote_packet);
		m->m_pkthdr.len = sizeof(struct remote_packet);
		m->m_data += sizeof(struct udpiphdr);  /* leave room here */

		    bcopy((caddr_t)&(pkt->info),mtod(m, caddr_t),
			  sizeof(struct remote_packet));
		    ragg->m = m;
		    Dprintf(("mn_aggregate: starting aggregate (0x%x) to (0x%x)\n",
			     (u_int32_t)ragg, ragg->id),MN_P_AGG);
	    } else {
		    ASSERT(ragg->m);
		    m = ragg->m;
		    m->m_pkthdr.len += sizeof(struct remote_packet);
		    ASSERT(m->m_len + sizeof(struct remote_packet) <= MCLBYTES);
		    bcopy((caddr_t)&(pkt->info),(caddr_t)(m->m_data + m->m_len),
			  sizeof(struct remote_packet));
		    m->m_len += sizeof(struct remote_packet);
		    Dprintf(("mn_aggregate: Using aggregate (0x%x) to (0x%x) bytesleft (%d)\n",
			     (u_int32_t)ragg, ragg->id, ragg->bytes_left),MN_P_AGG);
	    }
	    ragg->num_digests++;
	    ragg->bytes_left -= sizeof(struct remote_packet);
    } else {
	    printf("remote_aggregate: error: no room left on aggregate\n");
    }    
    if (ragg->bytes_left < sizeof(struct remote_packet)){
	    /* overflow push */
	    err = remote_aggregate_send(ragg);
    }
    return(err);    
}


/*
 * [remote_hop] Tunnel packet to another modelnet core.  Either hop to
 * another core for emulation or sending to home core.  All we care about
 * is owner_ip. 
 */
int
remote_hop(struct packet * pkt, in_addr_t owner_ip)
{
    struct mbuf    *m;
    struct in_addr  to;
    struct ip      *ip;
    int             err;


    /*
     * hop should never be zero 
     */
    if (pkt->info.hop == 0){
	    printf("remote_hop: trying to forward packet with hop==0, dropping.\n");
	    return EINVAL;
    }


    /*
     * prepare to tunnel pkt, fill in pkt fields, pcache if enabled 
     */
    if (MC_PKT_HOME(pkt)) {
        struct ip      *ip = mtod(pkt->m, struct ip *);
        pkt->info.src = ip->ip_src.s_addr;
        pkt->info.dst = ip->ip_dst.s_addr;
        pkt->info.ttl = mn.mc_ttl;
        pkt->info.id = 0;
        pkt->info.expire = 0;
        if (mn.state & MN_PCACHE) {
            mc_pcache(pkt);
        }
    }
    
    if (pkt->info.src == 0){
	    printf("remote_hop: srcip is 0. dropping.\n");
	    return EINVAL;
    }
    

    if ((mn.state & MN_AGGREGATE) && 
	(MC_PKT_PCACHED(pkt))){ 
	    /* only do this if we're pcaching */
	    err = remote_aggregate(pkt, owner_ip);    
 	    return(err);
    }

    mn.stats.mc_send++;
    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (!m) {
        DDB(printf("modelnet: cannot get mbuf for remote_hop()\n");
            );
        if (MC_PKT_HOME(pkt)) { /* remove from wait list */
            MN_UNCACHE_PKT(pkt);
        }
        return ENOBUFS;
    }
    /*
     * the mbuf can hold the whole packet
     * if MHLEN > sizeof(strcut udpiphdr) + sizeof(struct mnpkt_info)
     */
    m->m_len = sizeof(struct remote_packet);
    m->m_pkthdr.len = sizeof(struct remote_packet);
    m->m_data += sizeof(struct udpiphdr);  /* leave room here */
    bcopy((caddr_t)&(pkt->info),mtod(m, caddr_t),
	  sizeof(struct remote_packet));

    /*
     * if this core pcached, then info.id != NULL if came from remote
     * core who pcached, then info.id != NULL if came from remote core did 
     * not pcache, then info.id == NULL 
     */
    if (!MC_PKT_PCACHED(pkt)) {
        /*
         * add entire packet to outgoing chain 
         */
        ASSERT(pkt->m);
        pkt->m->m_flags &= ~M_PKTHDR;
        pkt->m->m_pkthdr.len = 0;       /* no header */
        pkt->m->m_pkthdr.rcvif = NULL;
        ip = mtod(pkt->m, struct ip *);
        pkt->m = m_pullup(pkt->m, sizeof(struct ip));
        MN_DUMP_PKT(pkt);
        m_cat(m, pkt->m);
        pkt->m = NULL;
        m->m_pkthdr.len += pkt->info.len;
    }

    to.s_addr = owner_ip;

    Dprintf(("modelnet: remote_hop %p to %lu.%lu.%lu.%lu"
             " (id=0x%x, expire=%d, len=%d, hop=%d, ttl=%d)\n",
             pkt, IP_ADDR_QUAD_N(owner_ip),
             (int) pkt->info.id, (int) pkt->info.expire,
             (int) pkt->info.len, (int) pkt->info.hop,
             (int) pkt->info.ttl), MN_P_PKTS);


    err = udp_send_out(to, htons(remote_port), htons(remote_port), m);
    if (err) {
        mn.stats.mc_errs++;
	printf("remote_hop: udp_send_out error(%d)\n",
	       err);
        if (MC_PKT_HOME(pkt) && (mn.state & MN_PCACHE)) {
            MN_UNCACHE_PKT(pkt);        /* remove from pcache */
        }
    }
    return err;
}



/*
 * [mc_reunite] Using reference in pkt, reunite modelnet pkt with
 * original mbuf Mbuf in m is ready to be sent out after this call. 
 */
static int
mc_reunite(struct packet * * pkt)
{
    struct remote_packet *info = &((*pkt)->info);
    struct packet *        cached_pkt;


    /*
     * this is one of our own packets, re-unite with the mbuf 
     */
    ASSERT(MC_PKT_HOME((*pkt)));

    /*
     * has expired ? 
     */
    if (info->expire <= ticks) {
        /*
         * dangerous, it might have expired 
         */
        printf("modelnet: info expired on pkt 0x%x\n", (int) info->id);
        mn.stats.mc_errs++;
        MN_LOG_PACKET(info->src, info->dst, info->hop, 1);
        return 1;
    }

    /*
     * validate pkt info id 
     */
    cached_pkt = (struct packet *) info->id;

    if (info->hop==0){
	    printf("mc_reunite: info->hop==0, dropping pkt.\n");
	    return 1;
    }


#if 0
    if (0 /* heavy DEBUG routine */){
	    /*
	     * Verify that info->id is really cached at this core
	     * as opposed to blindly accessing it.
	     */	    
	    mn_pkt_t nxt_pkt=TAILQ_FIRST(&(mn.mc_pcache));
	    while(nxt_pkt){
		    if((unsigned long)nxt_pkt->info.id == (unsigned long)cached_pkt){
			    /* we found it */
			    goto LIFE_IS_GOOD;
		    }
		    nxt_pkt = TAILQ_NEXT(nxt_pkt,list);
	    }
	    /* LIFE_SUCKS: */
	    printf("mc_reunite: pkt(0x%x)->info.id(0x%x) not found in pcache!",
		   (u_int32_t)*pkt, (u_int32_t)cached_pkt);
	    printf(" Dropping packet, and continuing...\n");
            mn.stats.mc_errs++;
	    MN_LOG_PACKET(info->src_ip, info->dst_ip, info->hop, 1);
	    return 1;
    }
    LIFE_IS_GOOD:
#endif


    if (cached_pkt->info.id != info->id ||
        cached_pkt->info.expire != info->expire) {
        /*
         * something looks fishy 
         */
        printf("modelnet: info looks fishy on cached_pkt 0x%x\n",
                   (int) info->id);
        mn.stats.mc_errs++;
        MN_LOG_PACKET(info->src, info->dst, info->hop, 1);
        return 1;
    }

    /*
     * remove pkt from pcache 
     */
    ASSERT(!(TAILQ_EMPTY(&(mn.mc_pcache))));
    MN_UNCACHE_PKT(cached_pkt);

    /*
     * update any state from prodigal packet 
     */
    cached_pkt->info.hop = info->hop;
    cached_pkt->info.id = (unsigned long) NULL;
    cached_pkt->path = (*pkt)->path;

    /*
     * free incoming packet 
     */
    MN_FREE_PKT((*pkt));

    /*
     * return cached packet 
     */
    *pkt = cached_pkt;

    return (0);
}



/*
 * [mc_pcache] Cache the mbuf of this incoming packet, store reference
 * in pkt. 
 */
static void
mc_pcache(struct packet * pkt)
{
    ASSERT(MC_PKT_HOME(pkt));

    /*
     * our packet, fill out info for packet 
     */
    pkt->info.expire = ticks + mn.mc_expire;
    pkt->info.id = (unsigned long) (pkt);
    TAILQ_INSERT_TAIL(&(mn.mc_pcache), pkt, list);
    Dprintf(("mc_pcache: caching packet 0x%x\n",(u_int32_t)pkt),MN_P_PKTS);
    mn.stats.pcache_occupancy++;
}


u_short         remote_port;

/*
 * For multicore we need to timeout packets from the list
 * if they are dropped by other cores.  Follow naming convention of
 * dummynet module.
 */
void
remote_flush()
{
    struct packet *        pkt;

    if (mn.state & MN_PCACHE) {
        /*
         * scan pcache for expired packets 
         */
        while (!TAILQ_EMPTY(&(mn.mc_pcache))) {
            pkt = TAILQ_FIRST(&(mn.mc_pcache));
            if (pkt->info.expire > ticks)
                break;
            MN_UNCACHE_PKT(pkt);
            MN_FREE_PKT(pkt);
        }
    }
}

void
remote_init(void)
{
	int i;
	struct remote_aggregate *new_aggs;
	
	/*
	 * initialize multi-core packet cache 
	 */
	bzero(&mn, sizeof(mn));
	mn.state = (MN_KERNEL | MN_PCACHE | MN_AGGREGATE );
	TAILQ_INIT(&(mn.mc_pcache));
	mn.mc_expire = MCORE_EXPIRE;
	remote_port = MODEL_PORT;
	mn.mc_ttl = MCORE_TTL;
	TAILQ_INIT(&(pending_xc));
	TAILQ_INIT(&(free_pending_xc));
	if (!(MALLOC(new_aggs,struct remote_aggregate*,
		     sizeof(struct remote_aggregate)*MN_MAX_CORES, 
		     M_DEVBUF, M_NOWAIT)))
	{
		printf("remote_init: failed malloc\n");
		return;
	}
	bzero(new_aggs, sizeof(struct remote_aggregate) * MN_MAX_CORES);
	for (i=0;i<MN_MAX_CORES;i++){
		new_aggs[i].state = MN_AGG_FREE;
		new_aggs[i].bytes_left = MN_MTU - sizeof(struct udpiphdr);
/*		TAILQ_INIT(&(new_aggs[i].aggregate_pktlist));*/
		TAILQ_INSERT_HEAD(&free_pending_xc, &(new_aggs[i]),
				  free);
	}
}

void
remote_uninit(void)
{
    struct packet  *pkt;
    /*
     * remove any packets waiting in our pcache 
     */
    while (!(TAILQ_EMPTY(&(mn.mc_pcache)))) {
        pkt = TAILQ_FIRST(&(mn.mc_pcache));
        MN_UNCACHE_PKT(pkt);
        MN_FREE_PKT(pkt);
    }
}
