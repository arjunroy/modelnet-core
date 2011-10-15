/*
 * modelnet  mn_pathtable.c
 *
 *     Full table of routes between all edge nodes.
 *     Requires (nodes*nodes) space, so only scales to 5000 nodes or so.
 *
 * author David Becker
 *
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 *
 */

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

#include "ip_modelnet.h"
#include "mn_xtq.h"

#define NODEMASK 0xffff
static int      nodetable[NODEMASK + 1];

static int      hopcount = 0;
static struct hop *hoptable = NULL;

static int      nodecount = 0;
static struct hop ****pathtable = NULL;

struct hop    **
lookup_path(in_addr_t src, in_addr_t dst)
{
    int             srcid = nodetable[ntohl(src) & NODEMASK];
    int             dstid = nodetable[ntohl(dst) & NODEMASK];
    if (srcid == -1 || dstid == -1 || pathtable == NULL)
        return NULL;

    return pathtable[srcid][dstid];
}

static void
free_path(void)
{
    int             i, j;

    if (pathtable && nodecount) {
        for (i = 0; i < nodecount; ++i)
            for (j = 0; j < nodecount; ++j)
                if (pathtable[i][j])
                    free(pathtable[i][j], M_MN_ROUTE);
        for (i = 0; i < nodecount; ++i)
            free(pathtable[i], M_MN_ROUTE);
        free(pathtable, M_MN_ROUTE);
    }
    pathtable = NULL;
    nodecount = 0;
}

/* uninit_paths[]
 *
 * remove paths, but also remove each hop 
 */
void
uninit_paths(void)
{
    int             i;

    mtx_lock(mn_mtx);
    
    free_path();
    if (hoptable) {
	    for (i = 0; i < hopcount; ++i){
		    xtq_uninstall(&(hoptable[i]),-1);
		    free(hoptable[i].exittick, M_MN_CONF);
		    free(hoptable[i].slotlen, M_MN_CONF);
	    }
	    free(hoptable, M_MN_CONF);
    }
    hoptable = NULL;
    hopcount = 0;

    mtx_unlock(mn_mtx);
}


SYSCTL_OPAQUE(_net_inet_ip_modelnet, OID_AUTO, nodetable,
              CTLFLAG_RW, &nodetable, sizeof nodetable,
              "LU",
              "Modelnet nodetable maps IP&NODEMASK to path table index");

static int
sysctl_nodecount(SYSCTL_HANDLER_ARGS)
{
    int             error, i;
    int             newcount = nodecount;


    error = sysctl_handle_int(oidp, &newcount, 0, req);
    if (error || !req->newptr)
        return (error);

    mtx_lock(mn_mtx);

    if (newcount != nodecount) {
        free_path();            /* free_path requires the old nodecount */
        nodecount = newcount;

        pathtable = malloc(nodecount * sizeof(struct hop **),
                           M_MN_ROUTE, M_WAITOK | M_ZERO);
        if (!pathtable) {
            free_path();
	    mtx_unlock(mn_mtx);
            return ENOMEM;
        }
        for (i = 0; i < nodecount; ++i) {
            pathtable[i] = malloc(nodecount * sizeof(struct hop *),
                                  M_MN_ROUTE, M_WAITOK | M_ZERO);
            if (!pathtable[i]) {
		nodecount=i-1;
                free_path();
		mtx_unlock(mn_mtx);
                return ENOMEM;
            }
        }
    }

    mtx_unlock(mn_mtx);
    
    return 0;
}

SYSCTL_PROC(_net_inet_ip_modelnet, OID_AUTO, nodecount,
            CTLTYPE_INT | CTLFLAG_RW, &nodecount, 0, sysctl_nodecount,
            "I", "Modelnet virtual node count");

SYSCTL_INT(_net_inet_ip_modelnet, OID_AUTO, hopcount,
           CTLFLAG_RD, &hopcount, 0, "Number of hops (virtual links)");

static int
sysctl_hophandle(SYSCTL_HANDLER_ARGS)
{
    int             error,
                    i;
    struct sysctl_hoptable tab;
    struct sysctl_hop *hops;

    error = SYSCTL_IN(req, &tab, sizeof(tab));
    if (error || !req->newptr) {
        if (!req->oldptr)
            error = SYSCTL_OUT(req, &hopcount, sizeof(hopcount));
        return error;
    }

    hops = malloc(tab.hopcount * sizeof(*hops), M_TEMP, M_WAITOK);
    if (!hops) {
        printf("copyin buf alloc failed\n");
        return ENOMEM;
    }
    copyin(tab.hops, hops, tab.hopcount * sizeof(*hops));

    uninit_paths();

    hoptable = malloc(tab.hopcount * sizeof(*hoptable),
                      M_MN_CONF, M_WAITOK | M_ZERO);

    mtx_lock(mn_mtx);
    
    if (!hoptable) {
        printf("hop alloc failed. (%d KB)\n",
		tab.hopcount * sizeof(*hoptable)/1024);
        
	mtx_unlock(mn_mtx);

        return ENOMEM;
    }

    hopcount = tab.hopcount;

    for (i = 0; i < hopcount; ++i) {
        struct hop     *hop = hoptable + i;

        hop->KBps = hops[i].bandwidth/8; 
        hop->delay = (hz * hops[i].delay) / 1000;
        hop->plr = hops[i].plr;
        hop->qsize = hops[i].qsize;
        hop->emulator = hops[i].emulator;
	hop->bytespertick = hop->KBps*1000/hz;
        hop->id = i;

#if 0
	printf("hoptable: idx(%d) bw(%d) delay(%d) plr(%d) qsize(%d)\n",
	   hop->id,hop->KBps, hop->delay, hop->plr, hop->qsize);
#endif
#ifdef QCALC
if ((hop->qsize*1500)/hop->KBps > (1<<14)/hz ) 
	printf("hop %d: max delay %dms may overrun calender period %dms\n", i,
		(hop->qsize*1500)/hop->KBps , (1<<14)/hz ); 
#endif

        /*
         * initialize bandwidth queue 
         */
        hop->exittick = malloc(hop->qsize * sizeof(*hop->exittick),
                              M_MN_CONF, M_WAITOK);
        if (!hop->exittick) {
            printf("hop alloc failed\n");
            
	    mtx_unlock(mn_mtx);
            return ENOMEM;
        }
        bzero(hop->exittick, sizeof(int *) * hop->qsize);
        hop->slotdepth = 0;
        hop->bytedepth = 0;
        hop->headslot = 0;
        hop->fragment = 0;

	hop->slotlen = malloc(hop->qsize * sizeof(*hop->slotlen),
                              M_MN_CONF, M_WAITOK);
        if (!hop->slotlen) {
            printf("hop alloc failed\n");
            
	    mtx_unlock(mn_mtx);
            return ENOMEM;
        } 	
        bzero(hop->slotlen, sizeof(int *) * hop->qsize);

	xtq_install(hop, hops[i].xtq_type);
    }

    free(hops, M_TEMP);
    
    mtx_unlock(mn_mtx);

    error = SYSCTL_OUT(req, &hopcount, sizeof(hopcount));

    return (error);
}

SYSCTL_PROC(_net_inet_ip_modelnet, OID_AUTO, hoptable,
            CTLTYPE_OPAQUE | CTLFLAG_RW, NULL, 0, sysctl_hophandle,
            "S,xlat", "Modelnet hop table");

static int
sysctl_hopmodhandle(SYSCTL_HANDLER_ARGS)
{
    int             error;
    struct sysctl_hopmod mod;
    struct hop     *hop;

    error = SYSCTL_IN(req, &mod, sizeof(mod));
    if (error || !req->newptr) {
        if (!req->oldptr)
            error = SYSCTL_OUT(req, &hopcount, sizeof(hopcount));
        return error;
    }

    if (mod.hopidx>=hopcount) return EINVAL;

    mtx_lock(mn_mtx);

    hop = hoptable + mod.hopidx;

    hop->KBps = mod.hop.bandwidth/8; 
    hop->delay = (hz * mod.hop.delay) / 1000;
    hop->plr = mod.hop.plr;
    /* qsize change not allowed due to re-allocation
     * hop->qsize = mod.hop.qsize;
     */
    hop->emulator = mod.hop.emulator;
    hop->bytespertick = hop->KBps*1000/hz;
    /* XTQ */
    xtq_uninstall(hop, mod.hop.xtq_type);

    mtx_unlock(mn_mtx);
#if 0
    printf("hopmod: idx(%d) bw(%d) delay(%d) plr(%d) qsize(%d)\n",
	   mod.hopidx,hop->KBps, hop->delay, hop->plr, hop->qsize);
#endif


    error = SYSCTL_OUT(req, &hopcount, sizeof(hopcount));

    return (error);
}

SYSCTL_PROC(_net_inet_ip_modelnet, OID_AUTO, hopmod,
            CTLTYPE_OPAQUE | CTLFLAG_RW, NULL, 0, sysctl_hopmodhandle,
            "S,xlat", "Modify hop params");

static int
sysctl_hopstats(SYSCTL_HANDLER_ARGS)
{
    int             error, i;
    struct sysctl_hopstats *stattab;

    stattab = malloc(hopcount * sizeof(*stattab), M_TEMP, M_WAITOK);
    if (!stattab) {
        printf("stattab alloc failed\n");
        return ENOMEM;
    }

    for (i = 0; i < hopcount; ++i) {
        stattab[i].pkts = hoptable[i].pkts;
        stattab[i].bytes = hoptable[i].bytes;
        stattab[i].qdrops = hoptable[i].qdrops;
    }

    error = SYSCTL_OUT(req, stattab, hopcount * sizeof(*stattab));
    free(stattab, M_TEMP);

    return (error);
}

SYSCTL_PROC(_net_inet_ip_modelnet, OID_AUTO, hopstats,
            CTLTYPE_OPAQUE | CTLFLAG_RD, NULL, 0, sysctl_hopstats,
            "S,xlat", "Hop statistics");


static int
sysctl_pathentry(SYSCTL_HANDLER_ARGS)
{
    int             error, i;
    struct sysctl_pathentry entry;
    int            *hops;



    error = SYSCTL_IN(req, &entry, sizeof(entry));
    if (error || !req->newptr) {
        if (!req->oldptr)
            error = SYSCTL_OUT(req, &entry.pathlen, sizeof(entry.pathlen));

        return error;
    }

    if (!pathtable || !hoptable)
        return EINVAL;

    if (entry.src_node > nodecount || entry.dst_node > nodecount)
        return EINVAL;


    hops = malloc(entry.pathlen * sizeof *hops, M_TEMP, M_NOWAIT);
    if (!hops) {
        printf("hop alloc failed\n");
        return ENOMEM;
    }
    copyin(entry.hops, hops, entry.pathlen * sizeof *hops);

    mtx_lock(mn_mtx);

    pathtable[entry.src_node][entry.dst_node] = malloc((entry.pathlen + 1) *
	       sizeof(struct hop *), M_MN_ROUTE, M_NOWAIT | M_ZERO);
    if (!pathtable[entry.src_node][entry.dst_node]) {
        printf("path alloc failed\n");
        
	mtx_unlock(mn_mtx);
        return ENOMEM;
    }
    for (i = 0; i < entry.pathlen; ++i) {
        pathtable[entry.src_node][entry.dst_node][i] = hoptable + hops[i];
    }
    pathtable[entry.src_node][entry.dst_node][entry.pathlen] = NULL;

    free(hops, M_TEMP);

    mtx_unlock(mn_mtx);

    return (error);
}

SYSCTL_PROC(_net_inet_ip_modelnet, OID_AUTO, pathentry,
            CTLTYPE_OPAQUE | CTLFLAG_RW, NULL, 0, sysctl_pathentry,
            "S,xlat", "Modelnet path table entry");






