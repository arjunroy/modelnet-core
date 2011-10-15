/*
 * modelnet  udp_rewrite.h
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
void udp_rewrite_src __P((struct mbuf * m, u_int32_t src));

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
void udp_rewrite_dst __P((struct mbuf * m, u_int32_t dst));

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
void udp_rewrite_ports __P((struct mbuf * m, short sport, short dport));

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
void udp_rewrite_sum __P((struct mbuf * m));

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
void udp_rewrite_misc __P((struct mbuf * m, int off, int len, void *data));
