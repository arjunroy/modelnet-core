/* Ethan Eade, 8/30/02
 * This tool takes an xml graph with bidrectional edges and
 * mirrors each edge, copying attribute info over.
 * This should be run just before assign.
 */

#include "mngraph.h"
#include <fstream>
#include <vector>
using namespace std;

void usage()
{
  cerr << "mirror inputfile.xml [outputfile.xml]" << endl;
  exit(1);
}

int main (int argc, char* argv[])
{
  MNGraph g;
  if (argc < 2)
    usage();
  if (!readGraphXML(argv[1],g)) {
    cerr << "Could not parse input file." << endl;
    exit(1);
  }
  edge e;
  vector<edge> allEdges;
  forall_edges(e,g)
    allEdges.push_back(e);
  forall(e,allEdges) {
    edge ne=g.addEdge(g.target(e),g.source(e));
    g[ne]=g[e];
  }
  ofstream fout;
  if (argc == 3)
    fout.open(argv[2]);
  else
    fout.open(argv[1]);
  writeGraphXML(fout, g);
  fout.close();
}
