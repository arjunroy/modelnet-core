/* $Id: gmlp.h 143 2003-05-22 17:01:29Z eade $ */

/*
 * Graph Markup Lanaguage parser utility routines.
 *
 * This file compliments the gml parsers from LEDA and Marcus Raitner.
 *
 * It provides a class Attributes which is a container for the
 * object attributes that can be attached to nodes and edges of a graph.
 *
 * The LEDA parser can only do single-level deep strings, ints, and doubles,
 * and must be told explicitly what keywords to look for BEFORE parsing.
 *
 * The Raitner parser is much simpler, and can handle multi-level structures,
 * and will return the list of keywords used in the file.
 *
 * Also provided are the hooks needed by LEDA to write a gml file with
 * these attributes.
 *
 * See the examples below.
 */


#ifndef __GMLP_H_
#define __GMLP_H_

#include <stdio.h>
#include <LEDA/string.h>
#include <LEDA/dictionary.h>
#include <LEDA/graph.h>
#include <LEDA/gml_graph.h>

#include "gml_parser.h" /* raitner parser */

/*
 * An Attributes object is a list of key-value pairs.
 * Each value can be an int, double, string, or Attributes list (a child).
 * We have accessors for i,d,s,c types in both const and non-const,
 * as well as a has_{i,d,s,c} call to see if an attribute is set.
 *
 * Some common and standard attribute pairs:
 *   id : int		- used internally by the gml file format
 *   source : int	- used internally by the gml file format
 *   target : int	- used internally by the gml file format
 *   parameter : string - (un)used internally by leda
 *   label : string	- visible name of node or edge
 *   type : string	- {as|transit|stub|client} or <aaa>-<bbb> for links
 *   bw : int		- bandwidth in kbit/s (perhaps should be bits/s?)
 *   lat : int		- latency in ms (perhaps should be microsec?)
 *   plr : double	- loss rate in range [0..1]
 *   hops : int		- number of hops represented by a condensed link
 *
 */
class Attributes {
    public:

	/* constructors and destructors */
	Attributes() { }
	~Attributes() { }
	Attributes(const Attributes &other)
	: the_ints(other.the_ints),
	  the_doubles(other.the_doubles),
	  the_strings(other.the_strings),
	  the_children(other.the_children) { }

	/* int accessors */
	const int i(const char *name) const {
	    dic_item it = the_ints.lookup(name);
	    if (it == nil) return 0;
	    return the_ints[it];
	}
	int &i(const char *name) {
	    dic_item it = the_ints.lookup(name);
	    if (it == nil) it = the_ints.insert(name, 0);
	    return the_ints[it];
	}
	const bool has_i(const char *name) const {
	    return (the_ints.lookup(name) != nil);
	}

	/* double accessors */
	const double d(const char *name) const {
	    dic_item it = the_doubles.lookup(name);
	    if (it == nil) return 0.0;
	    return the_doubles[it];
	}
	double &d(const char *name) {
	    dic_item it = the_doubles.lookup(name);
	    if (it == nil) it = the_doubles.insert(name, 0.0);
	    return the_doubles[it];
	}
	const bool has_d(const char *name) const {
	    return (the_doubles.lookup(name) != nil);
	}

	/* string accessors */
	const string s(const char *name) const {
	    dic_item it = the_strings.lookup(name);
	    if (it == nil) return string("");
	    return the_strings[it];
	}
	string &s(const char *name) {
	    dic_item it = the_strings.lookup(name);
	    if (it == nil) it = the_strings.insert(name, string(""));
	    return the_strings[it];
	}
	const bool has_s(const char *name) const {
	    return (the_strings.lookup(name) != nil);
	}

	/* child accessors */
	const Attributes c(const char *name) const {
	    dic_item it = the_children.lookup(name);
	    if (it == nil) return Attributes();
	    return the_children[it];
	}
	Attributes &c(const char *name) {
	    dic_item it = the_children.lookup(name);
	    if (it == nil) it = the_children.insert(name, Attributes());
	    return the_children[it];
	}
	const bool has_c(const char *name) const {
	    return (the_children.lookup(name) != nil);
	}

	/* helpers */
	bool add(const gml_object *gml); /* reads in gml object list */
	void write(ostream &out) const;  /* writes out gml format */


    /* private: */
	dictionary<string,int> the_ints;
	dictionary<string,double> the_doubles;
	dictionary<string,string> the_strings;
	dictionary<string,Attributes> the_children;
};
ostream &operator<<(ostream &out, const Attributes &atts); /* not used */
istream &operator>>(istream &in, Attributes &atts);	   /* not used */

/*
 * We mostly use graphs with attributes on both edges and nodes,
 * like so:
 */
typedef GRAPH<Attributes,Attributes> mn_graph;


/*
 * LEDA parser helpers.
 * The LEDA parser can not handle sub-lists in the node and
 * edge attribute lists (it will silently skip them). It also
 * needs to be told before hand what keywords to look for. This
 * is done by adding node and edge rules, like so:
 * 
 *    GRAPH<Attributes,Attributes> g;
 *    gml_graph gml(g);
 *
 *    gml.add_edge_rule(parse_edge_attribute, gml_double, "weight");
 *    gml.add_edge_rule(parse_edge_attribute, gml_int, "latency");
 *    gml.add_node_rule(parse_node_attribute, gml_string, "name");
 *
 *    gml.parse("my_file.gml") || die();
 *
 *    node v = g->...
 *    Attributes a = g->inf(v);
 *    a.d("weight") += a.i("latency") * 2.5;
 *    a.i("latency") = 300;
 *    ...
 *
 */
bool parse_node_attribute(const gml_object *gml, graph *g, node v);
bool parse_edge_attribute(const gml_object *gml, graph *g, edge e);

/*
 * The following two functions help LEDA to write a GML file
 * with Attributes for both edges and nodes. For example:
 *
 *   GRAPH<Attributes,Attributes> g;
 *   ...
 *   g.write_gml("out_file.gml",
 *                write_node_attributes, write_edge_attributes) || die();
 *
 */
void write_node_attribute(ostream &out, const graph *g, const node v);
void write_edge_attribute(ostream &out , const graph *g, const edge e);


/*
 * Raitner parser wrapper class.
 * The Raitner parser is very simple and should be much faster than
 * the LEDA parser. It can get a read file quite simply, like so:
 *
 *   GRAPH<Attributes,Attributes> g;
 *   raitner_parser gml(g);
 *
 *   gml.parse("my_file.gml") || die();
 *   ...
 */
class raitner_parser {
    public:
	raitner_parser(mn_graph &g) : the_graph(&g) { stat.key_list = NULL; }
	~raitner_parser() { }

	bool parse(const char *filename);
	bool parse(FILE * file, const char * filename);
	
    /* private: */
	mn_graph *the_graph;
	struct GML_stat stat;

};


/*
 * Some more helpers.
 */

/*
 * Derive a node type (e.g. client,stub,transit) or edge type
 * from the labels, outdegree, or other information.
 */
const char *derive_type(const node v);
const char *derive_type(const edge e);

#endif

