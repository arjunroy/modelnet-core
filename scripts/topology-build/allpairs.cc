/*
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 */

#include <boost/config.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <string>
#include <boost/graph/adjacency_list.hpp>

#include <boost/graph/dijkstra_shortest_paths.hpp>


#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <xercesc/dom/DOMErrorHandler.hpp>


#define StrX(x) XMLString::transcode(x)

using namespace boost;
using namespace std;

#include "mngraph.h"

struct AllPairRoute {
    ostream& _out;
    AllPairRoute(ostream& out=cout):_out(out){}

    void operator()(MNGraph& g, const MNGraph::Vertex& v) const {
        std::vector<MNGraph::Vertex> p(num_vertices(g));
        std::vector<int> d(num_vertices(g));
        if (g[v].s("role")!="virtnode") return;

	static int i=0;
	++i;
	if (i%10==0) cerr << "routing virtual node " << i << "\r" << flush;

        dijkstra_shortest_paths(g, v,
			predecessor_map(&p[0]).distance_map(&d[0]));
	MNGraph::VertexIterator vi, vend;
	for (tie(vi, vend) = vertices(g); vi != vend; ++vi) {
	    if (d[*vi]==0) continue;
            if (g[*vi].s("role")!="virtnode") continue;

	    MNGraph::Vertex node = *vi;
            std::vector<MNGraph::Edge> hops(d[*vi]);
	    for (int i=d[*vi]; i; --i) {
		graph_traits<MNGraph>::out_edge_iterator ei, eend;
		for (tie(ei, eend) = out_edges(p[node],g); ei != eend; ++ei) {
			if (target(*ei,g) == node) {
				hops[i-1] = *ei;
				break;
			}
		}
		node = p[node];
	    }
	    std::cout << "  <path int_vndst=\"" << g[*vi].i("vn")
		    << "\" int_vnsrc=\"" << g[v].i("vn")
		    << "\" hops=\"";
	    for(int i=0; i< d[*vi]; ++i) {
		    std::cout << g[hops[i]].i("idx") << " ";
	    }
	    std::cout << "\" />\n";
	}
    }
};

int
main(const int argc, const char **argv)
{
    const char *graphfile = argv[1];

    if (argc != 2) {
	    cerr << "usage: " << argv[0] << " file.graph > file.route" << endl;
	    exit(-1);
    }
    cerr << "reading " << graphfile << endl;

    MNGraph *g;
    g = readgraph(graphfile);

    if (!g) exit(1);
    cerr << "read " << num_vertices(*g) << " vertices, "
	    << num_edges(*g) << " edges." << endl;

    cout << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    cout << "<allpairs>\n";
    AllPairRoute router(cout);
    g->for_each_vertex(router);
    cout << "</allpairs>\n";

}
