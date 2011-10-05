/*
 * modelnet  mn_pathtable.h
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

#ifndef __MN_PATHTABLE_H
#define __MN_PATHTABLE_H
#define NODEMASK 0xffff

extern int hopcount;
extern int nodetable[NODEMASK+1];

extern spinlock_t topology_lock;

extern void uninit_paths(void);

extern int proc_nodecount(ctl_table *table, int write,
			   void __user *buffer, size_t *lenp, loff_t *ppos);

extern int proc_hophandle(ctl_table *table, int write,
			  void __user *buffer, size_t *lenp, loff_t *ppos);
extern int nodecount;

extern int proc_pathentry(ctl_table *table, int write,
			  void __user *buffer, size_t *lenp, loff_t *ppos);

extern int proc_nodetable(ctl_table *table, int write,
			  void __user *buffer, size_t *lenp, loff_t *ppos);

extern int proc_hopstats(ctl_table *table, int write,
			 void __user *buffer, size_t *lenp, loff_t *ppos);

extern struct hop ** lookup_path(in_addr_t src, in_addr_t dst);

#endif
