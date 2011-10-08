/***********
 flowgen takes a pattern description file (see assign) and generates a lst of flows fitting the pattern.
***********/

#include "assignment/flow.h"
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
using namespace std;

void usage()
{
  cerr << "usage: flowgen patternFile #clients outputfile" << endl;
  exit(1);
}

int main(int argc, char* argv[])
{
  if (argc <4)
    usage();
  ifstream in(argv[1]);
  if (!in)
  {
    cerr << "Invalid input file: "<< argv[1] <<endl;
    exit(1);
  }
  user_flow_pattern pattern;
  if (!pattern.read_pattern(in))
  {
    cerr << "Could not read pattern." <<endl;
    exit(1);
  }
  in.close();
  
  int numClients = atoi(argv[2]);
  if (numClients < 0)
  {
    cerr << "Invalid number of clients: " << argv[2] <<endl;
    exit(1);
  }
  MNGraph g;
  list<vertex> clients;
  list<flow*> flows;
  while (g.vertexCount() < numClients)
    clients.push_back(g.addVertex());
  
  pattern.constructFlows(g, clients, flows);
  vertex_array<int> nodeIndex(g);
  vertex v;
  int index=0; 
  forall_vertices(v,g)
    nodeIndex[v]=index++;
  ofstream out(argv[3]);
  if (!out) {
    cerr << "Invalid output file: "<< argv[3] <<endl;
    exit(1);
  }

  vertex_array<vector<flow*> > flowsFor(g);
  flow* f;
  forall(f, flows)
    flowsFor[f->source].push_back(f);
  forall_vertices(v,g) {
    out << nodeIndex[v] << " " << flowsFor[v].size();
    forall (f, flowsFor[v])
      out << " " <<nodeIndex[f->target];
    out << endl;
  }
  out.close();
  return 0;
}
