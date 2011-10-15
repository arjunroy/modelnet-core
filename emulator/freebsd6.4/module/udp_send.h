/*
 * modelnet  udp_send.h
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
int udp_send_out_complete __P((struct mbuf * m));

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
int udp_send_out __P((struct in_addr to, short sport, short dport,
                      struct mbuf * m));

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
int udp_send_in_complete __P((struct mbuf * m));

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
int udp_send_in __P((struct in_addr from, short sport, short dport,
                     struct mbuf * m));
