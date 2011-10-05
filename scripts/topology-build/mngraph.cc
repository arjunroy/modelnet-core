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

using namespace boost;
using namespace std;

#include "mngraph.h"

//This functor prints a (key,value) pair as key="value"
template <class T>
struct PrintPair{
	string _prefix;
	ostream& _out;
	PrintPair(ostream& out=cout, string prefix=""):_prefix(prefix), _out(out){}
	void operator()(const string& key, const T& value) const {
		_out << " " << _prefix << key << "=\"" << value << "\"";
	}
};

// Print all the attributes in an AttributeMap to out, with type prefixes
void printAttributes(ostream& out, AttributeMap& attr)
{
	attr.forEachString(PrintPair<string>(out,""));
	attr.forEachInt(PrintPair<int>(out,"int_"));
	attr.forEachDouble(PrintPair<double>(out,"dbl_"));
}

/*****************************************
 * GraphXMLPrinter works for iteration over vertices and edges, and prints
 * each element as XML.
 *****************************************/

struct GraphXMLPrinter {
	int _indent;
	ostream& _out;
	GraphXMLPrinter(ostream& out=cout, int indent=0):_indent(indent),_out(out){}
	
	// For iteration over vertices
	void operator()(MNGraph& g, const MNGraph::Vertex& v) const {
		_out << string(_indent,' ') << "<vertex";
		printAttributes(_out, g[v]);
		_out << "/> " << endl;
	}

	//For iteration over edges
	void operator()(MNGraph& g, const MNGraph::Edge& e) const {
		_out << string(_indent,' ') << "<edge";
		printAttributes(_out, g[e]);
		_out << "/> " << endl;
	}
};


// Print g to out as XML
void write_to_xml(MNGraph& g, ostream& out)
{
	out << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" << endl;
	out << "<topology";
	printAttributes(out, g);
	out << ">" << endl;
	GraphXMLPrinter printer(out,8);
	out << "    <vertices>" << endl;
	g.for_each_vertex(printer);
	out << "    </vertices>" << endl;
	out << "    <edges>" << endl;
	g.for_each_edge(printer);
	out << "    </edges>" << endl;
	out << "</topology>" << endl;
}

#if 0
int main(int, char*[])
{
	std::size_t N = 10;
	MNGraph g(N);
	unsigned int i;

	// Create random edges
	for (i=0; i<N*N/8; ++i) {
		std::size_t a = rand()%N, b = rand()%N;
		while ( a == b ) b = rand()%N;
		
		// Get the descriptor for a new edge.
		MNGraph::Edge e = add_edge(a, b, g).first;
		
		// Set an attribute of the edge
		g[e].d("bw") = rand()*3.14;
	}
	
	// Set graph attributes
	g.s("name") = "test graph";
	g.i("num_vertices") = num_vertices(g);
	g.i("num_edges") = num_edges(g);
	
	// Output
	write_to_xml(g);
	return 0;
}

#endif
