/*
 * modelnet  udp_send.c
 *
 *     send mbuf chains as UDP/IP packets.
 *
 *     Used by the modelnet module communication among forwarders.
 *
 * author Darrell Anderson
 *
 */

/*
 * Copyright (c) 2000 Duke University -- Darrell Anderson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Duke University
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ipprotosw.h>
#include <machine/in_cksum.h>

/*
 * #include "sliceconfig.h" 
 *//*
 * * for profiling macros 
 */
#include "udp_send.h"
#include "udp_rewrite.h"

extern struct ipprotosw inetsw[];
#ifdef KLD_MODULE
extern int      udpcksum;       /* are udp checksums enabled? */
#else
static int      udpcksum = 1;   /* hack to allow a static profiling kernel 
                                 * build. */
#endif

static __inline void
route_refcount_hack(struct route *ro)
{
#if 0
    ro->ro_rt->rt_refcnt = 32768;
#endif
}

/*
 * [udp_send_out_complete]
 *
 * send an mbuf (with udpiphdr) to the specified IP or broadcast address.
 *
 * Parameters:
 *      m       mbuf chain
 *
 * Returns:
 *      int see errno.h, 0 for success 
 */
int
udp_send_out_complete(struct mbuf *m)
{
    struct ip      *ip = mtod(m, struct ip *);
    struct sockaddr_in *dst;
    struct in_ifaddr *ia;
    struct route    ro;
    int             error;

    dst = (struct sockaddr_in *) &ro.ro_dst;
    dst->sin_family = AF_INET;
    dst->sin_len = sizeof(struct sockaddr_in);
    dst->sin_addr = ip->ip_dst;
    ro.ro_rt = (struct rtentry *) 0;
    rtalloc_ign(&ro, RTF_CLONING);
    route_refcount_hack(&ro);

    /*
     * rewrite the source address to match the outgoing interface.
     * makes the world a simpler place when rewriting between two
     * interfaces (e.g., fast ether -> myrinet).
     */
    for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
        if (ia->ia_ifp == ro.ro_rt->rt_ifp) {
            udp_rewrite_src(m, IA_SIN(ia)->sin_addr.s_addr);
            break;
        }
    }
    if (ia == NULL) {
        return EHOSTUNREACH;
    }

#if __FreeBSD_version <= 470000 
    error = ip_output(m, NULL, &ro, IP_ALLOWBROADCAST, 0);
#else
    error = ip_output(m, NULL, &ro, IP_ALLOWBROADCAST, 0, NULL);
#endif

    rtfree(ro.ro_rt);           /* route stays cached */
    return error;
}

/*
 * [udp_send_out]
 *
 * attach a UDP/IP header and unreliably send an mbuf chain to the
 * specified IP or broadcast address.
 *
 * Parameters:
 *      to      ip or broadcast address (network byte order)
 *      sport   source port number (network byte order)
 *      dport   destination port number (network byte order)
 *      m       mbuf chain
 *
 * Returns:
 *      int     see errno.h, 0 for success
 */
int
udp_send_out(struct in_addr to, short sport, short dport, struct mbuf *m)
{
    struct udpiphdr *ui;
    struct ip      *ip;
    struct route    ro;
    struct sockaddr_in *src,
                   *dst;
    struct in_ifaddr *ia;
    int             error;

    if (m == 0) {
        return ENOBUFS;
    }

    M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
    if (m == 0) {
        return ENOBUFS;
    }

    /*
     * compute the route early so we can grab the interface address
     * as our packet return address (need before checksumming).
     */
    dst = (struct sockaddr_in *) &ro.ro_dst;
    dst->sin_family = AF_INET;
    dst->sin_len = sizeof(struct sockaddr_in);
    dst->sin_addr = to;
    ro.ro_rt = (struct rtentry *) 0;
    rtalloc_ign(&ro, RTF_CLONING);
    route_refcount_hack(&ro);

    for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
        if (ia->ia_ifp == ro.ro_rt->rt_ifp) {
            src = IA_SIN(ia);
            break;
        }
    }
    if (ia == NULL) {
        return EHOSTUNREACH;
    }

    /*
     * fill in the udp header (including checksum), then the ip dregs.
     */
    ui = mtod(m, struct udpiphdr *);
    bzero(ui->ui_x1, sizeof(ui->ui_x1));
    ui->ui_pr = IPPROTO_UDP;
    ui->ui_len = htons(m->m_pkthdr.len - sizeof(struct ip));
    ui->ui_src = src->sin_addr;
    ui->ui_dst = dst->sin_addr;
    ui->ui_sport = sport;
    ui->ui_dport = dport;
    ui->ui_ulen = ui->ui_len;
    ui->ui_sum = 0;
    if (udpcksum != 0) {
        if ((ui->ui_sum = in_cksum(m, m->m_pkthdr.len)) == 0) {
            ui->ui_sum = 0xffff;
        }
    }
    ip = mtod(m, struct ip *);
    ip->ip_len = m->m_pkthdr.len;
    ip->ip_off = ip->ip_tos = 0;
    ip->ip_ttl = IPDEFTTL;

#if __FreeBSD_version <= 470000 
    error = ip_output(m, NULL, &ro, IP_ALLOWBROADCAST, 0);
#else
    error = ip_output(m, NULL, &ro, IP_ALLOWBROADCAST, 0, NULL);
#endif


    rtfree(ro.ro_rt);           /* route stays cached */
    return error;
}

/*
 * [udp_send_in_complete]
 *
 * send an mbuf (with udpiphdr) up our local UDP stack.
 *
 * Parameters:
 *      m       mbuf chain
 *
 * Returns:
 *      int see errno.h, 0 for success 
 */
int
udp_send_in_complete(struct mbuf *m)
{
    (*inetsw[ip_protox[IPPROTO_UDP]].pr_input)
        (m, sizeof(struct ip), IPPROTO_UDP);
    return 0;
}

/*
 * [udp_send_in]
 *
 * attach a UDP/IP header and send an mbuf chain up our local UDP stack,
 * forging the specified from address.
 *
 * Parameters:
 *      from    ip address (network byte order)
 *      sport   source port number (network byte order)
 *      dport   destination port number (network byte order)
 *      m       mbuf chain
 *
 * Returns:
 *      int     see errno.h, 0 for success
 */
int
udp_send_in(struct in_addr from, short sport, short dport, struct mbuf *m)
{
    struct udpiphdr *ui;
    struct ip      *ip;
    struct route    ro;
    struct sockaddr_in *src,
                   *dst;
    struct in_ifaddr *ia;

    M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
    if (m == 0) {
        return ENOBUFS;
    }

    /*
     * compute the route early so we can grab the interface address
     * as our packet destination address (need before checksumming).
     */
    src = (struct sockaddr_in *) &ro.ro_dst;
    src->sin_family = AF_INET;
    src->sin_len = sizeof(struct sockaddr_in);
    src->sin_addr = from;
    ro.ro_rt = (struct rtentry *) 0;
    rtalloc_ign(&ro, RTF_CLONING);
    route_refcount_hack(&ro);

    for (ia = in_ifaddrhead.tqh_first; ia; ia = ia->ia_link.tqe_next) {
        if (ia->ia_ifp == ro.ro_rt->rt_ifp) {
            dst = IA_SIN(ia);
            break;
        }
    }
    if (ia == NULL) {
        return EHOSTUNREACH;
    }
    m->m_pkthdr.rcvif = ia->ia_ifp;

    /*
     * fill in the udp header (including checksum), then the ip dregs.
     * on second thought, skip the checksum.  the packet won't be
     * corrupted between here and passing it to the local protocol
     * input.  or if it is, you have bigger problems.
     */
    ui = mtod(m, struct udpiphdr *);
    bzero(ui->ui_x1, sizeof(ui->ui_x1));
    ui->ui_pr = IPPROTO_UDP;
    ui->ui_len = htons(m->m_pkthdr.len - sizeof(struct ip));
    ui->ui_src = src->sin_addr;
    ui->ui_dst = dst->sin_addr;
    ui->ui_sport = sport;
    ui->ui_dport = dport;
    ui->ui_ulen = ui->ui_len;
    ui->ui_sum = 0;             /* don't compute checksum */
    ip = mtod(m, struct ip *);
    ip->ip_len = m->m_pkthdr.len - sizeof(struct ip);

    (*inetsw[ip_protox[IPPROTO_UDP]].pr_input)
        (m, sizeof(struct ip), IPPROTO_UDP);
    rtfree(ro.ro_rt);           /* route stays cached */
    return 0;
}
