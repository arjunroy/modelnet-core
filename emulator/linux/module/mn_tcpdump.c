/*
 * modelnet mn_tcpdump.c
 *
 *    captures packet traces for specified links
 *
 * Copyright (c) 2006 Calvin Hubble
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

#ifdef MN_TCPDUMP

#include <linux/module.h>
#include <linux/init.h>

#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>

#include <linux/vmalloc.h>

#include "ip_modelnet.h"
#include "mn_tcpdump.h"

/* Used eventually push out buffer when not much data is flowing */
unsigned int tcpdump_tick_counter=0;

int traceLinkCount = 0;

static unsigned char *tempPtr = NULL;
static unsigned char * tcpdump_holdbuf = NULL;
static unsigned char * tcpdump_storebuf = NULL;
static unsigned int tcpdump_holdbuf_len=0;
static unsigned int tcpdump_storebuf_len=0;

static int tcpdump_dropcount = 0;
static unsigned int tcpdump_capturelength = 50;

static DECLARE_WAIT_QUEUE_HEAD(tcpdump_wait_queue);

#define MN_TCPDUMP_CTL_ID 600

#define ROTATE_TCPDUMP_BUFFERS() \
tempPtr = tcpdump_holdbuf; \
tcpdump_holdbuf = tcpdump_storebuf; \
tcpdump_holdbuf_len = tcpdump_storebuf_len; \
tcpdump_storebuf = tempPtr; \
tcpdump_storebuf_len = 0;


struct mn_tcpdump_hdr 
{
  struct timeval tv;
  unsigned int linkId;
  unsigned int	packetLength;
  unsigned int captureLength;
};

static int proc_read_dumps(ctl_table *table, int write,
			   void __user *buffer, size_t *lenp, loff_t *ppos);

ctl_table mn_tcpdump_table[] = {
    {
	.ctl_name = MN_TCPDUMP_CTL_ID,
	.procname = "dropcount", /* kernel-esque naming */
	.data = &tcpdump_dropcount,
	.maxlen = sizeof(int),
	.mode = 0444, /* read-only */
	.child = NULL,
	.proc_handler = &proc_dointvec,
	.strategy = &sysctl_intvec,
    },
    {
	.ctl_name = MN_TCPDUMP_CTL_ID+1,
	.procname = "capturelength",/* kernel-esque naming */
	.data = &tcpdump_capturelength,
	.maxlen = sizeof(unsigned int),
	.mode = 0666, /* read-write */
	.child = NULL,
	.proc_handler = &proc_dointvec,
	.strategy = &sysctl_intvec,
    },
    {
	.ctl_name = MN_TCPDUMP_CTL_ID+2,
	.procname = "tracelinkcount",/* kernel-esque naming */
	.data = &traceLinkCount,
	.maxlen = sizeof(unsigned int),
	.mode = 0444, /* read only */
	.child = NULL,
	.proc_handler = &proc_dointvec,
	.strategy = &sysctl_intvec,
    },
    {
	.ctl_name = MN_TCPDUMP_CTL_ID+3,
	.procname = "dumps",
	.data = NULL,
	.maxlen = 0,
	.mode = 0444,
	.child = NULL,
	.proc_handler = &proc_read_dumps,
	.strategy = NULL,
    },
    {0}
};


inline void tcpdump_handle_inactivity( void ) 
{
    tcpdump_tick_counter = 0;
    if ((tcpdump_holdbuf_len == 0) && (tcpdump_storebuf_len != 0)) {
	ROTATE_TCPDUMP_BUFFERS();
	wake_up(&tcpdump_wait_queue);
    }    
}

/*
 * [proc_read_dumps]
 *
 * @table:
 * @write:
 * @filep:
 * @buffer:
 * @lenp:
 * @ppos: 
 * 
 * handler for proc file system when user does a read on
 * /proc/sys/modelnet/tcpdump/dumps.  Copies data from the hold buf to
 * user space.  If the entire buffer is finished, it resets ppos to
 * the beginning of file.  A user program reading that proc file will
 * never reach an EOF, rather it will just block waiting for more data
 */

static int proc_read_dumps(ctl_table *table, int write,
			   void __user *buffer, size_t *lenp, loff_t *ppos)
{
    unsigned long bytes_not_copied, bytes_copied;
    unsigned int copy_amount=0;

    if (write) {
	printk("Should not write\n");
	*lenp = 0;
	return -EINVAL;
    }
    
    if ((*ppos >= tcpdump_holdbuf_len) &&
	tcpdump_holdbuf_len) 
    {
	*lenp = 0;
	return 0;
    }

    /* XXX - how to determine if wait_event_interruptible returned b/c
     * of an interrupt or b/c of data.  This current implementation
     * will behave correctly unless we have >1 consumer of
     * tcpdump_holdbuf, which means more than 1 program is reading
     * from the proc file system ...*/

    wait_event_interruptible(tcpdump_wait_queue, 
			     (tcpdump_holdbuf_len != 0));

    
    if (tcpdump_holdbuf_len == 0) {
	printk ("We returned from wait_event but nothin in queue\n");
	return -EINVAL;
    }

    copy_amount = min(tcpdump_holdbuf_len-((unsigned int)*ppos), 
					   (unsigned int)(*lenp));

    bytes_not_copied = copy_to_user(buffer,
				    tcpdump_holdbuf + (*ppos),
				    copy_amount);
    bytes_copied = copy_amount - bytes_not_copied;
    
/*     printk("cpyamt: %d, bytes_copied: %d, holdbuf_len:%d, lenp:%d, ppos:%d\n", */
/* 	   copy_amount, bytes_copied, tcpdump_holdbuf_len, *lenp, *ppos); */

    *ppos += bytes_copied;
    *lenp = bytes_copied;

    /* If the client reads to the end of the buffer, then we know this
     * buffer is done with 
     */

    if (*ppos >= tcpdump_holdbuf_len) {
	local_bh_disable(); tcpdump_holdbuf_len = 0; local_bh_enable();
	*ppos = 0; /* Reset file position to 0, user will never reach
		      EOF */
    }

    return 0;
}


/* 
 * [init_mn_tcpdump_buffers]
 * 
 * Initializes the buffers used for tcpdump.
 */

int init_mn_tcpdump_buffers( void )
{
    if (tcpdump_holdbuf != NULL)
	kfree(tcpdump_holdbuf);
    if (tcpdump_storebuf != NULL)
	kfree(tcpdump_storebuf);

    tcpdump_holdbuf = (unsigned char *) vmalloc(TCPDUMP_BUF_SIZE);
    if (!tcpdump_holdbuf)
	return ENOMEM;
    
    tcpdump_storebuf = (unsigned char *) vmalloc(TCPDUMP_BUF_SIZE);
    if (!tcpdump_storebuf)
	return ENOMEM;
    
    tcpdump_dropcount = 0;
    tcpdump_storebuf_len = 0;
    tcpdump_holdbuf_len = 0;
    
    tcpdump_tick_counter = 0;
    return 0;
}


/*
 * [handle_tcpdump]
 *
 * @pkt: the packet in question
 * @hop: the hop the packet is traversing
 * @willdrop_plr: whether or not the packet will be dropped due to plr
 * @willdrop_bw: wtherh or not the pkt will be dropped due to bandwidth
 * 
 * Handles when we need to dump a link.  This is only called from
 * emulate_hop, but for readability purposes I put it in here (this
 * the inline-ness).  Returns 0 on success
 *
 */

inline int handle_tcpdump(struct packet *pkt, struct hop *hop, 
			  int willdrop_plr, int willdrop_bw)
{
    unsigned int packetLen;
    struct mn_tcpdump_hdr *mn_hdr;
    struct iphdr *iph;

    if (!hop->traceLink) {
	return 0;
    }

    /* Not done this case - multi-core */
    if (pkt->skb == NULL) {
	printk("skb is NULL\n");
	return 0;
    }    

    packetLen = pkt->skb->len;
    if ( (tcpdump_storebuf_len + packetLen + 
	  sizeof(struct mn_tcpdump_hdr)) >= TCPDUMP_BUF_SIZE) 
    {
	if (tcpdump_holdbuf_len == 0) {
	    tcpdump_tick_counter = 0;
	    ROTATE_TCPDUMP_BUFFERS();
	    wake_up(&tcpdump_wait_queue);
	} else {
	    ++tcpdump_dropcount;
	    return 0;
	}
    }

    /* Copy in the modelnet header */
    memset( tcpdump_storebuf + tcpdump_storebuf_len, 0, 
	    sizeof(struct mn_tcpdump_hdr));

    mn_hdr = (struct mn_tcpdump_hdr *) (tcpdump_storebuf + 
					tcpdump_storebuf_len);
    /* XXX get micro time */
    /* getmicrotime(&(mn_hdr.tv)); */
    do_gettimeofday(&(mn_hdr->tv));
    /* memcpy(&(mn_hdr->tv), &pkt->skb->stamp, sizeof(struct timeval)); */
    mn_hdr->linkId = hop->id;
    mn_hdr->packetLength = packetLen;
    
    if (tcpdump_capturelength == 0)
	mn_hdr->captureLength = packetLen;
    else
	mn_hdr->captureLength = min(packetLen, 
				    tcpdump_capturelength);
    tcpdump_storebuf_len += sizeof(struct mn_tcpdump_hdr);
    
    
    /* Copy in the actual packet data (starting at IP) */
    if (memcpy(tcpdump_storebuf + tcpdump_storebuf_len,
	       (unsigned char *)ip_hdr(pkt->skb),
	       mn_hdr->captureLength) == NULL)
    {
	printk ("Error with mn_tcpdump copy\n");
	return ENOBUFS;
    }
    iph = (struct iphdr *) (tcpdump_storebuf + tcpdump_storebuf_len);
    tcpdump_storebuf_len += mn_hdr->captureLength;
    
    /* Modelnet fixes - requested by Chip */
    iph->saddr = iph->saddr & 0xFFFF7FFF;
    iph->daddr = iph->daddr & 0xFFFF7FFF;
    
    if (willdrop_plr)
	iph->tos = 1;
    else if(willdrop_bw)
	iph->tos = 2;
    else
	iph->tos = 0;
    
    return 0;	 
}

#endif
