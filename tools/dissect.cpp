/*
  Dissect

  Walk a log file, and verify/print all log records to stdout.
  Flag anything unexpected to stderr.

 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include "logreader.h"
#include <mn_log.h>

#ifndef IP_ADDR_QUAD_H
#define IP_ADDR_QUAD_H(x) (((x)>>24)&0xff), (((x)>>16)&0xff), (((x)>>8)&0xff), ((x)&0xff)
#endif

#ifndef IP_ADDR_QUAD_N
#define IP_ADDR_QUAD_N(x) IP_ADDR_QUAD_H(ntohl(x))
#endif

int relative = 0;
int summaries = 0;
int skip = 0;
int tot_pkts = 0;
double tot_error = 0.0;
double tot_error_sq = 0.0;
char buffer[32768];

char *filename = NULL;

int parse_args(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-sum")) {
	    summaries=1;
	} else if (!strcmp(argv[i], "-relative")) {
	    relative=1;
	} else if (!strcmp(argv[i], "-skip")) {
	    if (i+1 < argc) skip=atoi(argv[++i]);
	    else skip=1000;
	} else if (!filename) {
	    filename = argv[i];
	} else {
	    fprintf(stderr, "unknown option \"%s\"\n", argv[i]);
	    return 0;
	}
    }
    return 1;
}

void usage(char *progname)
{
    fprintf(stderr, "usage: %s [-sum]\n", progname);
    fprintf(stderr, "Input comes from stdin, output goes to stdout.\n");
}

static long initial_pclock = 0;
static int first_pclock = 1;

const int HDR_SIZE = sizeof(struct net_log_header);
void prts(struct net_log_header *hdr)
{
    u_int64_t  pcc;

    if (first_pclock && relative) {
	initial_pclock = hdr->pclock;
	first_pclock = 0;
    }
    pcc = (u_int64_t)hdr->pcc;
    printf("%llu.%0llu", hdr->pclock - initial_pclock, (pcc%1000000000));
}

void handleNetlog(struct net_log_header* record)
{
    switch (record->type)
    {
    case NET_LOG_RECORD_TIMESTAMP:
	{
	    struct net_log_timestamp_record* ts = (struct net_log_timestamp_record*)record;
	    prts(record);
	    double sum=ts->cp_time_diff[0]+ts->cp_time_diff[1]+ts->cp_time_diff[2]+ts->cp_time_diff[3]+ts->cp_time_diff[4];
	    double cpuUsage = sum > 0 ? 1.0 - ts->cp_time_diff[4]/ sum : 0;
	    printf(" timestamp cpu usage: %.3f at %ld.%ld\n",cpuUsage*100.0, ts->tv_sec, (ts->tv_usec%1000000)/1000);
	} 
	break;
    case NET_LOG_RECORD_ID:
	{
	    break;
	    struct net_log_id_record *idr = (struct net_log_id_record *)record;
	    prts(record);
	    printf(" ID record: ");
	    printf(" lsn=%d closing=%d ipaddr=0x%x len=%d\n",
		   idr->lsn, idr->closing, idr->ipaddr, record->length);
	} 
	break;
    case MN_WAIT_RECORD:
	{
	    struct modelnet_wait_record *r = (struct modelnet_wait_record *)record;
	    printf(" pipe %d metric %d len %u wait %d usec\n",
		   r->pipe_nr, r->bw, r->len, r->usec);
	} 
	break;
    case MN_STATS_RECORD:
	{
	    struct modelnet_stats_record *r = (struct modelnet_stats_record *)record;
	    printf("stats: seen, acc., ref.:  %d, %d, %d  mcsend: %d\n",  
		   r->stats.pkts_seen, r->stats.pkts_accepted, r->stats.pkts_refused, r->stats.mc_send);
	}
	break;
    case MN_PACKET_RECORD:
	{
	    struct modelnet_packet_record* pr = (struct modelnet_packet_record*)record;
	    unsigned long ip_prefix = (1<<8)|(10<<0);
	    unsigned long from = ip_prefix | (pr->from_ip << 16);
	    unsigned long to = ip_prefix | (pr->to_ip << 16);
	    if (pr->drop == 0)
		printf(" packet entry: %u.%u.%u.%u -> %u.%u.%u.%u hop %d\n",
		       IP_ADDR_QUAD_N(from), IP_ADDR_QUAD_N(to), pr->hop);
	    else 
		printf(" packet  drop: %u.%u.%u.%u -> %u.%u.%u.%u hop %d\n",
		       IP_ADDR_QUAD_N(from), IP_ADDR_QUAD_N(to), pr->hop);
	}
	break;
    case MN_ROUTE_STATS_RECORD:
	{
	    struct modelnet_route_stats_record* rs = (struct modelnet_route_stats_record*)record;
	    printf("Routestats: %d entries (%d total bytes).\n",rs->count, record->length);
	}
	break;
    default:
	printf("unrecognized entry type: %d\n",record->type);
	break;
    }
}

int main(int argc , char * argv[])
{
    if (!parse_args(argc, argv)) {
	usage(argv[0]);
	exit(1);
    }
    ifstream fin;
    if (filename != 0)
    {
	fin.open(filename);
	cin=fin;
    }
    LogReader reader(cin);

    struct net_log_header* record=(struct net_log_header*)buffer;
    while (reader.nextEntry(buffer,32768)) 
    {
	handleNetlog(record);
    }
	
    if (summaries) {
	if (tot_pkts == 0) {
	    printf("summary: %d pkts error ave/stddev %4.3f/%2.3f\n",
		   0, 0.0, 0.0);
	} else {
	    double ave = tot_error/tot_pkts;
	    double std = sqrt(tot_error_sq/tot_pkts - ave*ave);
	    printf("summary: %d pkts error ave/stddev %4.3f/%2.3f\n",
		   tot_pkts, ave, std);
	}
    }
}
