/********
 hiergen generates topologies with a tree structure containing the specified number of clients and the specified path lengths. See gentargets.
********/

#include <set>
#include <vector>
#include <iostream>
#include <deque>
#include <fstream>
#include "mngraph.h"
#include <string>
#include <cmath>
using namespace std;

void usage()
{
  cerr << "Usage: hiergen <#clients> <#trees> <height> " << endl
       << "       <clnt-stb-bw> <clnt-stb-delay> <stb-trn-bw> <stb-trn-delay> <trn-trn-bw> <trn-trn-delay>" << endl
       << "       <xml output>" << endl;
    exit(1);
}

const string CLIENT = "virtnode";
const string STUB = "stub";
const string TRANSIT = "transit";

int minHeight=1000000;
int maxHeight=0;
vertex genTree(MNGraph& g, int height, int leaves, int level, int subtree)
{
  if (leaves == 0)
    return vertex::nil;
  if (level < height) {
    if (leaves == 0)
      cerr << "Level < Height, but leaves == 0!!" << endl;
    vertex root = g.addVertex();
    int branching = (int) pow(leaves, 1.0/(height-level));
    int i;
    for (i=0;i<branching;i++) {
      int subleaves = leaves/branching;
      if (i<(leaves%branching))
	subleaves++;
      if (height-level == 1)
	g[root].setString("role",STUB);
      else
	g[root].setString("role", TRANSIT);
      vertex child = genTree(g,height, subleaves, level+1, subtree);
      if (!child.isNil())
	g.addEdge(root, child);
    }
    return root;
  }

  if (leaves > 1)
    cerr << "Got to bottom, but leaves="<<leaves<<"!!"<<endl;
  else if (leaves == 1) {
    vertex n = g.addVertex();
    g[n].setInteger("subtree",subtree);
    g[n].setString("role",CLIENT);
    if (level > maxHeight)
      maxHeight=level;
    if (level < minHeight)
      minHeight=level;
    return n;
  }
  return vertex::nil;
}

int main(int argc, char* argv[])
{
  if (argc != 11)
    usage();
  int clients=atoi(argv[1]);
  int trees = atoi(argv[2]);
  int height = atoi(argv[3]);
  double client_stub_bw = strtod(argv[4],0);
  int client_stub_delay = atoi(argv[5]);
  double stub_transit_bw = strtod(argv[6],0);
  int stub_transit_delay = atoi(argv[7]);
  double transit_transit_bw = strtod(argv[8],0);
  int transit_transit_delay = atoi(argv[9]);
  char* filename = argv[10];
  if (clients < trees) {
    cerr << "Not enough clients to make "<<trees << " trees." << endl;
    exit(1);
  }
  MNGraph g;
  vector<vertex> rootList;
  vertex root;
  int tree;
  for (tree=0;tree<trees;tree++)  {
    int subleaves = clients/trees;
    if (tree < (clients%trees))
      subleaves++;
    vertex subTree= genTree(g,height,subleaves, 1, tree);
    if (subTree.isNil()) {
      cerr << "Empty subtree: " << tree << endl;
      exit(1);
    }
    forall(root, rootList) { 
      g.addEdge(root, subTree);
    }
    if (height == 1)
      g[subTree].setString("role", STUB);
    else
      g[subTree].setString("role",TRANSIT);
    rootList.push_back(subTree);
  }
  int clientCount = 0;
  vertex v;
  edge e;
  forall_vertices(v,g) {
    if (g[v].getString("role") == "virtnode") {
      g[v].setInteger("vn", clientCount);
      clientCount++;
    }
  }
  forall_edges(e, g) {
    string srcRole = g[g.source(e)].getString("role");
    string targetRole = g[g.target(e)].getString("role");
    string specs;
    if (srcRole == CLIENT || targetRole == CLIENT)
      specs = "client-"+STUB;
    else if (srcRole == STUB || targetRole == STUB)
      specs = STUB+"-"+TRANSIT;
    else
      specs = TRANSIT+"-"+TRANSIT;
    g[e].setString("specs", specs);
  }
  g.specs("client-"+STUB).setDouble("kbps", client_stub_bw);
  g.specs("client-"+STUB).setInteger("delayms", client_stub_delay);
  g.specs(STUB+"-"+TRANSIT).setDouble("kbps", stub_transit_bw);
  g.specs(STUB+"-"+TRANSIT).setInteger("delayms", stub_transit_delay);
  g.specs(TRANSIT+"-"+TRANSIT).setDouble("kbps", transit_transit_bw);
  g.specs(TRANSIT+"-"+TRANSIT).setInteger("delayms", transit_transit_delay);
  cout << "Graph has "<<g.vertexCount()<< " nodes ("<<clientCount
       <<" clients) and "<<g.edgeCount() << " edges." << endl;
  cout << "Height ranges from "<<minHeight << " to "<< maxHeight<<"."<<endl;
  ofstream out(filename);
  if (out.fail()) {
    cerr << "cannot open output file: \""<<filename<<"\"" << endl;
    exit(1);
  }
  bool ok = writeGraphXML(out, g);
  if (!ok) {
    cerr << "error writing xml output" << endl;
    exit(1);
  }
  out.close();
  return 0;
}
