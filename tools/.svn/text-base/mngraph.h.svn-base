#ifndef __MNGRAPH_H_
#define __MNGRAPH_H_

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>

#include "graph.h"
#include <map>
#include <string>
using namespace std;
class AttributeMap 
{
 public:
   AttributeMap();
   AttributeMap(const AttributeMap& other);
   const string& getString(const string& key) const;
   const int& getInteger(const string& key) const;
   const double& getDouble(const string& key) const;
   void setString(const string& key, const string& s);
   void setInteger(const string& key, const int& i);
   void setDouble(const string& key, const double& d);
   bool hasString(const string& key) const;
   bool hasInteger(const string& key) const;
   bool hasDouble(const string& key) const;
   template <class actor> void forEachString(const actor& a) {
      for (map<string,string>::iterator it=myStrings.begin(); it!=myStrings.end();it++)
	a(*this, it->first, it->second);
   }
   template <class actor> void forEachInteger(const actor& a) {
      for (map<string,int>::iterator it=myInts.begin(); it!=myInts.end();it++)
	a(*this, it->first, it->second);
   }
   template <class actor> void forEachDouble(const actor& a) {
      for (map<string,double>::iterator it=myDoubles.begin(); it!=myDoubles.end();it++)
	a(*this, it->first, it->second);
   }
 private:
   map<string, string> myStrings;
   map<string, int> myInts;
   map<string, double> myDoubles;
};

class MNGraph : public Graph<AttributeMap, AttributeMap> {
 public:
  typedef Graph<AttributeMap, AttributeMap> GraphType;
  AttributeMap& specs(const string& label);
  AttributeMap& specsFor(const EdgeIterator& e);
  template <class actor> void forAllSpecs(const actor& a) {
    for (map<string, AttributeMap>::iterator it=mySpecs.begin(); it!=mySpecs.end(); it++)
      a(it->first, it->second);
  }
 private:
  map<string, AttributeMap> mySpecs;
};

typedef VertexIterator vertex;
typedef EdgeIterator edge;
#define edge_array EdgeArray
#define vertex_array VertexArray
#define forall_vertices(v,g)  for(v=g.vertexBounds().first; v!=g.vertexBounds().second; v++)
#define forall_edges(e,g)  for(e=g.edgeBounds().first; e!=g.edgeBounds().second; e++)
#define forall_out_edges(e,v,g) for(unsigned int i__=0; i__<g.outEdges(v).size() && (e=g.outEdges(v)[i__],true); i__++)
#define forall_in_edges(e,v,g) for(unsigned int i__=0; i__<g.inEdges(v).size() && (e=g.inEdges(v)[i__],true); i__++)

bool readGraphXML(const string& filename, MNGraph& g);
bool writeGraphXML(ostream& out, MNGraph& g);

class Looper
{
 public:
  Looper() : started(false) {}
  template <class T, class Container>
    bool next(T& val, Container& c) {
    typename Container::iterator* it = (typename Container::iterator*)myIterator;
    if (!started) {
      started=true;
      *it = c.begin();
    }
    if (*it == c.end())
      return false;
    val = **it;
    (*it)++;
    return true;
  }
 private:
  char myIterator[32];
  bool started;
};

#define forall(v,c) for (Looper looper; looper.next(v,c);)

#endif
