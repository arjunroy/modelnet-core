/* Ethan Eade, 8/30/02
 * This is the implementation of utilities in util.h
 */
#include "util.h"

//Return random int
int randInt()
{
    return ((rand()&0xFFFF)<<16)|(rand()&0xFFFF);
}

//Find all nodes matching a given type.
int findNodes (MNGraph& g, string type, list<vertex>& results)
{
    vertex v;
    results.clear ();
    int count=0;
    forall_vertices(v, g) {
      if (g[v].hasString("role") && g[v].getString("role") == type) {
	results.push_back(v);
	count++;
      }
    }
    return count;
}
