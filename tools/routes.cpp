#include <iostream>
#include <fstream>
#include <string>
using namespace std;
#include "mngraph.h"

void usage(char* prog)
{
  cerr << "usage: " << prog << " <graph>" << endl;
  exit(1);
}

int main(int argc, char* argv[])
{
  if (argc != 2)
    usage(argv[0]);
  MNGraph g;
  if (!readGraphXML(argv[1], g)) {
    cerr << "Error reading graph from " << argv[1] << endl;
    exit(1);
  }
  
  cout << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
  cout << "<allpairs>\n";
  vertex src, dst;
  edge hop;
  forall_vertices(src, g) {
    if (g[src].getString("role") != "virtnode")
      continue;
    vertex_array<edge> pred(g);
    edge_array<int> cost(g,1);
    shortest_paths(g, cost, pred, src);
    forall_vertices(dst, g) {
      if (g[dst].getString("role") != "virtnode" || pred[dst].isNil())
	continue;
      hop = pred[dst];
      cout << "  <path vnsrc=\""<< src.index << "\" vndst=\"" << dst.index << "\" hops=\"";
      vector<int> hops;
      while (!hop.isNil()) {
	hops.push_back(hop.index);
	hop = pred[g.source(hop)];
      }
      for (unsigned int i=hops.size(); i>0;i--)
	cout << hops[i-1] << " ";
      cout << "\"/>\n";
    }
  }
  cout << "</allpairs>\n";
  return 0;
}
