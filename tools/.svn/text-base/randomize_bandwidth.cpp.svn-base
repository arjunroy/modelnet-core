#include "gmlp.h"
#include <fstream>

void usage()
{
  cerr << "mirror inputfile.gml [outputfile.gml]" << endl;
  exit(1);
}

int main (int argc, char* argv[])
{
  mn_graph g;
  raitner_parser gml(g);
  if (argc < 2)
    usage();
  if (!gml.parse(argv[1]))
  {
    cerr << "Could not parse input file." << endl;
    exit(1);
  }
  edge e;
  list<edge> allEdges=g.all_edges();
  int pipe_nr=1;
  forall(e,allEdges)
  {
    string name = g[e].s("type");

    if (name == "client-stub")
	{
	 g[e].i("bw")=(int)(1000+9000*drand48());
	}
else if (name == "stub-stub")
	{
	 g[e].i("bw")=(int)(10000+90000*drand48());
	}
else if (name == "stub-transit")
	{
	 g[e].i("bw")=(int)(10000+90000*drand48());
	}

 else if (name == "transit-transit")
	{
	 g[e].i("bw")=(int)(100000+45000*drand48());
	}
  }
  g.make_directed();
  ofstream fout;
  if (argc == 3)
    fout.open(argv[2]);
  else
    fout.open(argv[1]);
  g.write_gml(fout,write_node_attribute, write_edge_attribute);
  fout.close();
}
