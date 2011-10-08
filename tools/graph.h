#ifndef __GRAPH_H_
#define __GRAPH_H_

#include <vector>
#include <set>
#include <algorithm>
#include <deque>
using namespace std;

struct VertexIterator
{
  int index;
  VertexIterator(int i=-1) : index(i) {}
  bool isNil() const {return index == -1;}
  bool operator==(const VertexIterator& o) const { return o.index == index; }
  bool operator!=(const VertexIterator& o) const { return o.index != index; }
  bool operator<(const VertexIterator& o) const { return index < o.index; }
  void operator++(int dummy) {index++;}
  void operator++() {index++;}
  static VertexIterator nil;
};
struct EdgeIterator
{
  int index;
  EdgeIterator(int i=-1) : index(i) {}
  bool isNil() const {return index == -1;}
  bool operator==(const EdgeIterator& o) const { return o.index == index; }
  bool operator!=(const EdgeIterator& o) const { return o.index != index; }
  bool operator<(const EdgeIterator& o) const { return index < o.index; }
  void operator++(int dummy) {index++;}
  void operator++() {index++;}
  static EdgeIterator nil;
};

template <class T> class VertexArray {
 public:
  template <class G>
  VertexArray(const G& g, const T& initial=T()) : myValues(g.vertexCount(), initial){}
  T& operator[](const VertexIterator& vi) { return myValues[vi.index]; }
  const T& operator[](const VertexIterator& vi) const { return myValues[vi.index]; }
  template <class G> void init(const G& g, const T& initial=T()) {
    fill(myValues.begin(), myValues.end(), initial);
  }
 private:
  vector<T> myValues;
};
template <class T> class EdgeArray {
 public:
  template <class G>
  EdgeArray(const G& g, const T& initial=T()) : myValues(g.edgeCount(), initial){}
  T& operator[](const EdgeIterator& ei) { return myValues[ei.index]; }
  const T& operator[](const EdgeIterator& ei) const { return myValues[ei.index]; }
  template <class G> void init(const G& g, const T& initial=T()) {
    fill(myValues.begin(), myValues.end(), initial);
  }
 private:
  vector<T> myValues;
};

template <class VertexAttr, class EdgeAttr>
class Graph 
{
 public:	
   Graph() {}  
   VertexIterator addVertex() {
      myVertices.push_back(Vertex());
      myInEdges.push_back(vector<EdgeIterator>());
      myOutEdges.push_back(vector<EdgeIterator>());
      return VertexIterator(myVertices.size()-1);
   }
   EdgeIterator addEdge(const VertexIterator& src, const VertexIterator& target) {
      Edge e;
      e.source = src.index;
      e.target = target.index;
      myEdges.push_back(e);
      myOutEdges[e.source].push_back(EdgeIterator(myEdges.size()-1));
      myInEdges[e.target].push_back(EdgeIterator(myEdges.size()-1));
      return EdgeIterator(myEdges.size()-1);
   }	
   const vector<EdgeIterator>& outEdges(const VertexIterator& v) const {
      return myOutEdges[v.index];
   }
   const vector<EdgeIterator>& inEdges(const VertexIterator& v) const {
      return myInEdges[v.index];
   }
   VertexIterator source(const EdgeIterator& e) const { return VertexIterator(myEdges[e.index].source);}
   VertexIterator target(const EdgeIterator& e) const { return VertexIterator(myEdges[e.index].target);}
   int outDegree(const VertexIterator& v) const {return outEdges(v).size();}
   int inDegree(const VertexIterator& v) const {return inEdges(v).size();}
   int degree(const VertexIterator& v) const {return outEdges(v).size()+inEdges(v).size();}
   int vertexCount() const { return myVertices.size(); }
   int edgeCount() const { return myEdges.size(); }
   pair<EdgeIterator, EdgeIterator> edgeBounds() const {
      return make_pair(EdgeIterator(0), EdgeIterator(myEdges.size()));
   }
   pair<VertexIterator, VertexIterator> vertexBounds() const {
      return make_pair(VertexIterator(0), VertexIterator(myVertices.size()));
   }
   VertexAttr& operator[](const VertexIterator& v) { return myVertices[v.index].attr; }
   const VertexAttr& operator[](const VertexIterator& v) const { return myVertices[v.index].attr; }
   EdgeAttr& operator[](const EdgeIterator& e) { return myEdges[e.index].attr; }
   const EdgeAttr& operator[](const EdgeIterator& e) const { return myEdges[e.index].attr; }
   template <class actor> int forEachVertex(const actor& a) {
      for (VertexIterator vi(0); vi.index < (int)myVertices.size(); vi++) 
	a(*this, vi);
      return vertexCount();
   }
   template <class actor> int forEachEdge(const actor& a) {
      for (EdgeIterator ei(0); ei.index < (int)myEdges.size(); ei++) 
	a(*this, ei);
      return edgeCount();
   }
 private:
   struct Vertex 
     {
	VertexAttr attr;
     };
   struct Edge 
     {
	int source;
	int target;
	EdgeAttr attr;
     };
   vector<vector<EdgeIterator> > myOutEdges;
   vector<vector<EdgeIterator> > myInEdges;
   vector<Vertex> myVertices;
   vector<Edge> myEdges;
};

template <class G>
void breadth_first_search(const G& g, VertexArray<EdgeIterator>& pred, VertexArray<int>& dist, const VertexIterator& source) {
  unsigned int i;
  deque<int> q;
  vector<bool> visited(g.vertexCount(),false);
  q.push_back(source.index);
  dist.init(g,-1);
  dist[source]=0;
  visited[source.index]=true;
  pred.init(g,EdgeIterator::nil);
  while (q.size()) {
    int v = q.front();
    q.pop_front();
    const vector<EdgeIterator>& outs = g.outEdges(v);
    for (i=0; i<outs.size(); i++) {
      int u = g.target(outs[i]).index;
      if (visited[u])
	continue;
      visited[u] = true;
      pred[u] = outs[i];
      dist[u] = dist[v]+1;
      q.push_back(u);
    }
  }
}   

template <class W>
struct SPCompare {
  SPCompare(vector<W>& dist, vector<bool>& inf):d(dist),infinite(inf) {}
  bool operator()(const int& a, const int& b) const {
    return infinite[a] && !infinite[b] || infinite[a] && b < a || !infinite[b] &&
      (d[b]<d[a] || !(d[a]<d[b]) && b<a);
  }
  vector<W>& d;
  vector<bool>& infinite;
};
	   
template <class G, class W> 
int shortest_paths(const G& g, const EdgeArray<W>& weights, VertexArray<EdgeIterator>& pred, const VertexIterator& s) 
{
  vector<W> d(g.vertexCount());
  vector<bool> infinite(g.vertexCount(), true);
  SPCompare<W> comp(d,infinite);
  vector<int> heap(g.vertexCount());
  unsigned int i;
  for (i=0;i<(unsigned int)g.vertexCount();i++) {
    heap[i] = i;
    pred[i]= -1;
  }
  d[s.index]=0;
  infinite[s.index]=false;
  size_t heapSize = heap.size();
  while (heapSize) {
    make_heap(heap.begin(), heap.begin()+heapSize, comp);
    pop_heap(heap.begin(), heap.begin()+heapSize, comp);
    heapSize--;
    int u = heap[heapSize];
    const vector<EdgeIterator>& outs = g.outEdges(u);
    for (i=0;i<outs.size();i++) {
      int v = g.target(outs[i]).index;
      if (infinite[v] || d[u] + weights[outs[i]] < d[v]) {
	d[v] = d[u] + weights[outs[i]];
	infinite[v] = false;
	pred[VertexIterator(v)] = outs[i];
      }
    }
  }
  return g.vertexCount();
}

#endif
