/*
 * modelnet  mn_pathtable.c
 *
 *     Full table of routes between all edge nodes.
 *     Requires (nodes*nodes) space, so only scales to 5000 nodes or so.
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
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>

#include "ip_modelnet.h"
#include "mn_pathtable.h"
#include "mn_tcpdump.h"

int      nodetable[NODEMASK + 1];
int      hopcount = 0;
static struct hop *hoptable = NULL;
int nodecount  = 0;
static struct hop ****pathtable = NULL;


/** 
 * lookup_path - this is unmodified from BSD version of modelnet
 *
 */

struct hop    **
lookup_path(in_addr_t src, in_addr_t dst) 
{
    int             srcid = nodetable[ntohl(src) & NODEMASK];
    int             dstid = nodetable[ntohl(dst) & NODEMASK];
    if (srcid == -1 || dstid == -1 || pathtable == NULL)
    {
      printk ("lookup_path: no path for %x -> %x .... %x -> %x\n",
	      src, dst, ntohl(src) & NODEMASK, ntohl(dst) & NODEMASK);
      printk ("lookup_path: srcid(%d), dstid(%d), pathtable(%lx)\n",
	      srcid, dstid, (unsigned long)pathtable);
      return NULL;
    }
    return pathtable[srcid][dstid];
}



/** 
 * free_path - this is more-or-less unmodified from BSD version
 * of modelnet
 */

static void
free_path(void)
{
  int             i, j;

  if (pathtable && nodecount) 
  {
    for (i = 0; i < nodecount; ++i)
    {
      for (j = 0; j < nodecount; ++j)
      {
	if (pathtable[i][j]) 
	{
	  kfree(pathtable[i][j]);    
	}
      }
    }
    for (i = 0; i < nodecount; ++i)
    {
      kfree(pathtable[i]);
    }
    kfree(pathtable);
  }
  pathtable = NULL;
  nodecount = 0;
}


  
/* uninit_paths[]
 *
 * remove paths, but also remove each hop.  This function is
 * more-or-less unmodified from BSD version of modlenet
 */
void
uninit_paths(void)
{
  int i;

  free_path();
  if (hoptable) 
  {
    for (i = 0; i < hopcount; ++i)
    {
      kfree(hoptable[i].exittick);
      kfree(hoptable[i].slotlen);
    }
    
    vfree(hoptable);
  }
  hoptable = NULL;
  hopcount = 0;
}


/**
 * proc_nodetable - handles a user write to the nodetable.  This proc
 * should be write-only.  Apparently writes to the /proc filesystem
 * occur only in 4096 byte increments (regardless of what happens in
 * user-space), so ppos keeps track of our current offset in the file.
 *   
 * @table: the sysctl table
 * @write: %TRUE if this is a write to the sysctl file
 * @buffer: the user buffer
 * @lenp: the size of the user buffer
 * @ppos: current offset into the file
 * 
 * Returns 0 on success.  Not sure why the error values are negated,
 * but similar linux-module code seems to do that and errors are
 * passed down to the user correctly
 */

int
proc_nodetable(ctl_table *table, int write,
	       void __user *buffer, size_t *lenp, loff_t *ppos)
{
  /* Make sure we are doing a valid write amount */
  if ( ((*ppos) + (*lenp)) > (4 *(NODEMASK+1)))
  {
    printk("proc_nodetable: invalid ppos(%llu) or lenp(%d)\n",
	    *ppos, (int)*lenp);
    return -EINVAL;
  }

  /* Have not implemented reading from nodetable - should be marked
     as write-only in ip_modelnet.c */
  if (!write)
  { 
    printk("proc_nodetable: Error, expecting only writes but got a read\n"); 
    *lenp = 0; /* Read will keep calling until lenp = 0 */
    return -EINVAL;
  }
  
  if (copy_from_user( ((void*)nodetable)+(*ppos), buffer, *lenp ))
  {
    printk("proc_nodetable: copy_from_user failed\n");
    return -EFAULT;
  }
  *ppos += *lenp;

  return 0;
}



/**
 * proc_nodecount - handles changing of nodecount variable.  Uses the
 * pre-defined linux do_intvec method to update the nodecount variable
 * (this variable is treated as a string then converted to an int in
 * do_intvec).  After we update the nodecount variable, we resize
 * various data structures.
 *   
 * @table: the sysctl table
 * @write: %TRUE if this is a write to the sysctl file
 * @buffer: the user buffer
 * @lenp: the size of the user buffer
 * @ppos: current offset into the file
 * 
 * Returns 0 on success.  Not sure why the error values are negated,
 * but similar linux-module code seems to do that and errors are
 * passed down to the user correctly
 */

int
proc_nodecount(ctl_table *table, int write,
	       void __user *buffer, size_t *lenp, loff_t *ppos)
{
  int i;
  int             oldcount = nodecount;
  
  /* Use linux's procedure for copying integers from user space
   * nodecount variable is treated as an int vec of size 1.  It is
   * also important to note that proc_dointvec expects the integer
   * represented as a string then does the conversion from string ->
   * integer
   */
  proc_dointvec(table, write, buffer, lenp, ppos);

  /* If we are just doing a read, then return now.  proc_dointvec
   * would have taken care of copying to user space and the rest of
   * this function reallocates data structures
   */
  if (!write)
    return 0;
  
  if (oldcount != nodecount) 
  {

    /* Had to change values around b/c we need old nodecount value in
     * order to free the path.  However, proc_dointvec is set to
     * override the nodecount variable.
     */
    int newcount = nodecount;
    nodecount = oldcount;
    
    free_path();            /* free_path requires the old nodecount */
    nodecount = newcount;
    
    /* XXX - BSD version has M_WAITOK, but in the Linux kernel we have
     * interrupts disabled so I think we have to do this kmalloc
     * atomically.  This procedure is not in the critical path so I
     * did not give it much thought
     */
    pathtable = kmalloc(nodecount * sizeof(struct hop **),
			GFP_ATOMIC);
    if (!pathtable) 
    {
      free_path();
      return -ENOMEM;
    }

    for (i = 0; i < nodecount; ++i) 
    {
      /* See comment above regarding M_WAITOK and interrupts */
      pathtable[i] = kmalloc(nodecount * sizeof(struct hop *), GFP_ATOMIC);
      if (!pathtable[i]) 
      {
        nodecount=i-1;
        free_path();
        return -ENOMEM;
      }
      memset(pathtable[i], 0, nodecount * sizeof(struct hop *));
    }
  }

  return 0;
}



/**
 * proc_hophandle - not sure what happens when we are given a write
 * with ppos not = to 0.
 *   
 * @table: the sysctl table
 * @write: %TRUE if this is a write to the sysctl file
 * @buffer: the user buffer
 * @lenp: the size of the user buffer
 * @ppos: current offset into the file
 * 
 * Returns 0 on success.  Not sure why the error values are negated,
 * but similar linux-module code seems to do that and errors are
 * passed down to the user correctly
 */

int
proc_hophandle(ctl_table *table, int write,
	       void __user *buffer, size_t *lenp, loff_t *ppos)
{
  void __user *userHopsPtr;
  int error, i;
  
  struct sysctl_hoptable tab;
  struct sysctl_hop *hops;
  
#ifdef MN_TCPDUMP
  traceLinkCount = 0;
#endif
  
  /* hophandle should be set as write-only when registering in
   * ip_modelnet.c so this situation should not come up
   */
  if (!write) 
  {
    printk("proc_hophandle: should be write-only\n");
      *lenp=0;
    return -EINVAL;
  }
  
  /* XXX This might and probably will not work for buffers > 4096.
   * Todo - try to load a hop table that is bigger than 4096
   * bytes. It seems that all writes to a /proc file are broken up
   * somewhere in the kernel into multiple writes of 4096 bytes each.
   */
  if (*ppos != 0)
  {
    printk ("Not implemented yet\n");
    return -EFAULT;
  }
  
  if ((*lenp) != sizeof(tab))
  {
    
    printk ("ERROR Lenp(%d), sizeoftab(%lu)\n", (int)*lenp, (unsigned long)sizeof(tab));
    return -EINVAL;
  }

    
  /* Copy from user returns the number of bytes that could NOT be
   * copied and 0 on successs
   */
  error = copy_from_user(&tab, buffer, sizeof(tab));
  *ppos = sizeof(tab);
  if (error) 
  {
    printk("Copy returned %d (0 on success), sizeoftab(%lu)\n",
	   error, (unsigned long)sizeof(tab));
    return -EFAULT;
  }
    
  /* Switched to vmalloc as this was unstable for large topologies */
  /* hops = kmalloc(tab.hopcount * sizeof(*hops), GFP_KERNEL); */
  hops = vmalloc(tab.hopcount * sizeof(*hops));

  if (!hops) 
  {
    printk("copyin buf alloc failed\n");
    return -ENOMEM;
  }

  /* Copy from tab.hops to hops, tab.hops is a pointer to user-level
   * address space ...  XXX - since we are using the proc
   * filesystem, we can forgoe the entire sysctl_hoptable
   * datastructure altogether and just do a write of a certain size
   * (then check how long the write is!)  Leave this task for
   * another day ... for now I will try to keep the bsd code intact
   */
  userHopsPtr = tab.hops;
        
  if(copy_from_user(hops, userHopsPtr, tab.hopcount * sizeof(*hops)))
  {
    printk("Could not copy from user from usrHopsPtr\n");
    return -EFAULT;
  }
  uninit_paths();

  hoptable = vmalloc(tab.hopcount * sizeof(*hoptable));
  if (!hoptable) 
  {
    printk("hoptable alloc failed. (%lu KB)\n",
	   (unsigned long)(tab.hopcount * sizeof(*hoptable)/1024));
    return -ENOMEM;
  }
  memset(hoptable, 0, tab.hopcount * sizeof(*hoptable));

  hopcount = tab.hopcount;
  for (i = 0; i < hopcount; ++i) 
  {
    struct hop     *hop = hoptable + i;

    /* printk("Info about hop #%d, bandwidth(%d), delay(%d), plr(%d), qsize(%d), emulator(%d), xtq_type(%d)\n", i,hops[i].bandwidth, hops[i].delay, hops[i].plr, hops[i].qsize, hops[i].emulator, hops[i].xtq_type); */

    spin_lock_init(&hop->lock);
    hop->KBps = hops[i].bandwidth/8;
    hop->delay = (HZ * hops[i].delay) / 1000;
    hop->plr = hops[i].plr;
    hop->qsize = hops[i].qsize;
    hop->emulator = hops[i].emulator;
    hop->bytespertick = hop->KBps*1000/HZ;
    hop->id = i;
#ifdef MN_TCPDUMP
    hop->traceLink = hops[i].traceLink;
    if(hop->traceLink) {      
      ++traceLinkCount;
      printk("Tracing link %d\n", i);
    }
#endif

#if 0
    printk("hoptable: idx(%d) bw(%d) delay(%d) plr(%d) qsize(%d), hz(%d)\n",
	   hop->id,hop->KBps, hop->delay, hop->plr, hop->qsize, HZ);
#endif
#ifdef QCALC
    if ((hop->qsize*1500)/hop->KBps > (1<<14)/hz )
      printk("hop %d: max delay %dms may overrun calender period %dms\n", i,
	     (hop->qsize*1500)/hop->KBps , (1<<14)/HZ );
#endif
    
    /*
     * initialize bandwidth queue
     */

    /* usually pretty small, kmalloc should be ok */
    hop->exittick = kmalloc(hop->qsize * sizeof(*hop->exittick),
			    GFP_ATOMIC);
    if (!hop->exittick) {
      printk("hop->exittick alloc failed %lu\n", 
	     (unsigned long)(hop->qsize * sizeof(*hop->exittick)));
      return -ENOMEM;
    }
    memset(hop->exittick, 0, hop->qsize * sizeof(*hop->exittick));
    hop->slotdepth = 0;
    hop->bytedepth = 0;
    hop->headslot = 0;
    hop->fragment = 0;

    hop->slotlen = kmalloc(hop->qsize * sizeof(*hop->slotlen), GFP_ATOMIC);

    if (!hop->slotlen) 
    {
      printk("hop->slotlen alloc failed\n");
      return -ENOMEM;
    }
    memset(hop->slotlen, 0, hop->qsize * sizeof(*hop->slotlen));
  }
  vfree(hops);

  /* Return 0 on success */
  return 0;
}

/**
 * proc_hopstats - User is expected to do a read on the proc
 * filesystem of length hopcount * sizeof(*stattab).  Since it seems
 * that the max read amount for a single sys call is 4096 (page
 * size?), this function is repeatedly called with increasing *ppos
 * values.
 *   
 * @table: the sysctl table
 * @write: %TRUE if this is a write to the sysctl file
 * @buffer: the user buffer
 * @lenp: the size of the user buffer
 * @ppos: current offset into the file
 * 
 * Returns 0 on success.  Not sure why the error values are negated,
 * but similar linux-module code seems to do that and errors are
 * passed down to the user correctly
 */

int
proc_hopstats(ctl_table *table, int write,
	      void __user *buffer, size_t *lenp, loff_t *ppos)
{
  int             i;
  struct sysctl_hopstats *stattab;

  /* Sorta-Kinda-Hack - this check does exactly what is done in
   * proc_dostring.  Without this check if someone were to do a
   * /sbin/sysctl -A or cat /proc/modelnet/sys/hopstats, the result
   * would be simply calling this function over and over with
   * increasing ppos and lenp values.
   */
  if (*ppos && !write) 
  {
    *lenp = 0;
    return 0;
  }

  if (*lenp < (hopcount * sizeof(*stattab))) 
  {
    printk ("hopstats: user buffer is too small\n");
    return -EINVAL;
  }

  stattab = kmalloc(hopcount * sizeof(*stattab), GFP_KERNEL);
  
  if (!stattab) 
  {
    printk("stattab alloc failed\n");
    return -ENOMEM;
  }

  for (i = 0; i < hopcount; ++i) 
  {
    stattab[i].pkts = hoptable[i].pkts;
    stattab[i].bytes = hoptable[i].bytes;
    stattab[i].qdrops = hoptable[i].qdrops;
  }

  if (copy_to_user(buffer, stattab, hopcount * sizeof(*stattab)))
  {
    printk("proc_hopstats: error copying to user\n");
    return -EFAULT;
  } 

  *lenp = (hopcount * (sizeof(*stattab)));
  *ppos += (hopcount * (sizeof(*stattab)));

  kfree(stattab);

  return 0;
}



/**
 * proc_pathentry
 *   
 * @table: the sysctl table
 * @write: %TRUE if this is a write to the sysctl file
 * @buffer: the user buffer
 * @lenp: the size of the user buffer
 * @ppos: current offset into the file
 * 
 * Returns 0 on success.  Not sure why the error values are negated,
 * but similar linux-module code seems to do that and errors are
 * passed down to the user correctly
 */

int
proc_pathentry(ctl_table *table, int write,
	       void __user *buffer, size_t *lenp, loff_t *ppos)
{
  int             error, i;
  struct sysctl_pathentry entry;
  int            *hops;
  void __user *userHopsPtr;

  /* This sysctl should be set as write-only, but just in case ...*/
  if (!write)
  {
    printk("proc_pathentry: ERROR should be write-only\n");
    *lenp=0; /* otherwise reads would keep calling */
    return -EINVAL;
  }
  if (*lenp < sizeof(entry))
  {
    printk("proc_pathentry: ERROR not enough data given\n");
    return -EINVAL;
  }

  error = copy_from_user(&entry, buffer, sizeof(entry));
  *ppos += *lenp;
  if (error)
  {
    printk("pathentry: Error copying data from user\n");
    return -EFAULT;
  }
    
  if (!pathtable || !hoptable)
  {
    printk("pathentry: Pathtable(%lx) or hoptable(%lx) is null\n",
	   (unsigned long)pathtable, (unsigned long)hoptable);
    return -EINVAL;
  }

  if (entry.src_node > nodecount || entry.dst_node > nodecount)
  {
    printk("pathentry: ERROR nodecount issues\n");
    return -EINVAL;
  }

  hops = kmalloc(entry.pathlen * sizeof(*hops), GFP_ATOMIC); 
  /* printk("(int)hops size: %lu\n", entry.pathlen * sizeof(*hops)); */
  if (!hops) 
  {
      printk("hop alloc failed\n");
      return -ENOMEM;
  }
    
  userHopsPtr = entry.hops;
  error = copy_from_user(hops, userHopsPtr, entry.pathlen * sizeof(*hops));
  if (error)
  {
    printk("proc_pathentry: Could not copy from userHopsPtr\n");
    return -EFAULT;
  }
    
  pathtable[entry.src_node][entry.dst_node] = 
    kmalloc((entry.pathlen + 1) * sizeof(struct hop *), GFP_ATOMIC);
    
  if (!pathtable[entry.src_node][entry.dst_node]) 
  {
    printk("path alloc failed\n");
    return -ENOMEM;
  }
  memset(pathtable[entry.src_node][entry.dst_node], 0, 
	 (entry.pathlen + 1) * sizeof(struct hop *));

  for (i = 0; i < entry.pathlen; ++i) 
  {
/*     printk("Loaded entry from %d to %d, (%d)th hop\n", */
/* 	   entry.src_node, entry.dst_node, i); */
    pathtable[entry.src_node][entry.dst_node][i] = hoptable + hops[i];
  }
  pathtable[entry.src_node][entry.dst_node][entry.pathlen] = NULL;

  kfree(hops);
  return 0;
}

