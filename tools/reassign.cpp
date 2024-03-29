/***********
 Reassign takes a map of pipe reassignments such as that generated by "assigndiff" and calls the reassignment daemon on each core machine, changing pipe ownerships appropriately.  See assigndiff and reassigner, and dynamic.
***********/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <errno.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <strstream>
#include <reassign.h>
#include "xml.h"

bool verbose = true;
void usage()
{
    cerr << "usage: reassign <.hosts file> <changefile> <port>" << endl;
    exit(1);
}

unsigned short port;
set<int> cores;
map<int, string> coreToHost;
map<int, unsigned long> coreToIP;
map<int, int> connectionForCore;
typedef vector<ReassignEntry> Stage;
map<int, vector<Stage> > stagesFor;

class Change
{
public:
    int nr;
    int c1, c2;
};

vector<Change> changes;

int getCores(string hostsFile) {
  XMLElement hosts;
  if (!readXML(hostsFile, hosts)) {
    cerr << "Error reading " << hostsFile << endl;
    return false;
  }
  vector<const XMLElement*> emuls = hosts.findChildrenByName("emul");
  for (unsigned int i=0; i<emuls.size(); i++) {
    coreToHost[i]=emuls[i]->getAttribute("hostname");
    if (verbose) cerr << coreToHost[i] << endl;
  }
  return true;
}

int getAddrs()
{
  map<int, string>::iterator it;
  for (it=coreToHost.begin();it!=coreToHost.end();it++) {
    struct hostent* host=gethostbyname(it->second.c_str());
    if (host == 0) {
      cerr << "Failed to resolve host: " << it->second << endl;
      return false;
    }
    if (verbose) cerr << it->second << " --> " << inet_ntoa(*(struct in_addr*)host->h_addr_list[0]) << endl;
    coreToIP[it->first]=*(unsigned long*)host->h_addr_list[0];
  }
  return true;
}

int initConnections()
{
  map<int, unsigned long>::iterator it;
  for (it=coreToIP.begin();it!=coreToIP.end();it++) {
    int s=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1) {
      cerr<<"Could not open socket to "<<coreToHost[it->first] << endl;
      return false;
    }
    struct sockaddr_in saddr;
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(port);
    saddr.sin_addr.s_addr=it->second;
    if (connect(s, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
      cerr<<"Could not connect to "<<coreToHost[it->first]<<endl;
      return false;
    }
    connectionForCore[it->first]=s;
  }
  return true;
}

template <class T>
int sendThing(T& data, int s)
{
    int len = send(s, (char*) &data, sizeof(data), 0);
    if (len != sizeof(data))
	return false;
    return true;
}

int sendCmd(ReassignCmd cmd, int s)
{
    cmd.cmd = htonl(cmd.cmd);
    cmd.arg = htonl(cmd.arg);
    return sendThing(cmd,s);
}

template <class T>
int receiveThing(T& data, int s)
{
    int len = recv(s, (char*) &data, sizeof(data), 0);
    if (len != sizeof(data))
	return false;
    return true;
}

int receiveCmd(ReassignCmd& cmd, int s)
{
    if (!receiveThing(cmd,s))
	return false;
    cmd.cmd = ntohl(cmd.cmd);
    cmd.arg = ntohl(cmd.arg);
    return true;
}

int waitForData(vector<int> sockets, int& sel)
{
    cout << "Waiting for data on " << sockets.size() << " sockets."<<endl;
    unsigned int i;
    int maxfd = 0;
    fd_set readSet;
    FD_ZERO(&readSet);
    for (i=0;i<sockets.size();i++)
    {
	FD_SET(sockets[i], &readSet);
	if (sockets[i] > maxfd)
	    maxfd = sockets[i];
    }
    struct timeval tv;
    sel = 0;
    while (sel == 0)
    {
 	cout << "Waiting on sockets..." << endl;
	tv.tv_sec=1;
	tv.tv_usec=0;
	sel = select(maxfd+1, &readSet, 0, 0, &tv);
    }
    if (sel == -1)
    {
	cout << "Select error: " << errno << endl;
	return false;
    }
    for (i=0; i<sockets.size(); i++)
    {
	if (FD_ISSET(sockets[i], &readSet))
	{
	    sel = sockets[i];
	    return true;
	}
    }
    return false;
}

template <class T>
int receiveThing(T& data, vector<int> sockets)
{
    int s;
    if (!waitForData(sockets, s))
	return false;
    return receiveThing(data,s);
}

int receiveCmd(ReassignCmd& cmd, vector<int> sockets)
{
    if (!receiveThing(cmd,sockets))
	return false;
    cmd.cmd = ntohl(cmd.cmd);
    cmd.arg = ntohl(cmd.arg);
    return true;
}

int finishConnections()
{
    map<int,int>::iterator it;
    ReassignCmd finish;
    finish.cmd=FINISHED;
    finish.arg=0;
    for (it=connectionForCore.begin();it!=connectionForCore.end();it++)
    {
      if (!sendCmd(finish, it->second))
	return false;
      shutdown(it->second,1);
      close(it->second);
    }
    return true;
}

int readChanges(istream& in)
{
    changes.clear();
    Change change;
    while (in >> change.nr >> change.c1 >> change.c2)
    {
	changes.push_back(change);
	cores.insert(change.c1);
	cores.insert(change.c2);
    }
    return true;
}

int makeStages()
{
  unsigned int i;
  set<int>::iterator it;
  for (it=cores.begin();it!=cores.end();it++)  {
    stagesFor[*it].resize(3);
  }
  for (i=0;i<changes.size();i++) {
    ReassignEntry entry;
    entry.pipe_nr =htonl(changes[i].nr);
    entry.owner=htonl(coreToIP[changes[i].c2]);
    for (it=cores.begin();it!=cores.end();it++) {
      if (*it == changes[i].c1)
	stagesFor[*it][1].push_back(entry);
      else if (*it == changes[i].c2) {
	stagesFor[*it][0].push_back(entry);
	stagesFor[*it][0].back().owner = 0;
      }
      else
	stagesFor[*it][2].push_back(entry);
    }
  }
  return true;
}

int sendStages()
{
  unsigned int i, stage;
  map<int,string>::iterator it;
  for (it=coreToHost.begin();it!=coreToHost.end();it++) {
    if (verbose) cout << "Sending stages to " << it->second << endl;
    int s = connectionForCore[it->first];
    unsigned int stages = (unsigned int)stagesFor[it->first].size();
    ReassignHeader header;
    header.stages = htonl(stages);
    sendThing(header,s);
    for (stage=0; stage<stages;stage++) {
      ReassignStage stg;
      stg.count = htonl(stagesFor[it->first][stage].size());
      sendThing(stg,s);
      vector<ReassignEntry>& entries=stagesFor[it->first][stage];
      for (i=0;i<entries.size();i++)
	sendThing(entries[i],s);
    }
    ReassignCmd response;
    if (!receiveCmd(response,s) || response.cmd != OK)
      {
	cerr << "Bad response from " << it->first <<": "<<response.cmd << endl;
	return false;
      }
  }
  return true;
}

int doStage(int stage)
{
  map<int,string>::iterator it;
  ReassignCmd start, response;
  start.cmd=START_STAGE;
  start.arg = stage;
  vector<int> sockets;
  for (it=coreToHost.begin();it!=coreToHost.end();it++) {
    int s = connectionForCore[it->first];
    sockets.push_back(s);
    sendCmd(start,s);
  }
  for (it=coreToHost.begin();it!=coreToHost.end();it++) {
    int s = connectionForCore[it->first];
    if (!receiveCmd(response,s) || response.cmd != OK)
      return false;
  }
  return true;
}

void errcheck(int val)
{
  if (!val) {
    cerr << "An error has occurred." << endl;
    exit(1);
  }
}

int main(int argc, char* argv[])
{
  if (argc !=4)
    usage();
  string hostsFile=argv[1];
  ifstream diff(argv[2]);
  port = (unsigned short)atoi(argv[3]);
  if (diff.fail()) {
    cerr << "error: open diff file" << endl;
    usage();
  }
  if (verbose) cout << "Reading diff..." << endl;
  assert(readChanges(diff));
  if (verbose) cout << "Reading core info..." << endl;
  assert(getCores(hostsFile));
  diff.close();
  if (verbose) cout << "Getting addresses..."<<endl;
  errcheck(getAddrs());
  if (verbose) cout << "Constructing stage data..."<<endl;
  errcheck(makeStages());
  if (verbose) cout << "Opening connections..." << endl;
  errcheck(initConnections());
  
  if (verbose) cout << "Transferring stage data..." << endl;
  errcheck(sendStages());
  int i;
  for (i=0;i<3;i++) {
    if (verbose) cout << "Doing stage " << i << "..."<<endl;
    errcheck(doStage(i));
  }
  if (verbose) cout << "Closing connections..." <<endl;
  errcheck(finishConnections());
  if (verbose) cout<<"Reassign complete."<<endl;
}
