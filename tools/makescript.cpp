/*************
 makescript generates the scripts necessary to run a modelnet experiment with static communication patterns.
*************/

#include <string>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <sstream>
#include <map>
#include <set>
#include <vector>
using namespace std;
#include "xml.h"

void usage()
{
    cerr << "usage: makescript <.model> <commMap> <duration> <size> <port>" << endl;
    exit(1);
}

struct Client {
    string name;
    int id;
    string ip;
};

class HostReader : public XMLStream {
public:
  HostReader(map<int, string>& ips, map<string, vector<int> >& hosts) : myIPs(ips), myHosts(hosts) {}
  void onElement(const string& name, map<string,string>& attrs) {
    if (name == "host") {
      myCurrHost = attrs["hostname"];
      return;
    }
    if (name == "virtnode") {
      int id = atoi(attrs["int_vn"].c_str());
      string ip = attrs["vip"];
      myIPs[id]=ip;
      myHosts[myCurrHost].push_back(id);
    }
  }
  void onError(const string& msg) {
    cerr << "Error reading .model file: " << msg << endl;
    exit(1);
  }
private:
  map<int,string>& myIPs;
  map<string, vector<int> >& myHosts;
  string myCurrHost;
};

int main(int argc, char* argv[])
{
    if (argc !=6)
	usage();
    string modelFile=argv[1];
    string commMapFilename = argv[2];
    int duration= atoi(argv[3]);
    int psize = atoi(argv[4]);
    int port = atoi(argv[5]);

    map<string, vector<int> > hosts;
    map<int, vector<int> > commMap;
    map<int, string> ips;

    HostReader reader(ips, hosts);
    streamXML(modelFile, reader);
    ifstream commMapFile(commMapFilename.c_str());
    if (commMapFile.fail()) {
      cerr << "error: open comm map: " << commMapFilename << endl;
      exit(1);
    }
    string lineStr;
    while (getline(commMapFile, lineStr)) {
      istringstream line(lineStr.c_str());
      int src,dst;
      line >> src;
      while (line >> dst)
	commMap[src].push_back(dst);
    }
    commMapFile.close();

    map<string, vector<int> >::iterator it;
    for (it=hosts.begin();it!=hosts.end();it++) {
      vector<int>& clients = it->second;
      ofstream out(("clients_"+it->first+".sh").c_str());
      out << "#!/bin/sh" << endl;
      out << "echo `hostname` running `uname -sr`..." << endl;
      string mnhome = getenv("MN_HOME");
      unsigned int i,t;
      for (i=0;i<clients.size();i++) {
	out << "/usr/bin/env LD_PRELOAD="<<mnhome<<"/lib/libipaddr.so SRCIP=" << ips[clients[i]] << " "
	    <<mnhome<<"/tools/server -p "<<port<<" &" << endl;
      }
      out << "sleep 10" << endl;
      for (i=0;i<clients.size();i++) {
	out << "/usr/bin/env LD_PRELOAD="<<mnhome<<"/lib/libipaddr.so SRCIP=" << ips[clients[i]] << " "
	    <<mnhome<<"/tools/blaster  -p "<<port<<" -l "<<duration<<" -s "<<psize << " ";
	vector<int>& targets = commMap[clients[i]];
	for (t=0;t<targets.size();t++)
	  out << ips[targets[t]] << " ";
	out << " &"<<endl;
      }
      out << "echo `hostname` has `ps -u root | grep -c blaster`" << endl;
      out << "sleep "<<(int)(duration*1.1) << endl;
      out << "killall blaster; killall server" << endl;
      out.close();
    }
    
    char buf[501];
    getcwd(buf,500);
    string dir = buf;
    if (dir[dir.length()-1] != '/')
	dir += '/';
    
    ofstream runner("runclients.sh");
    for (it=hosts.begin();it!=hosts.end();it++)
      runner << "ssh " << it->first <<" sudo hostname" <<  endl;
    for (it=hosts.begin();it!=hosts.end();it++)
      runner << "ssh " << it->first << " sudo sh "<<dir<<"clients_"<<it->first<<".sh &" << endl;
    runner.close();
    return 0;
}
