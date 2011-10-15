/*
 * modelnet  udp_rewrite.c
 *
 *     modify udp packets, updating the udp checksum if necessary.
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
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <machine/in_cksum.h>

/*
 * #include "sliceconfig.h" 
 *//*
 * * for profiling macros 
 */
#include "udp_rewrite.h"

#ifdef KLD_MODULE
extern int      udpcksum;       /* compute udp checksums? */
#else
static int      udpcksum = 1;   /* hack to allow a static profiling kernel 
                                 * build. */
#endif

/*
 * recomputing checksums from partial changes thanks to Darren Reed's
 * ip_nat.c.  these are encapsulated versions of several functions there.
 */
#define	LONG_SUM(in) (((in) & 0xffff) + ((in) >> 16))
#define	CALC_SUMD(s1, s2, sd) { \
	(s1) = ((s1) & 0xffff) + ((s1) >> 16); /* once */ \
	(s1) = ((s1) & 0xffff) + ((s1) >> 16); \
	(s2) = ((s2) & 0xffff) + ((s2) >> 16); /* twice */ \
	(s2) = ((s2) & 0xffff) + ((s2) >> 16); \
	if ((s1) > (s2)) (s2)--; /* ~1 == -2, we really need ~1 == -1 */ \
	(sd) = (s2) - (s1); \
	(sd) = ((sd) & 0xffff) + ((sd) >> 16); \
}

static __inline void
udp_rewrite_word(struct mbuf *m, int off, u_int32_t new)
{
    struct udpiphdr *udp = mtod(m, struct udpiphdr *);

#if 0
    u_int32_t       old,
                    sum1,
                    sum2,
                    sumd;
    unsigned short  sumshort;

    if (udp->ui_sum != 0 && udpcksum) {
        if (off + sizeof(u_int32_t) <= m->m_len) {
            old = *((u_int32_t *) (mtod(m, caddr_t) + off));
        } else {
            m_copydata(m, off, sizeof(u_int32_t), (caddr_t) & old);
        }
        sum1 = LONG_SUM(ntohl(new));
        sum2 = LONG_SUM(ntohl(old));
        CALC_SUMD(sum1, sum2, sumd);
        sumd = (sumd & 0xffff) + (sumd >> 16);
        if (sumd != 0) {
            sum1 = (~ntohs(udp->ui_sum)) & 0xffff;
            sum1 += ~(sumd) & 0xffff;
            sum1 = (sum1 >> 16) + (sum1 & 0xffff);
            sum1 = (sum1 >> 16) + (sum1 & 0xffff);      /* again */
            sumshort = ~(u_short) sum1;
            udp->ui_sum = htons(sumshort);
        }
    }
#else
    if (udp->ui_sum != 0 && udpcksum) {
        if (m->m_pkthdr.csum_flags & CSUM_UDP) {
            int             len =
                m->m_pkthdr.len - sizeof(struct udpiphdr);
            /*
             * funky checksum stuff.. XXX 
             */
            udp->ui_sum = in_pseudo(udp->ui_src.s_addr, udp->ui_dst.s_addr,
                                    htons((u_short) len +
                                          sizeof(struct udphdr) +
                                          IPPROTO_UDP));
            m->m_pkthdr.csum_flags = CSUM_UDP;
            m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
        } else if (udp->ui_sum != 0) {
            printf("warning: not m->m_pkthdr.csum_flags & CSUM_UDP");
        }
    }
#endif
    if (off + sizeof(u_int32_t) <= m->m_len) {
        *((u_int32_t *) (mtod(m, caddr_t) + off)) = new;
    } else {
        m_copyback(m, off, sizeof(u_int32_t), (caddr_t) & new);
    }
}

/*
 * [udp_rewrite_misc]
 *
 * rewrite a portion of a udp packet, updating its checksum.
 * fast even for large packets.
 *
 * Parameters:
 *      m       mbuf chain, entire udpiphdr must be in the first mbuf.
 *      off     offset where change begins
 *      len     length of new data
 *      data    pointer to new data
 *
 * Returns:
 *      <void>
 */
void
udp_rewrite_misc(struct mbuf *m, int off, int len, void *data)
{
    u_int32_t      *wordstream = (u_int32_t *) data;
    int             i;

    if ((off & 0x3) || (len & 0x3)) {
        panic("udp_rewrite_misc: invalid offset (%d) or len (%d)",
              off, len);
    }
    if (off + len > m->m_pkthdr.len) {
        panic("udp_rewrite_misc: offset (%d) + len (%d) > mlen (%d)",
              off, len, m->m_pkthdr.len);
    }

    for (i = 0; i < len / sizeof(u_int32_t); i++) {
        udp_rewrite_word(m, off + sizeof(u_int32_t) * i, wordstream[i]);
    }
}

/*
 * [udp_rewrite_src]
 *
 * rewrite the source address of a udp/ip packet and update its udp
 * checksum.  fast even for large packets.
 *
 * Parameters:
 *      m       mbuf chain, entire udpiphdr must be in the first mbuf.
 *      src     ip address (network byte order).
 *
 * Returns:
 *      <void>
 */
void
udp_rewrite_src(struct mbuf *m, u_int32_t src)
{
    udp_rewrite_word(m, (int) (long) &((struct ip *) 0)->ip_src.s_addr,
                     src);
}

/*
 * [udp_rewrite_dst]
 *
 * rewrite the destination address of a udp/ip packet and update its udp
 * checksum.  fast even for large packets.
 *
 * Parameters:
 *      m       mbuf chain, entire udpiphdr must be in the first mbuf.
 *      dst     ip or broadcast address (network byte order).
 *
 * Returns:
 *      <void>
 */
void
udp_rewrite_dst(struct mbuf *m, u_int32_t dst)
{
    udp_rewrite_word(m, (int) (long) &((struct ip *) 0)->ip_dst.s_addr,
                     dst);
}

/*
 * [udp_rewrite_ports]
 *
 * rewrite the source and/or destination ports udp/ip packet and update its
 * udp checksum.  fast even for large packets.
 *
 * Parameters:
 *      m       mbuf chain, entire udpiphdr must be in the first mbuf
 *      sport   source port (network byte order), or -1 to preserve old
 *      dport   dest port (network byte order), or -1 to preserve old
 *
 * Returns:
 *      <void> 
 */
void
udp_rewrite_ports(struct mbuf *m, short sport, short dport)
{
    struct udpiphdr *ui = mtod(m, struct udpiphdr *);
    u_int32_t       word;
    if (sport == -1) {
        sport = ui->ui_sport;
    }
    if (dport == -1) {
        dport = ui->ui_dport;
    }
    bcopy(&sport, &word, 2);
    bcopy(&dport, (caddr_t) & word + 2, 2);
    udp_rewrite_word(m, (int) (long) &((struct udpiphdr *) 0)->ui_sport,
                     word);
}

/*
 * [udp_rewrite_sum]
 *
 * rewrite the checksum of a udp/ip packet.  traverses entire packet if udp
 * checksumming is enabled.
 *
 * Parameters:
 *      m       mbuf chain, entire udpiphdr must be in the first mbuf.
 *
 * Returns:
 *      <void> 
 */
void
udp_rewrite_sum(struct mbuf *m)
{
    struct udpiphdr *udp = mtod(m, struct udpiphdr *);
    struct ip      *ip = (struct ip *) &(udp->ui_i),
                    ipcopy;

    if (udp->ui_sum != 0 && udpcksum) {
        /*
         * remember fields that are filled in after the checksum
         * was generated, zero them, compute the new checksum, and
         * restore the fields.  (receiver zeros them before
         * checksumming.)
         */
        bcopy(ip, &ipcopy, sizeof(struct ip));
        bzero(((struct ipovly *) ip)->ih_x1, 9);
        ((struct ipovly *) ip)->ih_len = udp->ui_ulen;

        udp->ui_sum = 0;
        if ((udp->ui_sum = in_cksum(m, m->m_pkthdr.len)) == 0)
            udp->ui_sum = 0xffff;
        bcopy(&ipcopy, ip, sizeof(struct ip));
    }
}
