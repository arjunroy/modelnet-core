#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "mngraph.h"
using namespace std;

int main() {
  MNGraph g;
  map<int, string> ismap;
  vector<string> vs;
  set<int> si;
  cout << string("hello") << endl;
  int i;
  forall(i,si)
    cout << i << endl;
  forall(i,si)
    cout << i << endl;
  edge e;
  forall_edges(e,g){}
  return 0;
}
