#include <time.h>
#include <set>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace std;
#include <unistd.h>
#include <ctype.h>
#include <popt.h>

#include "mngraph.h"
#include "xml.h"
#include "assignment.h"
#include "flow.h"

void usage()
{
  cerr<<"Usage: assign <input.xml>  -cores cores.xml -o outputFile "<<endl
      <<"\t[-ones | -pattern <patternFile> | -flowmap <flowmapFile> | -worst] "<<endl
      <<"\t[-transform <transformFile>] [-seed <randSeed>]"<<endl
      <<"\t[-random #] [-blob #] [-chaco_node] [-chaco_edge] [-metis_node] [-metis_edge] [-existing]"<<endl;
}

void usage(poptContext optCon, const string& error, const string& more) 
{
  poptPrintUsage(optCon, stderr, 0);
  fprintf(stderr, "%s: %s\n", error.c_str(), more.c_str());
  exit(1);
}

//This holds info about the arguments
struct Settings
{
  string input;
  string hostsInput;
  string flowmapInput;
  string patternInput;
  string output;
  string transformOutput;
  string statsOutput;
  string weighting;
  vector<assignment_factory*> methods;
  int randomSeed;
  map<string, bool> flags;
};

int getArgs(int argc, const char* argv[], struct Settings& settings)
{
  const char* inputFile=0;
  char* outputFile=0;
  char* statsFile=0;
  char* patternFile=0;
  char* transformFile=0;
  char* hostsFile=0;
  char* flowmapFile=0;
  char* weighting=0;
  char* method=0;
  long randomSeed=time(0);
  poptOption optArray[]={
    {"output", 'o', POPT_ARG_STRING, &outputFile, 'o', "specify the output graph file" ,"<output file>"},
    {"stats", 0, POPT_ARG_STRING, &statsFile, 'S', "output statistics to a file", "<stats output file>"},
    {"pattern", 'p', POPT_ARG_STRING, &patternFile, 'p', "specify a pattern file", "<pattern file>"},
    {"transform", 't', POPT_ARG_STRING, &transformFile, 't', "transform the graph", "<transformed graph output file>"},
    {"seed", 's', POPT_ARG_LONG, &randomSeed, 's', "specify random seed", "<random seed>"},
    {"hosts", 'h', POPT_ARG_STRING, &hostsFile, 'h', "specify hosts description xml file", "<hosts file>"},
    {"flowmap", 'f', POPT_ARG_STRING, &flowmapFile, 'f', "specify flowmap file", "<flowmap file>"},
    {"method", 'm', POPT_ARG_STRING, &method, 'm', 
     "specify a partitioning method", 
     "{metis_edge | metis_node | chaco_edge | chaco_node | random | kcluster | existing}[:iterations]\n\
      for instance: '-m random:5'  One iteration is the default."},
    {"weight", 'w', POPT_ARG_STRING, &weighting, 'w', "specify weighting method", "{ worst | ones | flowmap | pattern }"},
    POPT_AUTOHELP
    {0,0,0,0,0}
  };
  poptContext optCon = poptGetContext(NULL, argc, argv, optArray, 0);
  poptSetOtherOptionHelp(optCon, "<directed input topology>");
  char c;
  while ((c = poptGetNextOpt(optCon)) >= 0) {
    if (c<0)
      usage(optCon, "Bad arguments.","");
    else if (c == 'm') {
      string mthd = method;
      int iter = 1;
      unsigned int idx=mthd.find(':');
      if (idx != string::npos) {
	iter = atoi(mthd.substr(idx+1).c_str());
	mthd = mthd.substr(0, idx);
	if (iter <= 0 || iter > 1000) 
	  usage(optCon, 
		string("Bad number of iterations for partitioning method ")+mthd, "Must have 1<=i<=1000.");
      }
      if (mthd == "metis_edge") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new metis_assignment::factory(true));
      else if (mthd == "metis_node") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new metis_assignment::factory(false));
      else if (mthd == "chaco_edge") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new metis_assignment::factory(true));
      else if (mthd == "chaco_node") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new metis_assignment::factory(false));
      else if (mthd == "random") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new random_assignment::factory());
      else if (mthd == "kcluster") 
	for (int i=0;i<iter; i++) settings.methods.push_back(new blob_assignment::factory());
      else if (mthd == "existing") 
	settings.methods.push_back(new existing_assignment::factory());
      else
	usage(optCon, string("Invalid partitioning method: ")+mthd, "");
    }
  }
  inputFile = poptGetArg(optCon);
  if (inputFile == 0)
    usage(optCon, "Must specify input file.", "e.g. mirrored.graph");
  if (settings.methods.size() == 0)
    usage(optCon, "No partitioning methods specified.", "Valid methods are \n\
    'metis_edge', 'metis_node', 'chaco_edge', 'chaco_node', 'random', 'kcluster', and 'existing'.");
  if (hostsFile == 0)
    usage(optCon, "No hosts file specified.", "e.g. experiment.hosts");
  if (weighting == 0) {
    fprintf(stderr,"No weighting specified; using worst-case weighting.\n");
    weighting = "worst";
  }
  if (string(weighting) == "flowmap" && flowmapFile==0)
    usage(optCon, "Specified flowmap weighting, but no flowmap.", "e.g. -w flowmap -f myflows.txt");
  if (string(weighting) == "pattern" && patternFile==0)
    usage(optCon, "Specified flow pattern weighting, but no pattern file.", "e.g. -w pattern -p myflowpattern.fp");
  if (transformFile)
    settings.flags["transform"] = true;
  if (statsFile)
    settings.flags["stats"] = true;
  settings.input = inputFile;
  settings.output = outputFile == 0 ? settings.input : outputFile;
  settings.hostsInput = hostsFile ? hostsFile : "";
  settings.patternInput = patternFile ? patternFile : "";
  settings.flowmapInput = flowmapFile ? flowmapFile : "";
  settings.statsOutput = statsFile ? statsFile : "";
  settings.transformOutput = transformFile ? transformFile : "";
  settings.weighting = weighting;
  settings.randomSeed = randomSeed;
  return true;
}

//Output a transform graph for diagnostic purposes
int outputTransformGraph(weighted_graph &wg, ostream& out)
{
  MNGraph & g = wg.g;
  MNGraph transform;
  edge e,e1;
  edge_array<vertex> toNode(g,vertex::nil);
  vertex vt;
  forall_edges(e,g) {
    if (wg.weight(e) == 0)
      continue;
    toNode[e]=transform.addVertex();
    transform[toNode[e]].setInteger("weight", (int) wg.scaledWeight(e));
  }
  forall_edges(e,g) {
    if (toNode[e].isNil())
      continue;
    vt = g.target(e);
    forall_out_edges (e1, vt, g) {
      double w=wg.flowAcross(e,e1);
      if (w==0.0)
	continue;
      edge newEdge=transform.addEdge(toNode[e],toNode[e1]);
      transform[newEdge].setInteger("weight", (int)(w * wg.edgeScale()));
    }
  }
  printf("Score factor: %f\n",wg.edgeScale()/2);
  printf("Saving transform...\n");
  return writeGraphXML(out, transform);
}

//Save the assigned graph to an xml file.
void saveAssign(assignment* asg, MNGraph& g, const string& outputName)
{
  edge e;
  forall_edges(e,g)
    g[e].setInteger("emul", asg ? asg->assignmentOf(e) : 0);
  ofstream asgOut(outputName.c_str());
  if (asgOut.fail()) {
    cerr<<"Could not open output file \""<<outputName<<"\""<<endl;
    exit(1);
  }
  writeGraphXML(asgOut, g);
  asgOut.close();
}

int main(int argc, const char* argv[]) 
{
  struct Settings settings;

  clock_t startTime;
  float t;
  MNGraph g;

  //Parse the args
  getArgs(argc, argv, settings);

  cout << "Random seed: " << settings.randomSeed << endl;
  srand(settings.randomSeed);
  
  //Output statistics?
  
  FILE* statsOut=0;
  if (settings.flags["stats"]) {
    statsOut = fopen(settings.statsOutput.c_str(),"a");
    if (statsOut == 0) {
      cerr << "Bad stats filename: " << settings.statsOutput << endl;
      exit(1);
    }
  }

  //Read cores info
  string hosts = settings.hostsInput;
  ifstream hostsFile(hosts.c_str());
  bool exists = !hostsFile.fail();
  hostsFile.close();
  vector<unsigned int> splits;
  if (!exists) {
    //The argument is a list of comma separated numbers
    istringstream numList(hosts);
    unsigned int num;
    char comma=',';
    while (numList >> num) {
      splits.push_back(num);
      if (!numList>>comma)
	break;
    }
  }
  else  {
    //The argument is the name of a cores.xml file
    XMLElement hostsXML;
    if (!readXML(hosts, hostsXML)) {
      cerr << "Error reading hosts file." << endl;
      exit(1);
    }
    vector<const XMLElement*> emuls = hostsXML.findChildrenByName("emul");
    splits.push_back(emuls.size());
  }
  if (!readGraphXML(settings.input, g)) {
    cerr<<"Error parsing input graph!"<<endl;
    exit(1);
  }
  weighted_graph wg(g);
  cerr << wg.getClients().size() << " virtual nodes." << endl;
  //Now we weight the graph by the requested method
  if (settings.weighting == "pattern") {
    //Read the pattern in and use it to weight
    user_flow_pattern p;
    ifstream in(settings.patternInput.c_str());
    if (in.fail()) {
      cerr << "Error opening pattern file: " << settings.patternInput << endl;
      exit(1);
    }
    if (!p.read_pattern(in)) {
      cerr<<"Error reading pattern file: " << settings.patternInput << endl;
      exit(1);
    }
    wg.weight_by_pattern(&p);
    wg.scaleWeights();
  }
  else if (settings.weighting == "ones") {
    //Weight all edges and vertices with 1
    wg.weight_ones();
  }
  else if (settings.weighting == "flowmap") {
    //Use a flowmap to weight
    wg.weight_by_flowmap(settings.flowmapInput.c_str());
    wg.scaleWeights();
  }
  else if (settings.weighting == "worst") {
    //Use worst-case
    wg.weight_worst_case();
    wg.scaleWeights();
  }
  else {
    cerr << "Invalid weighting strategy: " << settings.weighting << endl;
    exit(1);
  }
  
  if (settings.flags["transform"]) {
    ofstream tOut(settings.transformOutput.c_str());
    if (tOut.fail()) {
      cerr << "Error opening transformed graph output file: " << settings.transformOutput << endl;
      exit(1);
    }
    outputTransformGraph(wg, tOut);
    tOut.close();
  }
  
  //Check for bad weighting
  if (wg.maxVWeight == 0 && wg.maxEWeight == 0) {
    cerr<<"Error: All edges or vertices have zero weight."<<endl;
    cerr<<"Have you run 'addspecs' ?"<<endl;
    exit(1);
  }
  assignment_factory* f;
  int numCores;
  assignment* best=0;

  //For each set of cores, do the assignments
  forall(numCores, splits) {
    cerr << "Assigning to " << numCores << " emulators..." << endl;
    int partV,partE;
    forall(f,settings.methods) {
      //Is there anything to do?
      if (numCores>1) {
	assignment* a=f->create(wg,numCores);
	cerr << "Using " << a->name() << "...";
	cerr.flush();
	a->getGraphSize(partV,partE);
	startTime=clock();
	a->assign();
	t=(clock()-startTime)/float(CLOCKS_PER_SEC);
	if (statsOut)
	  fprintf(statsOut,"%12d %12d %12d %6d %2d %s %10.0f %5.2f\n",
		  wg.g.vertexCount(), wg.g.edgeCount(), partE, 
		  wg.getClients().size(), numCores, a->name(), a->getBadness().first, t);
	cerr << " cross traffic = " << a->getBadness().first << endl;
	//Was this one better than the best so far?
	if (best==0 || a->getBadness().first < best->getBadness().first ) {
	  if (best)
	    delete best;
	  best=a;
	}
	else
	  delete a;
      }
      else if (settings.output.length()) {//There's nothing to do, dump all in one
	cerr<<"Putting everything in the one core..."<<endl;
	saveAssign(0,g,settings.output);
	break;
      }
    }
  }
  if (best) {
    //We found a best assignment, so save it if necessary
    cerr<<"Best was "<<best->name()<<", cross traffic = "<< best->getBadness().first <<"."<<endl;
    if (settings.output.length())
      saveAssign(best,g,settings.output);
    delete best;
  }
  //Cleanup
  forall(f,settings.methods)
    delete f;
  if (statsOut)
    fclose(statsOut);
  return 0;
}
