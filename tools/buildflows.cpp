/* buildFlows takes the route and host info from an MDB
 * and a logfile from a ModelNet run with packet logging enabled,
 * and outputs a flowmap files describing packet flow from 
 * edge to edge.  The format of the file is as follows:
 * Each line represents the outgoing flows for one pipe:
   <src_pipe_nr> <# of flows on this line> <dst_pipe_nr> <#packets> ... <dst_pipe_nr> <#packets>
*/

#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <logreader.h>
#include <ip_modelnet_struct.h>
#include <net_log_format.h>
#include <mn_log.h>
#include "xml.h"

void usage()
{
    cerr<<"usage: buildflows <.model file> <.route file> logfile1 logFile2 ... logFileN"<<endl;
    exit(1);
}

string modelFile;
string routeFile;
vector<string> logFilenames;
int numPipes = 0;

void parseArgs(int argc, char* argv[])
{
  if (argc < 4)
    usage();
  modelFile = argv[1];
  routeFile = argv[2];
  logFilenames.resize(argc-3);
  copy(argv+3, argv+argc, logFilenames.begin());
}

typedef map<int, unsigned long> VNtoIPMap;
typedef map<unsigned long, int> IPtoVNMap;
typedef vector<int> Route;
typedef map<int, set<int> > CommMap;
typedef map<int, map<int, Route> > RoutingTable;
typedef vector<map<int, long long> > FlowMap;

class ClientReader : public XMLStream 
{
public:
  ClientReader(VNtoIPMap& vnToIP, IPtoVNMap& ipToVN) : myVNtoIP(vnToIP), myIPtoVN(ipToVN) {}
  void onElement(const string& name, map<string, string>& attrs) {
    if (name != "virtnode")
      return;
    if (!(attrs.count("int_vn") && attrs.count("vip"))) {
      cerr << "Some nodes missing vn#'s or IPs." << endl;
      exit(1);
    }
    int vn = atoi(attrs["int_vn"].c_str());
    struct in_addr addr;
    if (!inet_aton(attrs["vip"].c_str(), &addr)) {
      cerr << "Invalid IP address." << endl;
      exit(1);
    }
    myVNtoIP[vn]=addr.s_addr;
    myIPtoVN[addr.s_addr]=vn;
  }
  void onError(const string& msg) {
    cerr << "XML error: " << msg << endl;
    exit(1);
  }
private:
  VNtoIPMap& myVNtoIP;
  IPtoVNMap& myIPtoVN;
};

int readClients(VNtoIPMap& vnToIP, IPtoVNMap& ipToVN)
{
  ClientReader reader(vnToIP, ipToVN);
  return streamXML(modelFile, reader);
}

class RoutesReader : public XMLStream 
{
public:
  RoutesReader(CommMap& commMap, RoutingTable& rt) : myComm(commMap), myRoutingTable(rt) {}

  void onElement(const string& name, map<string, string>& attrs) {
    if (name != "path")
      return;
    if (!(attrs.count("int_vnsrc") && attrs.count("int_vndst") && attrs.count("hops"))) {
      cerr << "Some paths missing src, dst, or hops." << endl;
      exit(1);
    }
    int src = atoi(attrs["int_vnsrc"].c_str());
    int dst = atoi(attrs["int_vndst"].c_str());
    if (myComm.count(src) == 0 || myComm[src].count(dst) == 0)
      return;
    istringstream hops(attrs["hops"]);
    Route route;
    int hop;
    while (hops >> hop) {
      if (hop > numPipes)
	numPipes = hop;
      route.push_back(hop);
    }
    myRoutingTable[src][dst]=route;
  }
  void onError(const string& msg) {
    cerr << "XML error: " << msg << endl;
    exit(1);
  }
private:
  CommMap& myComm;
  RoutingTable& myRoutingTable;
};

int readRoutes(CommMap& reqRoutes, RoutingTable& routes)
{
  RoutesReader reader(reqRoutes, routes);
  return streamXML(routeFile, reader);
}

const int BUF_SIZE=10240;
int readLogs(vector<string>& logFiles, IPtoVNMap& ipMap, CommMap& reqRoutes)
{
  char buffer[BUF_SIZE];
  struct net_log_header* record = (struct net_log_header*)buffer;
  struct modelnet_route_stats_record* rstats = (struct modelnet_route_stats_record*)buffer;
  unsigned int i;
  vector<string> useful;
  for (i=0;i<logFiles.size(); i++) {
    bool used = false;
    ifstream fin(logFiles[i].c_str());
    if (fin.fail())
      return false;
    LogReader reader(fin);
    while (reader.nextEntry(buffer,BUF_SIZE)) {
      if ((int)record->type != (int)MN_ROUTE_STATS_RECORD)
	continue;
      used = true;
      struct mn_route_usage* ru=rstats->stats;
      int count = rstats->count;
      while (count--) {
	if (ipMap.count(htonl(ru->src_ip))==0 || ipMap.count(htonl(ru->dst_ip))==0) {
	  cerr << "Missing addresses... is this the right .model file?" << endl;
	  return false;
	}
	int c1 = ipMap[htonl(ru->src_ip)];
	int c2 = ipMap[htonl(ru->dst_ip)];
	reqRoutes[c1].insert(c2);
	ru++;
      }
    }
    fin.close();
    if (used) useful.push_back(logFiles[i]);
  }
  logFiles = useful;
  return true;
}

int buildFlows(RoutingTable& routes, IPtoVNMap& ipMap, vector<string>& logFiles, FlowMap& flows)
{
  flows.resize(numPipes+1);
  char buffer[BUF_SIZE];
  struct net_log_header *record = (struct net_log_header*)buffer;
  struct modelnet_route_stats_record* rstats = (struct modelnet_route_stats_record*)buffer;
  struct modelnet_packet_record *pkt = (struct modelnet_packet_record*)buffer;
  unsigned int i;
  for (i=0;i<logFiles.size();i++) {
    ifstream fin(logFiles[i].c_str());
    if (fin.fail())
      return false;
    LogReader reader(fin);
    while (reader.nextEntry(buffer,BUF_SIZE)) {
      if ((int)record->type == (int)MN_PACKET_RECORD) {
	if (!pkt->drop)
	  continue;
	int c1=ipMap[pkt->from_ip];
	int c2=ipMap[pkt->to_ip];
	vector<int>& route=routes[c1][c2];
	if (route.size() == 0) {
	  cerr << "No route for "<<c1<<"-->"<<c2<<endl;
	  return false;
	}
	int ehop = pkt->hop;
	unsigned int hop=0;
	int jump = ehop + 1;
	for (hop=jump; hop < route.size()-1; hop++) {
	  flows[route[hop]][route[hop+1]]--;
	}
      }
      else if ((int)record->type == (int)MN_ROUTE_STATS_RECORD) {
	struct mn_route_usage* ru=rstats->stats;
	int count = rstats->count;
	while (count--) {
	  int c1=ipMap[htonl(ru->src_ip)];
	  int c2=ipMap[htonl(ru->dst_ip)];
	  vector<int>& forwardRoute=routes[c1][c2];
	  if (forwardRoute.size() == 0) {
	    cerr << "No route for "<<c1<<"-->"<<c2<<endl;
	    return false;
	  }
	  unsigned int hop=0;
	  for (hop=0;hop<forwardRoute.size()-1;hop++)
	    flows[forwardRoute[hop]][forwardRoute[hop+1]]+=ru->packet_count;
	  ru++;
	}
      }
    }
    fin.close();
  }
  return true;
}

int main(int argc , char * argv[])
{
  VNtoIPMap vnToIP;
  IPtoVNMap ipToVN;
  CommMap reqRoutes;
  RoutingTable routes;
  FlowMap flows;

  parseArgs(argc, argv);
  if (!readClients(vnToIP, ipToVN)) {
    cerr<<"Error -- could not parse model file: "<<modelFile<<endl;
    exit(1);
  }
  if (!readLogs(logFilenames, ipToVN, reqRoutes))  {
    cerr<<"Error -- could not read log file"<<endl;
    exit(1);    
  }
  if (!readRoutes(reqRoutes, routes)) {
    cerr<<"Error -- could not parse routes file: "<<routeFile<<endl;
    exit(1);
  }
  if (!buildFlows(routes, ipToVN, logFilenames, flows))
  {
    cerr<<"Error -- could not build routes"<<endl;
    exit(1);
  }
  unsigned int pipe_nr;
  for (pipe_nr = 0; pipe_nr < flows.size(); pipe_nr++) {
    if (flows[pipe_nr].size() == 0)
      continue;
    cout << pipe_nr << " " << flows[pipe_nr].size();
    map<int, long long>::iterator it;
    for (it = flows[pipe_nr].begin(); it != flows[pipe_nr].end(); it++)
      cout<<" "<<it->first<<" "<<it->second;
    cout << endl;
  }
  return 0;
}
