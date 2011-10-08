#include <stdio.h>
#include <LEDA/set.h>
#include <LEDA/array.h>
#include <LEDA/stream.h>
#include <LEDA/graph.h>
#include <LEDA/gml_graph.h>
#include <string.h>
#include "gmlp.h"
using namespace std;
/*
 * A simple program to add clients (aka endpoints) to a topology.
 */ 


// Patterns
#define AMONG_PATTERN "among"
#define EACH_PATTERN  "each"

// Prototypes
void usage();
bool isValidPattern(char* pattern);
bool addClients(const int numDesiredClients, const char* pattern, const int numStubs, mn_graph& g);

// Declarations
void usage()
{
  cerr << "USAGE: addclients -p <pattern> [-f <input filename>] [-o <output filename>]\n"
       << " Where <pattern> can be:\n"
       << "   <n> among <k> stub -- <n> clients distributed among the first <k> stub nodes\n"
       << "                     ordered according to their non-client degrees\n"
       << "                     (n) clients total\n\n"
       << "   <n> each  <k> stub -- <n> clients put at each of <k> stub nodes\n"
       << "                     (n * k) clients total\n";
  exit(2);
}

bool isValidPattern(char* pattern)
{
  return ((!strcmp(pattern,AMONG_PATTERN)) || 
	  (!strcmp(pattern,EACH_PATTERN)));
}

void fileOpenTest(ios & stream, string filename)
{
  if (stream.fail())
    {
      cerr << "Failed to open file: " << filename << endl;
      exit(2);
    }
}

struct future_stub
{
  future_stub() {};
  future_stub(node v, int des_clients): n(v), desired_clients(des_clients) {}
  node n;
  int desired_clients;
};

int comparator(const future_stub& lhs, const future_stub& rhs)
{
  if (lhs.desired_clients < rhs.desired_clients)
    return 1;
  return ((lhs.desired_clients > rhs.desired_clients) ? -1 : 0);
}
bool isClientType(const node n, const mn_graph & g)
{
  return (g.degree(n) == 1);
}

// Distribute desiredClientTotal among the first numStubs nodes (after they're
// sorted according to descending, non-client degree)
void distClients(const int desiredClientTotal, const int numStubs, mn_graph& g)
{
  node n, n2;
  edge e, e2;
  int i;
  list_item lit;
  dic_item dit;
  // the degree of a node excluding clients
  node_array<int> degrees(g, -1);
  list<future_stub> future_stubs;
  dictionary<node, int> clients_desired;
  int new_client_total = 0;
  int num_clients = 0;
  double scalar;
  
  g.make_undirected();
  forall_nodes(n, g)
    {
      if (isClientType(n, g))
	{
	  num_clients++;
	  forall_adj_edges(e, n)
	    {
	      n2 = opposite(n, e);
	      if (degrees[n2] == -1)
		{
		  degrees[n2]++;
		  forall_adj_edges(e2, n2)
		    if (!isClientType(opposite(n2, e2), g))
		      degrees[n2]++;
		}
	      clients_desired.insert(n, degrees[n2]);
	      new_client_total += degrees[n2];
	    }
	}
    }
  
  if (desiredClientTotal < num_clients)
    {
      cerr << "You can't add fewer clients than you currently have (" 
	   << desiredClientTotal 
	   << " < " 
	   << num_clients 
	   << ")" << endl;
      return;
    }

  if (clients_desired.empty())
    {
      cerr << "I don't know where to add the clients!" << endl;
      return;
    }

  scalar = double(desiredClientTotal) / num_clients;
  new_client_total = 0;
  forall_items(dit, clients_desired)
    {
      clients_desired[dit] = int(ceil(scalar * clients_desired[dit]));
      new_client_total += clients_desired[dit];
      future_stub s(clients_desired.key(dit), clients_desired.inf(dit));
      future_stubs.push(s);
    }
  
  future_stubs.sort(comparator);

  while(future_stubs.size() > numStubs)
    future_stubs.pop_back();

  // new_client_total has to be greater than desiredClientTotal b/c of rounding
  // so we only need to worry about subtracting clients
  bool nothingchanged;
  while (new_client_total != desiredClientTotal)
    {
      nothingchanged = true;
      forall_items(lit, future_stubs)
	{
	  int & cur = clients_desired[clients_desired.lookup(future_stubs[lit].n)];
	  
	  if (new_client_total == desiredClientTotal)
	    break;
	  else if (cur > 1)
	    {
	      nothingchanged = false;
	      cur--;
	      new_client_total--;
	    }
	}
      if (nothingchanged)
	{
	  cerr << "Fatal Error: Could not find anybody to take clients away from.  You shouldn't be reading this." << endl;
	  exit(-1);
	}
    }

  forall_items(dit, clients_desired)
    for (i = 0; i < clients_desired.inf(dit); i++)
      {
	n = g.new_node();
	g[n].s("type") = "client";
	g.new_edge(n, clients_desired.key(dit));
      }
}

bool addClients(const int numDesiredClients, const char* pattern, const int numStubs, mn_graph& g)
{
  
  if(!strcmp(pattern,AMONG_PATTERN))
    {
      distClients(numDesiredClients, numStubs, g);	
      return true;
    }

  array<node> parents(g.number_of_nodes());
  int p = 0;

  /* find all the stub nodes */
  node v;
  forall_nodes(v, g) {
    /* attempt to classify as stub or transit */
    string &type = g[v].s("type");
    if (type.length() == 0) type = derive_type(v);
    if (type == "stub") parents[p++] = v;
  }

  /* choose k */
  if (p < numStubs) {
    cerr << "Warning: not enough stub nodes ("<<p<<")\n";
  } else while (p > numStubs) {
    int r = rand() % p;
    parents[r] = parents[--p];
  }

//   if (!do_each) {
//     for (int i = 0; i < num_clients; i++) {
//       int r = rand() % p;
//       v = g.new_node(Attributes());
//       string_ostream label;
//       label << "client "<<i;
//       g[v].s("label") = label.str();
//       g[v].s("type") = "client";
//       edge e = g.new_edge(parents[r], v, Attributes());
//       string_ostream elabel;
//       elabel << label.str() << " <-> "<<g[parents[r]].s("label");
//       g[e].s("label") = elabel.str();
//     }
//   } else { /* do_each */

  for(int k = 0; k < p; k++) {
    for (int i = 0; i < numDesiredClients; i++) {
      v = g.new_node(Attributes());
      g.new_edge(parents[k], v, Attributes());
    }
  }

  return true;
}

// The program's main entrypoint
int main(int argc, char** argv)
{
  char *inFilename = NULL;
  char *outFilename = NULL;

  int numDesiredClients = -1;
  char* pattern = NULL;
  int numStubs = -1;

  argc--; argv++;
  while (argc) 
    {
      if        (!strcmp(argv[0], "-f") && argc > 1) {
	inFilename        = argv[1];
	argc-=2; argv+=2;
      } else if (!strcmp(argv[0], "-o") && argc > 1) {
	outFilename       = argv[1];
	argc-=2; argv+=2;
      } else if (!strcmp(argv[0], "-p") && argc > 4) {
	numDesiredClients = atoi(argv[1]);
	pattern           = argv[2];
	numStubs          = atoi(argv[3]);
	if (strcmp(argv[4], "stub")) usage();
	argc-=5; argv+=5;
      } else usage();
    }

  if (numDesiredClients < 1) 
    {	
      cerr << "Must enter a positive number of clients to add." << endl;
      usage();
    }
  
  if (!isValidPattern(pattern))
    {
      cerr << "Must enter a valid pattern." << endl;
      usage();
    }

  if (numStubs < 1)
    {
      cerr << "Must enter a positive number of stubs to add to." << endl;
      usage();
    }

  mn_graph g;
  raitner_parser gml(g);
  int ok;

  if (inFilename)
    ok = gml.parse(inFilename);
  else
    ok = gml.parse(stdin, "stdin");

  if (!ok) 
    {
      cerr << "Error parsing input file: \"" << inFilename << "\"." << endl;
      exit(1);
    }

  addClients(numDesiredClients, pattern, numStubs, g);

  ofstream fout;
  if (outFilename)
    {
      fout.open(outFilename);
      cout = fout;
      fileOpenTest(cout, outFilename);
    }

  cout.setf(ios::showpoint);
  g.write_gml(cout, write_node_attribute, write_edge_attribute);
  fout.close();
}
