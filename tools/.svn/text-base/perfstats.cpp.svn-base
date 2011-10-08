/*************
 perfstats turns log files output by netlogging into columnar text output of the specified fields.  The data all come from stats in the log file.
*************/

#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "logreader.h"
#include <mn_log.h>

#ifndef IP_ADDR_QUAD_H
#define IP_ADDR_QUAD_H(x) (((x)>>24)&0xff), (((x)>>16)&0xff), (((x)>>8)&0xff), ((x)&0xff)
#endif

#ifndef IP_ADDR_QUAD_N
#define IP_ADDR_QUAD_N(x) IP_ADDR_QUAD_H(ntohl(x))
#endif

char buffer[32768];

const int HDR_SIZE = sizeof(struct net_log_header);

double initial_clock=0;

typedef struct modelnet_stats_record StatsEntry;
typedef struct mn_stats Stats;

double getTimestamp(Stats *s)
{
    return s->time/10000.0-initial_clock;
}

double getCpu(Stats* a, Stats* b)
{
    long diff[5];
    double sum =0 ;
    int i;
    for (i=0;i<5;i++)
    {
	diff[i]=b->cp_time[i]-a->cp_time[i];
	sum+=diff[i];
    }
    return 1.0-diff[4]/(sum >0 ? sum : 1.0);
}

class PerfStat
{
public:
    virtual double getStat(Stats* now, Stats* last, double dt){return -1;}
    virtual string name(){return "none";}
};

class StatCpu : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return getCpu(last,now);}
    string name(){return "cpu";}
};

class StatTicksMissed : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->ticks_missed-last->ticks_missed)/dt);}
    string name(){return "ticks_missed";}
};

class StatMaxPackets : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int(now->max_packets_per_hopclock);}
    string name(){return "max_packets";}
};

class StatTicksForMaxPackets : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int(now->ticks_for_max_packets);}
    string name(){return "max_packets_ticks";}
};

class StatSeen : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->pkts_seen-last->pkts_seen)/dt);}
    string name(){return "pkts_seen";}
};

class StatAccepted : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->pkts_accepted-last->pkts_accepted)/dt);}
    string name(){return "pkts_accepted";}
};

class StatFromEdge : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->pkts_from_edge-last->pkts_from_edge)/dt);}
    string name(){return "pkts_from_edge";}
};

class StatMCSend : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->mc_send-last->mc_send)/dt);}
    string name(){return "mc_send";}
};

class StatMCRecv : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->mc_recv-last->mc_recv)/dt);}
    string name(){return "mc_recv";}
};

class StatIntHops : public PerfStat { public:
    double getStat(Stats* now, Stats* last, double dt){return int((now->pkt_hops-last->pkt_hops)/dt);}
    string name(){return "pkt_hops";}
};

vector<PerfStat*> allStats;
map<string, PerfStat*> statMap;
void initStats()
{
    allStats.push_back(new StatCpu);
    allStats.push_back(new StatMaxPackets);
    allStats.push_back(new StatTicksForMaxPackets);
    allStats.push_back(new StatTicksMissed);
    allStats.push_back(new StatSeen);
    allStats.push_back(new StatAccepted);
    allStats.push_back(new StatFromEdge);
    allStats.push_back(new StatMCSend);
    allStats.push_back(new StatMCRecv);
    allStats.push_back(new StatIntHops);
    unsigned int i;
    for (i=0;i<allStats.size();i++)
	statMap[allStats[i]->name()]=allStats[i];
}

void usage(char *progname)
{
    unsigned int i;
    cerr << "Usage: "<<progname << " <logfile (- for stdin)> [";
    for (i=0;i<allStats.size();i++)
	cerr << " " << allStats[i]->name();
    cerr << "]" << endl;
    exit(1);
}


int main(int argc , char * argv[])
{
    initStats();
    if (argc<3)
	usage(argv[0]);
    unsigned int i;
    vector<PerfStat*> cols;
    for (i=2;i<(unsigned int)argc;i++)
    {
	if (statMap.count(argv[i]) == 0)
	{
	    cerr << "Unknown stat: "<<argv[i]<<endl;
	    exit(1);
	}
	else
	    cols.push_back(statMap[argv[i]]);
    }
    ifstream fin;
    if (string(argv[1]) != "-")
    {
	fin.open(argv[1]);
	cin=fin;
    }
    LogReader reader(cin);

    struct net_log_header* record=(struct net_log_header*)buffer;
    struct modelnet_stats_record* rs = (struct modelnet_stats_record*)buffer;
    struct mn_stats lastStats;
    double lastTime = 0;
    bool first = true;
    memset(&lastStats,0, sizeof(lastStats));
    while (reader.nextEntry(buffer,32768)) 
    {
	if ((int)record->type != (int)MN_STATS_RECORD)
	    continue;
	if (first) { initial_clock = lastTime = getTimestamp(&rs->stats); first=false;}
	double now = getTimestamp(&rs->stats);
	double dt = now-lastTime;
	cout << setw(12) << now;
	for (i=0;i<cols.size();i++)
	    cout << setw(12) << cols[i]->getStat(&rs->stats, &lastStats, dt);
	cout <<endl;
	lastStats=rs->stats;
	lastTime = now;
    }
}
