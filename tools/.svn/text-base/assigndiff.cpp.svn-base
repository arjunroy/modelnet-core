/***************
 assigndiff takes two pipe assignments in the form of a topology xml file with "emul" fields on each edge, and outputs the the pipes which have changed owners (after remapping the partitions to minimize changes.)
***************/
#include <set>
#include <map>
#include <vector>
#include <string>
#include <queue>
using namespace std;
#include "mngraph.h"

bool verbose = false;
int main(int argc, char* argv[])
{
  if (argc != 3) {
    cerr << "usage: assigndiff asg1.xml asg2.xml" << endl;
    exit(1);
  }
  MNGraph g1,g2;
  if (!readGraphXML(argv[1],g1) || !readGraphXML(argv[2],g2)) {
    cerr << "error: reading graphs" << endl;
    exit(1);
  }

  map<int, int> asg1, asg2;
  map<int, int> partSizes;
  set<int> entrypipes;
  edge e1,e2;
  string name;
  forall_edges(e1,g1) {
    if (!g1[e1].hasInteger("emul")) {
      cerr << "error: graph1 is missing assignments." << endl;
      exit(1);
    }
    asg1[e1.index]=g1[e1].getInteger("emul");
  }
  forall_edges(e2,g2) {
    if (!g2[e2].hasInteger("emul")) {
      cerr << "error: graph1 is missing assignments." << endl;
      exit(1);
    }
    int asg = g2[e2].getInteger("emul");
    int nr = e2.index;
    if (!asg1.count(nr)) {
      cerr << "error: graph2 has pipe " << nr<< " and graph1 doesn't."<<endl;
      exit(1);
    }
    if (g2.outEdges(g2.source(e2)).size() == 1)
      entrypipes.insert(nr);
    asg2[nr]=asg;
    if (!partSizes.count(asg))
      partSizes[asg]=0;
    partSizes[asg]++;
  }
  priority_queue<pair<int,int> > parts;
  pair<int, int> entry;
  forall(entry, partSizes) {
    parts.push(make_pair(entry.second, entry.first));
  }
  map<int,int> perm, revperm;
  int part2, part1, nr;
  while (parts.size()) {
    part2 = parts.top().second;
    parts.pop();
    int bestscore = INT_MAX;
    int bestpart = -1;
    pair<int, int> entry;
    forall(entry, partSizes) {
      part1 = entry.first;
      if (perm.count(part1))
	continue;
      int mismatches = 0;
      pair<int,int> asgEntry;
      forall(asgEntry,asg2) {
	nr = asgEntry.first;
	if (asgEntry.second != part2)
	  continue;
	if (asg1[nr]!=part1)
	  mismatches++;
      }
      if (mismatches < bestscore) {
	bestscore = mismatches;
	bestpart = part1;
      }
    }
    perm[bestpart]=part2;
  }
  pair<int,int> asgEntry;
  forall(asgEntry,perm) {
    part1 = asgEntry.first;
    revperm[perm[part1]]=part1;
    if (verbose) cerr << perm[part1] << " --> " << part1 <<endl;
  }
  int changes = 0;
  int pipes = 0;
  int tries = 0;
  forall(asgEntry,asg2) {
    nr = asgEntry.first;
    if (revperm.count(asg2[nr]))
      asg2[nr]=revperm[asg2[nr]];
    if (asg2[nr] != asg1[nr]) {
      if (entrypipes.count(nr))
	tries++;
      else {
	cout << nr << "\t" << asg1[nr] <<"\t" << asg2[nr] << endl;
	changes++;
      }
    }
    pipes++;
  }
  cerr << changes << " pipes changed out of " << pipes <<", "<<tries<<" entry pipes forced." <<  endl;
}
