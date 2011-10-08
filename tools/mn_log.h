/* $Id: mn_log.h 109 2002-10-30 04:23:04Z eade $ */
#ifndef _MN_LOG_H
#define _MN_LOG_H

#include <net_log_format.h>
#include <ip_modelnet_struct.h>

enum {
    MN_STATS_RECORD=NET_LOG_RECORD_USER+1,
    MN_WAIT_RECORD,
    MN_HBADNESS_RECORD,
    MN_PBADNESS_RECORD,
    MN_PACKET_RECORD,
    MN_ROUTE_STATS_RECORD
};

struct modelnet_packet_record {
    struct net_log_header head;
    unsigned short  from_ip;
    unsigned short  to_ip;
    unsigned short hop;
    unsigned short drop;
};

struct modelnet_stats_record {
    struct net_log_header head;
    struct mn_stats stats;
};

struct modelnet_route_stats_record {
    struct net_log_header head;
    int count;
    struct mn_route_usage stats[0];
};

struct modelnet_wait_record { 
    struct net_log_header head;
    int pipe_nr;
    int bw;
    unsigned int len;
    int usec;
};

struct modelnet_hbadness_record { 
    struct net_log_header head; 
    int from_nr;
    int to_nr;
    unsigned int len;
    int usec;
};

struct modelnet_pbadness_record {
    struct net_log_header head; 
    unsigned int from_ip;
    int to_ip;
    unsigned int len;
    int usec;
};

/* macros for each subtype */
extern void initEventHeader(struct net_log_header* r, int size, int type);
extern char mn_log_buffer[];

#define MN_LOG_STATS(e_stats) do {                                      \
        if (net_log_enabled) {                                          \
                struct modelnet_stats_record _er;                       \
                initEventHeader(&_er.head,sizeof(_er), MN_STATS_RECORD);                          \
                _er.stats = (e_stats);                                  \
		(net_log_appendp)(&_er, sizeof(_er));			\
        }                                                               \
} while (0)

#define MN_LOG_WAIT(e_pipenr, e_bw, e_len, e_usec) do {                 \
        if (net_log_enabled) {                                          \
                struct modelnet_wait_record _er;                        \
                initEventHeader(&_er.head,sizeof(_er), MN_WAIT_RECORD); \
                _er.pipe_nr = (e_pipenr);                               \
                _er.bw = (e_bw);                                        \
                _er.len = (e_len);                                      \
                _er.usec = (e_usec);                                    \
		(net_log_appendp)(&_er, sizeof(_er));			\
        }                                                               \
} while (0)

#define MN_LOG_HBADNESS(e_fromnr, e_tonr, e_len, e_usec) do {            \
        if (net_log_enabled) {                                          \
                struct modelnet_hbadness_record _er;                     \
                initEventHeader(&_er.head,sizeof(_er), MN_HBADNESS_RECORD); \
                _er.from_nr = (e_fromnr);                               \
                _er.to_nr = (e_tonr);                                   \
                _er.len = (e_len);                                      \
                _er.usec = (e_usec);                                    \
		(net_log_appendp)(&_er, sizeof(_er));			\
        }                                                               \
} while (0)

#define MN_LOG_PBADNESS(e_fromip, e_toip, e_len, e_usec) do {            \
        if (net_log_enabled) {                                          \
                struct modelnet_pbadness_record _er;                     \
                initEventHeader(&_er.head,sizeof(_er), MN_PBADNESS_RECORD);\
                _er.from_ip = (e_fromip);                               \
                _er.to_ip = (e_toip);                                   \
                _er.len = (e_len);                                      \
                _er.usec = (e_usec);                                    \
		(net_log_appendp)(&_er, sizeof(_er));			\
        }                                                               \
} while (0)

#define MN_LOG_PACKET(e_fromip, e_toip, e_hop, e_drop) do {            \
        if (net_log_enabled) {                                          \
                struct modelnet_packet_record _er;                     \
                initEventHeader(&_er.head,sizeof(_er), MN_PACKET_RECORD); \
                _er.from_ip = ((e_fromip)&0xFFFF0000) >> 16;                               \
                _er.to_ip = ((e_toip)&0xFFFF0000) >> 16;                                   \
                _er.hop = (e_hop);                                      \
                _er.drop = (e_drop);                                    \
		(net_log_appendp)(&_er, sizeof(_er));			\
        }                                                               \
} while (0)


#define MN_LOG_ROUTE_USAGE(e_count, e_data) do {            \
        if (net_log_enabled) {                                          \
                struct modelnet_route_stats_record _er;                     \
                initEventHeader(&_er.head,sizeof(_er)+e_count*sizeof(struct mn_route_usage), MN_ROUTE_STATS_RECORD);  \
                _er.count = e_count;                                    \
                memcpy(mn_log_buffer, &_er, sizeof(_er));    \
                memcpy(mn_log_buffer+sizeof(_er), e_data, e_count*sizeof(struct mn_route_usage)); \
		(net_log_appendp)(mn_log_buffer, sizeof(_er)+e_count*sizeof(struct mn_route_usage));			\
        }                                                               \
} while (0)

#define MN_LOG_FLUSH() do {            \
        if (net_log_enabled) {         \
	   (net_log_forcep)();            \
        }                              \
} while (0)

#endif /* _MN_LOG_H */
