/*******
 gentargets makes a list of flows based on a few different strategies and a topology.
*******/

#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace std;
#include "mngraph.h"


void usage(const char* prog) {
  cerr << "usage: "<<prog<<" <random|subtree|dynsubtree|dynrandom> <xmlfile> <#targets>"<<endl;
  exit(1);
}

void subtreeDynamicTargets(MNGraph& g, unsigned int targetCount)
{
  vector<vertex> clients;
  map<int, vector<vertex> > subTrees;
  vertex_array<int> idOf(g);
  vertex v;
  forall_vertices(v,g) {
    if (g[v].getString("role") != "virtnode")
      continue;
    idOf[v]=g[v].getInteger("vn");
    clients.push_back(v);
    subTrees[g[v].getInteger("subtree")].push_back(v);
  }
  int target1 = rand()%subTrees.size();
  int target2 = rand()%(subTrees.size()-1);
  if (target2 >= target1)
    target2++;
  vector<vertex>& tset1=subTrees[target1];
  vector<vertex>& tset2=subTrees[target2];
  int t1=0,t2=0;
  forall(v,clients) {
    if (g[v].getInteger("subtree") == target1 || g[v].getInteger("subtree") == target2)
      continue;
    set<int> targets1, targets2;
    while (targets1.size() < min(targetCount, tset1.size()))
      targets1.insert( idOf[ tset1[ (t1=(t1+1)%tset1.size()) ] ] );
    while (targets2.size() < min(targetCount, tset2.size()))
      targets2.insert( idOf[ tset2[ (t2=(t2+1)%tset2.size()) ] ] );
    double switchOverTime = (rand()%10000)/10000.0;
    int t;
    cout << idOf[v] << " 2 0.0 " << targets1.size()<< " ";
    forall(t, targets1)
      cout << t << " ";
    cout << switchOverTime << " " << targets2.size() << " ";
    forall(t, targets2)
      cout << t << " ";
    cout << endl;
  }
}

void randomDynamicTargets(MNGraph& g, unsigned int targetCount)
{
  vector<vertex> clients;
  vertex_array<int> idOf(g);
  vertex v;
  forall_vertices(v,g) {
    if (g[v].getString("role") != "virtnode")
      continue;
    idOf[v]=clients.size();
    clients.push_back(v);
  }
  forall(v,clients) {
    set<int> targets1, targets2;
    while (targets1.size() < targetCount || targets2.size() < targetCount) {
      int r = rand()%clients.size()-1;
      if (r >= idOf[v]) r++;
      targets1.insert(r);
      r = rand()%clients.size()-1;
      if (r >= idOf[v]) r++;
      targets2.insert(r);
    }
    double switchTime = (rand()%10000)/10000.0;
    int t;
    cout << idOf[v] << " 2 0 " << targets1.size() << " ";
    forall(t, targets1)
      cout << t << " ";
    cout << switchTime << "  ";
    forall(t, targets2)
      cout << t << " ";
  }
}

void subtreeTargets(MNGraph& g, unsigned int targetCount)
{
    vector<vertex> clients;
    map<int, vector<vertex> > subTrees;
    vertex_array<set<vertex> > targetsOf(g);
    vertex_array<int> idOf(g);
    vertex v;
    forall_vertices(v,g) {
      if (g[v].getString("role") != "virtnode")
	continue;
      idOf[v]=clients.size();
      clients.push_back(v);
      subTrees[g[v].getInteger("subtree")].push_back(v);
    }
    forall(v,clients) {
      int branch = rand()%(subTrees.size()-1);
      if (branch >= g[v].getInteger("subtree"))
	branch++;
      vector<vertex>& tset=subTrees[branch];
      while (targetsOf[v].size() < min(targetCount,tset.size()))
	targetsOf[v].insert(tset[rand()%tset.size()]);
      vertex t;
      cout << idOf[v] <<" ";
      forall(t, targetsOf[v])
	cout << idOf[t] << " ";
      cout << endl;
    }
}

void randomTargets(MNGraph& g, unsigned int targetCount)
{
  vector<vertex> clients;
  vertex v;
  forall_vertices(v,g) {
    if (g[v].getString("role") == "virtnode")
      clients.push_back(v);
  }
  vector<vertex> clientArray(clients.size());
  unsigned int i=0;
  int t;
  forall(v,clients)
    clientArray[i++]=v;
  for(i=0;i<clients.size();i++) {
    set<int> targets;
    while (targets.size() < targetCount) {
      int index = rand()%clientArray.size();
      if (index == (int)i)
	continue;
      targets.insert(index);
    }
    cout << i << " " ;
    forall(t, targets)
      cout << t<<" ";
    cout << endl;
  }
}

int main(int argc, char* argv[]) {
  MNGraph g;
  if (argc != 4)
    usage(argv[0]);
  if (!readGraphXML(argv[2],g)) {
    cerr << "Could not parse input file." << endl;
    exit(1);
  }
  vertex v;
  forall_vertices(v,g) {
    if (g.outEdges(v).size() == 1)
      g[v].setString("role", "virtnode");
  }

  int targetCount = atoi(argv[3]);
  if (string(argv[1]) == "random")
    randomTargets(g,targetCount);
  else if (string(argv[1]) == "subtree")
    subtreeTargets(g,targetCount);
  else if (string(argv[1]) == "dynsubtree")
    subtreeDynamicTargets(g,targetCount);
  else if (string(argv[1]) == "dynrandom")
    randomDynamicTargets(g,targetCount);
  else {
    cerr << "Unrecognized method: " << argv[1] << endl;
    exit(1);
  }
  return 0;
}
