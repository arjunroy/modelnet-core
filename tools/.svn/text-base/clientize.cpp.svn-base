/********
 clientize takes one input graph and sets the "type" field of nodes with out degree 1 to "client".
********/

#include <LEDA/set.h>
#include <LEDA/d_array.h>
#include <LEDA/array.h>
#include <LEDA/list.h>
#include <LEDA/stream.h>
#include <LEDA/graph.h>
#include <LEDA/gml_graph.h>
#include <string.h>
#include <cmath>
#include <fstream>
#include <algorithm>
#include "gmlp.h"

using namespace std;

int main(int argc, char* argv[])
{
    mn_graph g;
    raitner_parser gml(g);
    if (argc != 2)
	return 1;
    if (!gml.parse(argv[1]))
    {
	cerr << "Could not parse input file." << endl;
	exit(1);
    }
    node v;
    forall_nodes(v,g)
    {
	if (g.outdeg(v) == 1)
	    g[v].s("type") = "client";
    }
    ofstream out(argv[1]);
    out.setf(ios::showpoint);
    g.write_gml(out,write_node_attribute, write_edge_attribute);

    return 0;
}
