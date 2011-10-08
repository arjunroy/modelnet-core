/* $Id: gmlp.cpp 143 2003-05-22 17:01:29Z eade $ */

/*
 * Implementation of helpers in gmlp.h
 */

#include <LEDA/stream.h>
#include <string.h>

#include "gmlp.h"


/*
 * Implementation of Attributes class.
 * Some well-known attributes (see gmlp.h) are explicitly converted here.
 */

bool Attributes::add(const gml_object *gml)
{
    string name(gml->get_key());    /* get key name */

    switch(gml->get_type()) {
    case gml_int:
	if (name == "plr") d(name) = (double)gml->get_int();
	else i(name) = gml->get_int();
	break;
    case gml_double:
	if (name == "bw" || name == "lat") i(name) = (int)gml->get_double();
	else d(name) = gml->get_double();
	break;
    case gml_string:
	s(name) = string(gml->get_string());
	break;
    default:
	cerr << "gml object type not recognized:"
	     <<" key="<<name<<" type="<<gml->get_type()<<"\n";
	return false;
    }
    return true;
}

// used when? during non-gml writes?
ostream &operator<<(ostream &out, const Attributes &atts)
{ out << "<private data>"; return out; } 
/* { atts.write(out); return out; }  */

// used when? during non-gml reads?
istream &operator>>(istream &in, Attributes &atts)
{ cerr <<"why read?!?!\n"; return in; }

#define TAB "    "
void Attributes::write(ostream &out) const
{
    dic_item it;
    forall_items(it, the_ints)
	out <<TAB<<the_ints.key(it)<<" "<<the_ints.inf(it)<<"\n";
    forall_items(it, the_doubles)
	out <<TAB<<the_doubles.key(it)<<" "<<the_doubles.inf(it)<<"\n";
    forall_items(it, the_strings)
	out <<TAB<<the_strings.key(it)<<" \""<<the_strings.inf(it)<<"\"\n";
}
#undef TAB

void write_node_attribute(ostream &out, const graph *g, const node v)
{
    const Attributes &atts = (*(mn_graph *)g)[v];
    atts.write(out);
}

void write_edge_attribute(ostream &out , const graph *g, const edge e)
{
    const Attributes &atts = (*(mn_graph *)g)[e];
    atts.write(out);
}


/*
 * Implementation of LEDA parser helpers
 */

bool parse_node_attribute(const gml_object *gml, graph *g, node v)
{
    Attributes &atts = (*(mn_graph *)g)[v];   /* get attributes for node  */
    return atts.add(gml);
}

bool parse_edge_attribute(const gml_object *gml, graph *g, edge e)
{
    Attributes &atts = (*(mn_graph *)g)[e];   /* get attributes for edge */
    return atts.add(gml);
}


void add_attributes(Attributes &atts, struct GML_pair *lst,
	int *idp, int *srcp, int *dstp)
{
    for (struct GML_pair *a = lst; a; a = a->next) {
	switch(a->kind) {
	case GML_INT:
	    /* we never preserve internal atts: id, source, target */
	    if (!strcmp(a->key, "id")) {
		if (idp) *idp = a->value.integer;
		break;
	    }
	    if (!strcmp(a->key, "source")) {
		if (srcp) *srcp = a->value.integer;
		break;
	    }
	    if (!strcmp(a->key, "target")) {
		if (dstp) *dstp = a->value.integer;
		break;
	    }
	    if (!strcmp(a->key, "plr")) {
		atts.d(a->key) = (double)a->value.integer;
		break;
	    }
	    atts.i(a->key) = a->value.integer;
	    break;
	case GML_DOUBLE:
	    if (!strcmp(a->key, "bw")) {
		atts.i(a->key) = (int)a->value.floating;
		break;
	    }
	    if (!strcmp(a->key, "lat")) {
		atts.i(a->key) = (int)a->value.floating;
		break;
	    }
	    atts.d(a->key) = a->value.floating;
	    break;
	case GML_STRING:
	    /* we never preserve internal atts: parameter (used by leda) */
	    if (!strcmp(a->key, "parameter")) {
		break;
	    }
	    atts.s(a->key) = a->value.string;
	    break;
	case GML_LIST:
	    add_attributes(atts.c(a->key), a->value.list, NULL, NULL, NULL);
	    break;
	default:
	    break;
	}
    }
}

/*
 * Implementation of Raitner parser wrapper class.
 */
bool raitner_parser::parse(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
	cerr << "No such file: " << filename << "\n";
	return false;
    }
    parse(file, filename);
    return true;
}

/*
 * Implementation of Raitner parser wrapper class.
 */
bool raitner_parser::parse(FILE * file, const char * filename)
{
    GML_init(); /* clear status */
    struct GML_pair *list = GML_parser(file, &stat, 0);

    if (stat.err.err_num != GML_OK) {
	cerr << "Parse error in " << filename
	     <<" (line "<<stat.err.line << ", column "<<stat.err.column<<")\n";
	switch(stat.err.err_num) {
	case GML_UNEXPECTED:
	    cerr << "  >> unexpected character\n";
	    break;
	case GML_SYNTAX:
	    cerr << "  >> bad syntax\n";
	    break;
	case GML_PREMATURE_EOF:
	    cerr << "  >> premature end-of-file\n";
	    break;
	case GML_TOO_MANY_DIGITS:
	    cerr << "  >> number has too many digits\n";
	    break;
	case GML_OPEN_BRACKET:
	    cerr << "  >> open brackets left and end-of-file\n";
	    break;
	case GML_TOO_MANY_BRACKETS:
	    cerr << "  >> too many closing brackets\n";
	    break;
	default:
	    cerr << "  >> unknown error\n";
	    break;
	}
	GML_free_list(list, stat.key_list);
	return false;
    }

    while (list != NULL &&
	(list->kind != GML_LIST || strcmp(list->key, "graph"))) {
	list = list->next;
    }
	
    if (list == NULL || list->kind != GML_LIST || strcmp(list->key, "graph")) {
	cerr << "File does not contain a graph: " << filename << "\n";
	GML_free_list(list, stat.key_list);
	return false;
    }

    /* convert to LEDA graph */
    the_graph->clear();
    the_graph->make_directed();
    dictionary<int, node> nodes;

    int directed = 0; /* undirected by default */

    /* first add the nodes, maintaining a mapping from id to nodeptr */
    bool warned = false;
	struct GML_pair *l;

    for (l = list->value.list; l; l = l->next) {
	if (l->kind == GML_LIST && !strcmp(l->key, "node")) {
	    node v = the_graph->new_node(Attributes());
	    Attributes &atts = (*the_graph)[v];
	    int id = -1;
	    add_attributes(atts, l->value.list, &id, NULL, NULL);
	    if (id >= 0) nodes.insert(id, v);
	    else if (!warned) {
		cerr << "Warning: found node(s) with no id\n";
		warned = true;
	    }
	} else if (l->kind == GML_INT && !strcmp(l->key, "directed")) {
	    directed = l->value.integer;
	}
    }

    warned = false;
    for (l = list->value.list; l; l = l->next) {
	if (l->kind == GML_LIST && !strcmp(l->key, "edge")) {
	    Attributes atts;
	    int source = -1;
	    int target = -1;
	    add_attributes(atts, l->value.list, NULL, &source, &target);
	    dic_item src = (source >= 0 ? nodes.lookup(source) : nil);
	    dic_item dst = (target >= 0 ? nodes.lookup(target) : nil);
	    if (src == nil || dst == nil) {
		if (!warned) {
		    cerr << "Warning: ignoring edges with invalid endpoints\n";
		    cerr << "  source = "<<source<<", target = "<<target<<"\n";
		    warned = true;
		}
	    } else {
		the_graph->new_edge(nodes[src], nodes[dst], atts);
	    }
	}
    }

    /* we have to do this last (since we dont filter self-loops) */
    if (!directed) the_graph->make_undirected();

    return true;
}


/*
 * Extra helper functions.
 */

/*
 * Derive a type (e.g. client,stub,transit,as) for an untyped node.
 * The as type could be eliminated by looking at the outdegree
 * and classifying as either transit or stub.
 */
const char *derive_type(const node v)
{
    const mn_graph *g = (mn_graph *)graph_of(v);
    const string name = (*g)[v].s("label");

    if (name.length() > 0) {
	if (name[0] == 's' || name[0] == 'S') return "stub";
	else if (name[0] == 't' || name[0] == 'T') return "transit";
	else if (name[0] == 'c' || name[0] == 'C') return "client";
	/* else if (name[0] == 'a' || name[0] == 'A') return "as"; */
    }

    /* classify by outdegree */
    edge e;
    int deg = g->degree(v);
    if (deg == 1) return "client";
    forall_adj_edges(e, v)
	if (g->degree(opposite(v,e)) == 1) return "stub";

    return "transit";
/*
    static bool warned = false;
    if (!warned) {
	cerr << "Warning: some nodes can not be typed: e.g. \""
	     << name <<"\"\n";
	warned = true;
    }
    return "unknown";
*/
}

/*
 * Derive a type for an untyped edge.
 */
const char *derive_type(const edge e)
{
    const char *src = derive_type(source(e));
    const char *dst = derive_type(target(e));

    if (!strcmp(src, "client")) {
	if (!strcmp(dst, "client")) return "client-client";
	if (!strcmp(dst, "stub")) return "client-stub";
	if (!strcmp(dst, "transit")) return "client-transit";
    } else if (!strcmp(src, "stub")) {
	if (!strcmp(dst, "client")) return "client-stub";
	if (!strcmp(dst, "stub")) return "stub-stub";
	if (!strcmp(dst, "transit")) return "stub-transit";
    } else if (!strcmp(src, "transit")) {
	if (!strcmp(dst, "client")) return "client-transit";
	if (!strcmp(dst, "stub")) return "stub-transit";
	if (!strcmp(dst, "transit")) return "transit-transit";
    }

    static bool warned = false;
    if (!warned) {
	const mn_graph *g = (mn_graph *)graph_of(e);
	cerr << "Warning: some edges can not be typed: e.g. \""
	     << (*g)[e].s("label")<<"\" between "<<src<<" and "<<dst<<"\n";
	warned = true;
    }
    return "unknown";
}

