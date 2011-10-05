/*
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 */

/*************************************
 * AttributeMap maps strings to strings, ints, and doubles, and
 * provides iteration functions.
 *************************************/

class AttributeMap
{
public:
	AttributeMap(){}
	AttributeMap(const AttributeMap& other) {
		_strings = other._strings;
		_ints = other._ints;
		_doubles = other._doubles;
	}
	bool hasString(const string& key) const {return _strings.count(key)>0;}
	bool hasInt(const string& key) const {return _ints.count(key)>0;}
	bool hasDouble(const string& key) const {return _doubles.count(key)>0;}
	string& s(const string& key) {return _strings[key];}
	int& i(const string& key) {return _ints[key];}
	double& d(const string& key) {return _doubles[key];}
	template <class actor> void forEachString(const actor& a){
		map<string,string>::iterator it;
		for (it = _strings.begin(); it != _strings.end(); it++)
			a(it->first, it->second);
	}
	template <class actor> void forEachInt(const actor& a){
		map<string,int>::iterator it;
		for (it = _ints.begin(); it != _ints.end(); it++)
			a(it->first, it->second);
	}
	template <class actor> void forEachDouble(const actor& a){
		map<string,double>::iterator it;
		for (it = _doubles.begin(); it != _doubles.end(); it++)
			a(it->first, it->second);
	}
private:
	map<string,string> _strings;
	map<string,int> _ints;
	map<string,double> _doubles;
};

/*************************************
 * BaseGraphType contains only a type, GraphType.
 * GraphType is a refinement of adjacency_list,
 * using ATTR as vertex and edge properties.
 *************************************/

template <class ATTR>
struct BaseGraphType
{
	typedef adjacency_list<vecS, vecS, bidirectionalS, 
	 property<vertex_name_t, ATTR>, 
	 property<edge_name_t, ATTR, property<edge_weight_t, int> >
		 > GraphType;
};


/*************************************
 * MyGraph defines the basic attribute-propertied graph.
 * It extends ATTR as well so that the graph itself has attributes.
 *************************************/

template <class ATTR>
class MyGraph : public BaseGraphType<ATTR>::GraphType, public ATTR
{
public:
	typedef typename BaseGraphType<ATTR>::GraphType Base;
	typedef typename graph_traits<Base>::vertex_descriptor Vertex;
	typedef typename graph_traits<Base>::vertex_iterator VertexIterator;
	typedef typename graph_traits<Base>::edge_descriptor Edge;
	typedef typename graph_traits<Base>::edge_iterator EdgeIterator;

	//On construction, assign id's to vertices
	MyGraph(unsigned int N=0): Base(N)
	{
		_vertexAttr = get(vertex_name_t(), *this);
		_edgeAttr = get(edge_name_t(), *this);
		VertexIterator i, end;
		for(boost::tie(i,end) = vertices(*this); i != end; ++i) 
		{
			_vertexAttr[*i].i("idx")=*i;
		}
  	}

	// Attribute accessors
	ATTR& operator[](const Vertex v){return _vertexAttr[v];}
	ATTR& operator[](const Edge e){return _edgeAttr[e];}
	const ATTR& operator[](const Vertex v) const {return _vertexAttr[v];}
	const ATTR& operator[](const Edge e) const {return _edgeAttr[e];}

	// Edge iteration
	template <class actor> void for_each_edge(const actor& a) {
		EdgeIterator i,end;
		for (boost::tie(i,end) = edges(*this); i != end; ++i)
			a(*this, *i);
	}

	// Vertex iteration
	template <class actor> void for_each_vertex(const actor& a) {
		VertexIterator i,end;
		for (boost::tie(i,end) = vertices(*this); i != end; ++i)
			a(*this, *i);
	}
private:
	// Our attribute property maps
	typename property_map<Base, vertex_name_t>::type _vertexAttr;
	typename property_map<Base, edge_name_t>::type _edgeAttr;
};

// The ModelNet graph type
typedef MyGraph<AttributeMap> MNGraph;


MNGraph* readgraph(const char *graphfile);
void write_to_xml(MNGraph& g, ostream& out = cout);

