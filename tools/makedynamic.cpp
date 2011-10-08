/************
 makedynamic creates a family of scripts for use in running experiments with dynamic behavior.  The scripts use the "traffic" tool to generate flows.
************/

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
    cerr << "usage: makedynamic <.model> <commMap> <duration> <port>" << endl;
    exit(1);
}

struct Stage
{
    vector<int> targets;
    double when;
};

struct Client {
    int id;
    string ip;
    vector<Stage> stages;
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
    if (argc !=5)
	usage();
    string modelFile=argv[1];
    string commMapFilename = argv[2];
    int duration= atoi(argv[3]);
    int port = atoi(argv[4]);

    map<string, vector<int> > hosts;
    map<int, string> ips;
    map<int, vector<Stage> > stages;

    HostReader reader(ips, hosts);
    streamXML(modelFile, reader);

    ifstream commMapFile(commMapFilename.c_str());
    if (commMapFile.fail()) {
      cerr << "error: open comm map " << commMapFilename << endl;
      exit(1);
    }

    Stage stage;
    int sender;
    while (commMapFile >> sender) {
      int n,m;
      commMapFile >> m;
      while (m--) {
	commMapFile >> stage.when >> n;
	stage.targets.resize(n);
	while (n>0)
	  commMapFile >> stage.targets[--n];
	stages[sender].push_back(stage);
      }
    }
    commMapFile.close();
    
    string mnhome = getenv("MN_HOME");
    char buf[501];
    getcwd(buf,500);
    string dir = buf;
    if (dir[dir.length()-1] != '/')
	dir += '/';
    
    map<string, vector<int> >::iterator it;
    for (it=hosts.begin();it!=hosts.end();it++) {
      vector<int>& clients = it->second;
      ofstream out(("clients_"+it->first+".sh").c_str());
      out << "#!/bin/sh" << endl;
      out << "echo `hostname` running `uname -sr`..." << endl;
      unsigned int i;
      for (i=0;i<clients.size();i++) {
	out << "/usr/bin/env LD_PRELOAD="<<mnhome<<"/lib/libipaddr.so SRCIP=" << ips[clients[i]] << " "
	    <<mnhome<<"/tools/server -p "<<port<<" &" << endl;
      }
      out << "sleep 10" << endl;
      for (i=0;i<clients.size();i++) {
	if (stages[clients[i]].size() == 0)
	  continue;
	out << "/usr/bin/env LD_PRELOAD="<<mnhome<<"/lib/libipaddr.so SRCIP=" << ips[clients[i]] << " "
	    <<mnhome<<"/tools/traffic ";

	ostringstream flowsFilename;
	flowsFilename << "client"<<clients[i] << ".flows";
	out << dir << flowsFilename.str() << " &" << endl;
	ofstream flowsFile(flowsFilename.str().c_str());
	for (unsigned int stage = 0; stage < stages[clients[i]].size(); stage++) {
	  Stage& currStage = stages[clients[i]][stage];
	  for (unsigned int target = 0; target < currStage.targets.size(); target++) {
	    flowsFile << (stage+1)*100+target << " " << ips[currStage.targets[target]] << " " << port << " ";
	    flowsFile << (int)(currStage.when * duration) << " ";
	    if (stage < stages[clients[i]].size()-1)
	      flowsFile << (int)((stages[clients[i]][stage+1].when - currStage.when)*duration)<<" " ;
	    else
	      flowsFile << (int)((1-currStage.when)*duration) << " ";
	    flowsFile << " -1 -1" << endl;
	  }
	}
	flowsFile.close();
      }
      out << "sleep 2" << endl;
      out << "echo `hostname` has `ps -u root | grep -c traffic`" << endl;
      out << "sleep "<<(int)(duration*1.2) << endl;
      out << "killall traffic; killall server" << endl;
      out.close();
    }
    
    
    ofstream runner("runclients.sh");
    for (it=hosts.begin();it!=hosts.end();it++)
      runner << "ssh " << it->first <<" sudo hostname" << endl;
    for (it=hosts.begin();it!=hosts.end();it++)
      runner << "ssh " << it->first << " sudo sh "<<dir<<"clients_"<<it->first<<".sh &" << endl;
    runner.close();
    return 0;
}
