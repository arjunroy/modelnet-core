/* Ethan Eade, 8/30
 * This is the implementation of the various assignment methods.
 * See assignment.h.
 */
#include "assignment.h"
#include "mngraph.h"

//The libraries are c routines.
extern "C"
{
#include <metis.h>
#include <chaco.h>
}

// Both Chaco and METIS require the edge weights of mirror 
// edges (a,b) and (b,a) to be the same.  This method
// enforces the restriction.
void library_assignment::makeEdgeWeightsSymmetric()
{
  int v, e, e1;
  for (v = 0; v < nverts - 1; v++) {
    for (e = xadj[v]; e < xadj[v + 1]; e++) {
      if (adjacency[e]<v)
	continue;
      for (e1 = xadj[adjacency[e]]; e1 < xadj[adjacency[e] + 1]; e1++) {
	if (adjacency[e1] == v) {
	  eweights[e] = eweights[e1] = MAX((eweights[e] + eweights[e1])/2,1);
	  break;
	}
      }
    }
  }
}

//Construct the base assignment object.
assignment::assignment (weighted_graph & toAssign, int numParts): wg(toAssign), assigns(toAssign.g, 0)
{
  parts = numParts;
  myBadnessDirty = true;
}

//Get a specific assignment.
int assignment::assignmentOf(edge e)
{
  return assigns[e];
}

pair<double, double> assignment::getBadness()
{
  if (myBadnessDirty) {
    badness = evalBadness();
    myBadnessDirty = false;
  }
  return badness;
}

//Badness is a function of crossing flow and imbalance of partitions.
pair<double,double> assignment::evalBadness()
{
  vector<double> bins(parts,0);
  MNGraph & g = wg.g;
  double netstress = wg.getStress(assigns);
  edge e;
  forall_edges (e, g)
    bins[assigns[e]] += wg.weight(e);
  
  int i;
  double minPart, maxPart;
  double sumParts = 0;
  minPart = maxPart = bins[0];
  for (i = 0; i < parts; i++) {
    sumParts += bins[i];
    minPart = MIN (minPart, bins[i]);
    maxPart = MAX (maxPart, bins[i]);
  }
  return make_pair(netstress, maxPart-minPart);
}

// Write the assignment to the "assignment" attribute
// of all edges in the graph.
void assignment::save()
{
  edge e;
  forall_edges (e, wg.g)
    wg.g[e].setInteger("emul", assigns[e]);
}

// Construction of a library based assignment.
// If partition is by edge, construct a transform graph for the libraries.
// Otherwise, remove unused vertices and construct a copy of the graph
// for the libraries.
library_assignment::library_assignment(weighted_graph & toAssign, int numParts, bool byEdge)
  : assignment(toAssign, numParts), indexOfNode(toAssign.g), indexOfEdge(toAssign.g),
  nodeAt(wg.g.vertexCount()), edgeAt(wg.g.edgeCount()), edgePart(byEdge)
{
  edge e;
  vertex v;
  int i=0;
  nverts = 0;
  //Partition by Edge
  if (byEdge) {
    forall_edges(e,wg.g) {
      indexOfEdge[e]=wg.weight(e) > 0 ? nverts++ : -1;  	
      edgeAt[i++]=e;
    }
    constructTransformGraph();
  }
  //Partition by node
  else {
    forall_vertices(v,wg.g) {
      if (!(wg.weight(v) > 0)) {
	bool hasWeight = false;
	forall_out_edges(e,v,wg.g) {
	  if (wg.weight(e) > 0) {
	    hasWeight = true;
	    break;
	  }
	}
	if (!hasWeight) {
	  forall_in_edges(e,v,wg.g) {
	    if (wg.weight(e) > 0) {
	      hasWeight = true;
	      break;
	    }
	  }
	}
	indexOfNode[v] = hasWeight ? nverts++ : -1;
      }
      else
	indexOfNode[v]= nverts++;
      nodeAt[i++]=v;
    }
    constructGraph();
    makeEdgeWeightsSymmetric();
  }
}

// Call the appropriate library routine and clean up after it.
int library_assignment::assign()
{
  MNGraph & g = wg.g;
  doLibraryCall();
  int currPart=0;
  double edgeCut=0;
  if (edgePart) {
    edge e;
    forall_edges (e, g) {
      assigns[e] = indexOfEdge[e] == -1 ? (currPart = (currPart+1)%parts ) : assignvec[indexOfEdge[e]];
    }
  }
  else {
    vector<double> bins(parts);
    vertex v;
    int i;
    forall_vertices(v,g) {
      if (indexOfNode[v] == -1)
	continue;
      bins[assignvec[indexOfNode[v]]]+=wg.weight(v);
    }
    cerr<<"Vertex partition: ";
    for (i=0;i<parts;i++)
      cerr<<bins[i]<<" ";
    cerr<<endl;
    edge e;
    fill(bins.begin(), bins.end(), 0);
    forall_edges (e, g) {
      int it=indexOfNode[g.target(e)];
      int is=indexOfNode[g.source(e)]; 
      if (it == -1 || is == -1) {
	if (wg.scaledWeight(e) > 1.0)
	  cerr << "Fishy: nonzero edge connected to unassigned vertex." << endl;
	assigns[e] = (currPart = (currPart + 1)%parts);
	continue;
      }
      if (assignvec[it]!=assignvec[is])
	edgeCut+=wg.weight(e);
      if (bins[assignvec[it]] > bins[assignvec[is]]) {
	assigns[e] = assignvec[is];
	bins[assignvec[is]] += wg.weight(e);
      }
      else {
	assigns[e] = assignvec[it];
	bins[assignvec[it]] += wg.weight(e);
      }
    }
    cerr<<"Edge cut: "<<edgeCut<<endl;
  }
  return 0;
}

metis_assignment::metis_assignment(weighted_graph & toAssign, int numParts, bool byEdge):
    library_assignment(toAssign, numParts, byEdge)
{
}

// Call the METIS library
int metis_assignment::doLibraryCall()
{
    int n = nverts;
    int weightFlag = xadj[nverts] ? 3 : 2;
    int numFlag = 0;
    int options[5] = { 0 };
    int edgecut = 0;

    METIS_PartGraphKway (&n, xadj, adjacency, vweights, weightFlag == 3 ? eweights : 0,
			 &weightFlag, &numFlag, &parts, options, &edgecut,
			 assignvec);
    return 0;
}

chaco_assignment::chaco_assignment(weighted_graph & toAssign, int numParts, bool byEdge):
    library_assignment(toAssign, numParts, byEdge)
{
}

// Ensure proper weights, convert to floating point, and call Chaco
int chaco_assignment::doLibraryCall()
{
    int n = nverts;
    int coarse=(int) (log(nverts)/log(2)*100);
    float* feweights=(float*)eweights;
    short* sassignvec=(short*)assignvec;
    int mesh[]={parts,0,0};
    int ndims=(int) MIN(log(parts)/log(2),3);
    int i;
    int maxEdge=0;
    for (i=0;i<xadj[n];i++)
	maxEdge=MAX(maxEdge,eweights[i]);
    float scale=MIN(1.0, 100000.0/maxEdge);
    for (i=0;i<xadj[n];i++)
      feweights[i]=float(eweights[i])*scale;
    for (i=0;i<n;i++)
	vweights[i] = MAX(vweights[i],1);
    for (i=0;i<xadj[n];i++)
	++adjacency[i];
    interface(n,xadj,adjacency,vweights, feweights,0,0,0,0,0,sassignvec,1,0,mesh,0,1,1,1,coarse, ndims, 1e-3,rand());
    for (i=n;i>0;i--)
	assignvec[i-1]=(int)sassignvec[i-1];
    nverts=0;
    return 0;
}

// Prepare a copy of the graph in the right format for library calls
void library_assignment::constructGraph()
{
  MNGraph & g = wg.g;
  xadj = new int[nverts + 1];
  adjacency = new int[g.edgeCount()];
  vweights = new int[nverts];
  eweights = new int[g.edgeCount()];
  assignvec = new int[nverts];
  
  vertex v;
  edge e;
  int edgeNum = 0;
  int n;
  int currVert = 0;
  for(n=0;n<g.vertexCount();n++) {
    v=nodeAt[n];
    if (indexOfNode[v] == -1)
      continue;
    xadj[currVert] = edgeNum;
    vweights[currVert] = (int) wg.scaledWeight(v);
    forall_out_edges(e,v,g) {
      double w = wg.scaledWeight(e) + wg.scaledWeight(wg.mirror(e));
      if (w < 1.0)
	continue;
      adjacency[edgeNum] = indexOfNode[g.target(e)];
      eweights[edgeNum] = (int) (w /2 + 0.500000001);
      ++edgeNum;
    }
    currVert++;
  }
  xadj[nverts] = edgeNum;
}

// Transform the graph, turning edges into vertices and so on, 
// and remove unused edges.
void  library_assignment::constructTransformGraph()
{
  MNGraph & g = wg.g;
  edge e,e1;
  vertex vs, vt;
  int eNum = 0;
  int connectNum = 0;
  for (eNum=0;eNum<g.edgeCount();eNum++) {
    e=edgeAt[eNum];
    if (indexOfEdge[e] == -1)
      continue;
    vt = g.target(e);
    forall_out_edges(e1,vt,g) {
      if (wg.flowAcross(e,e1)>0)
	connectNum++;
    }
  }
  xadj = new int[nverts + 1];
  xadj[nverts]=connectNum*2;
  assignvec = new int[nverts];
  vweights = new int[nverts];
  adjacency = new int[connectNum*2];
  eweights = new int[connectNum*2];
  connectNum = 0;
  int currEdge=0;
  eNum=0;
  while (currEdge<nverts) {
    e=edgeAt[eNum++];
    if (indexOfEdge[e] == -1)
      continue;
    xadj[currEdge] = connectNum;
    vweights[currEdge] = (int) wg.scaledWeight(e);
    vs = g.source (e);
    vt = g.target (e);
    forall_out_edges (e1, vt,g) {
      double w=wg.flowAcross(e,e1);
      if (!(w>0.0) || indexOfEdge[e1]== -1)
	continue;
      adjacency[connectNum] = indexOfEdge[e1];
      eweights[connectNum] = (int)(w * wg.edgeScale()/2+0.5);
      connectNum++;
    }
    forall_in_edges (e1, vs, g) {
      double w=wg.flowAcross(e1,e);
      if (!(w>0.0) || indexOfEdge[e1]== -1)
	continue;
      adjacency[connectNum] = indexOfEdge[e1];
      eweights[connectNum] =(int)(w * wg.edgeScale()/2+0.5);
      connectNum++;
    }
    currEdge++;
  }
  if (connectNum != xadj[nverts])
    {
      cerr << "!! Something fishy with transform graph !!" <<endl;
      cerr << xadj[nverts] << " != " << connectNum << endl;
    }
}

library_assignment::~library_assignment()
{
    if (nverts)
    {
	delete[] xadj;
	delete[] adjacency;
	delete[] eweights;
	delete[] vweights;
	delete[] assignvec;
    }
}

random_assignment::random_assignment (weighted_graph & toAssign, int numParts, int seed): assignment (toAssign, numParts)
{
    randSeed=seed;
}

int random_assignment::assign()
{
  edge e;
  srand(randSeed);
  forall_edges(e,wg.g)
    assigns[e]=rand()%parts;
  return true;
}

blob_assignment::blob_assignment(weighted_graph& toAssign, int numParts, int seed) : assignment (toAssign, numParts)
{
  randSeed=seed;
}


int blob_assignment::assign()
{
  MNGraph& g = wg.g;
  srand(randSeed);
  assigns.init(g,-1);
  edge_array<int> seq(g, 0);
  vector<edge> edges;
  set<edge> frontier;
  set<edge> nextFrontier;
  int part=0, r, pass;
  vector<double> blobs(parts);
  for (part=0;part<parts;part++)
    blobs[part]=0;
  double targetSize=0;
  edge e,e1;
  forall_edges(e,g) {
    targetSize+=wg.weight(e);
    edges.push_back(e);
  }
  targetSize/=parts;
  targetSize = targetSize == 0 ? 1.0 : targetSize;
  for (part=0;part<parts;part++) {
    r=random()%edges.size();
    e=edges[r];
    assigns[e]=part;
    frontier.insert(e);
    blobs[part]+=wg.weight(e);
    edges.erase(edges.begin()+r);
  }
  edges.clear();
  bool changed=true;
  pass=0;
  int assigned=0;
  while (changed) {
    pass++;
    changed=false;
    nextFrontier.clear();
    forall(e,frontier) {
      part=assigns[e];
      for (int end=0;end<2;end++) {
	bool hasForeignNeighbor=false;
	forall_out_edges(e1, end ? g.target(e) : g.source(e), g) {
	  if (assigns[e1]==part)
	    continue;
	  if (assigns[e1]==-1 || (blobs[assigns[e1]] > targetSize && 
				  (blobs[part] + wg.weight(e1) <= targetSize || 
				   blobs[assigns[e1]] - wg.weight(e1) >= blobs[part]+wg.weight(e1))))
	    {
	      if (assigns[e1]==-1)
		assigned++;
	      else
		blobs[assigns[e1]]-=wg.weight(e1);
	      blobs[part]+=wg.weight(e1);
	      assigns[e1]=part;
	      nextFrontier.insert(e1);
	      changed=true;
	    }
	  else if (blobs[part]<targetSize)
	    hasForeignNeighbor=true;
	}
	if (hasForeignNeighbor)
	  nextFrontier.insert(e);
      }
    }
    frontier=nextFrontier;
    double minp=blobs[0], maxp=blobs[0];
    for (part=0;part<parts;part++) {
      minp=MIN(minp,blobs[part]);
      maxp=MAX(maxp,blobs[part]);
    }
    if (assigned == g.edgeCount() && (maxp-minp)/(targetSize) < 0.03 * targetSize)
      break;
  }
  return true;
}

existing_assignment::existing_assignment(weighted_graph& toAssign, int numParts) : assignment(toAssign, numParts)
{
}

//Use the assignment embedded in the graph
int existing_assignment::assign()
{
  edge e;
  map<string, int> coreNames;
  int coreNum=0;
  forall_edges(e, wg.g) {
    if (!wg.g[e].hasString("emul")) {
      cerr<<"All edges must have an assignment already to use \"existing\"."<<endl;
      cerr<<"Edge "<<e.index<<" ("<<wg.g.source(e).index<<"->"<<wg.g.target(e).index<<") has none."<<endl;
      exit(1);
    }
    string asg=wg.g[e].getString("emul");
    if (coreNames.count(asg))
      coreNames[asg]=coreNum++;
    assigns[e]=coreNames[asg];
    if (assigns[e] > parts-1) {
      cerr<<"This graph was assigned to more cores than given as input."<<endl;
      exit(1);
    }
  }
  return true;
}
