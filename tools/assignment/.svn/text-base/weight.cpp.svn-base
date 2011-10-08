/* Ethan Eade, 8/30/02
 * This is the implementation of weighted graphs described
 * in weight.h
 */
#include "weight.h"
#include <deque>
#include <fstream>
using namespace std;

weighted_graph::weighted_graph (MNGraph & G):g (G), flowMap(G),
					     eweights (G, 0),
					     vweights (G,0), mirrors(G,edge::nil)
{
  maxVWeight=maxEWeight=0;
  vscale=escale=1.0;
  calculateMirrors();
  findNodes(g,"virtnode", myClients);
}

void weighted_graph::scaleWeights()
{
  edge e;
  vertex v;
  maxEWeight=maxVWeight=0;
  double minEWeight=100000;
  //double minVWeight=100000;
  double sumE=0, sumV=0;
  forall_edges(e, g)
    {
      sumE+=eweights[e];
      minEWeight=MIN(minEWeight, eweights[e]);
      maxEWeight=MAX(maxEWeight, eweights[e]);
    }
  forall_vertices (v, g)
    {
      sumV+=vweights[v];
      maxVWeight=MAX(maxVWeight,vweights[v]);
    }
  if (sumV==0 || sumE==0)
    {
      cerr<<"No weights."<<endl;
      return;
    }
    
  vscale=2000000000.0/sumV;
  escale=2000000000.0/sumE;
}

void weighted_graph::calculateMirrors()
{
  edge e,e1;
  vertex vs,vt;
  forall_edges(e,g)    {
    if (mirrors[e]!=edge::nil)
      continue;
    vs=g.source(e);
    vt=g.target(e);
    forall_out_edges(e1,vt,g) {
      if (g.target(e1)==vs) {
	mirrors[e]=e1;
	mirrors[e1]=e;
	break;
      }
    }
  }
}

edge weighted_graph::mirror(edge e)
{
  return mirrors[e];
}
double weighted_graph::scaledWeight(edge e)
{
  return eweights[e]*escale;
}

double weighted_graph::scaledWeight(vertex v)
{
  return vweights[v]*vscale;
}

double weighted_graph::weight(edge e)
{
  return eweights[e];
}

double weighted_graph::weight(vertex v)
{
  return vweights[v];
}

double weighted_graph::edgeScale()
{
  return escale;
}

double weighted_graph::nodeScale()
{
  return vscale;
}

double weighted_graph::flowAcross(edge s, edge t)
{
  if (flowMap[s].count(t))
    return flowMap[s][t];
  return 0.0;
}

int weighted_graph::flowsFrom(edge e)
{
  return flowMap[e].size();
}
void weighted_graph::buildFlowLists(list<vertex>& clients, list<flow*>& flows, edge_array<list<pipeUser> >& users)
{
  vertex v,v1,v2;
  edge e;
  vertex_array<set<vertex> > targets(g);
  vertex_array<list<flow*> > flowsFor(g);
  vertex_array<edge> pred(g, edge::nil);
  vertex_array<int> visited(g);
  edge_array<int> bandwidth(g);
  forall_edges(e,g)
    {
      bandwidth[e]=(int)g.specsFor(e).getDouble("kbps");
    }
  deque<vertex> q;
  flow* f;
  forall(f, flows)
    {
      targets[f->source].insert(f->target);
      flowsFor[f->source].push_back(f);
    }
  for(list<vertex>::iterator it=clients.begin(); it!=clients.end(); it++) {
    v=*it;
    visited.init(g,0);
    q.clear();
    unsigned int discovered=0;
    q.push_back(v);
    visited[v]=1;
    while (discovered < targets[v].size()) {
      v1=q.front();
      q.pop_front();
      forall_out_edges(e,v1,g) {
	v2=g.target(e);
	if (visited[v2])
	  continue;
	pred[v2]=e;
	if (targets[v].count(v2))
	  discovered++;
	visited[v2]=1;
	q.push_back(v2);
      }
      forall(f, flowsFor[v]) {
	v1=f->target;
	double minBW = -1;
	edge lastEdge=edge::nil;
	while (v1!=v) {
	  pipeUser pu;
	  pu.f=f;
	  pu.next=lastEdge;
	  lastEdge=pred[v1];
	  users[pred[v1]].push_back(pu);
	  minBW = minBW==-1 ? bandwidth[pred[v1]] : MIN(bandwidth[pred[v1]],minBW);
	  v1=g.source(pred[v1]);
	}
	f->bandwidth=MIN(minBW, f->bandwidth);
      }
    }
  }
}

void weighted_graph::weight_by_pattern(flow_pattern* pattern)
{
  vertex v;
  edge e;
  flow* f;
  pipeUser pu;
  list<flow*> flows;
  edge_array<list<pipeUser> > users(g);
  edge_array<double> room(g);
  vertex_array<double> sendRate(g,0);
  vertex_array<list<flow*> > flowsFor(g);
  list<edge> activeEdges;
    
  pattern->constructFlows(g,myClients, flows);
  buildFlowLists(myClients, flows, users);
  forall (f, flows)
    flowsFor[f->source].push_back(f);
  forall_edges(e, g)
    {
      g[e].setInteger("stress", users[e].size());
      if (users[e].size())
	{
	  activeEdges.push_back(e);
	  room[e]=g.specsFor(e).getDouble("kbps");
	}
    }
  forall(v,myClients) {
    forall_out_edges(e,v,g)
      {
	if (room[e])
	  {
	    sendRate[v]=room[e];
	    break;
	  }
      }
  }
  double step=0;
  int steps=0;
  unsigned int fullClients=0;
  int numFilledPipes=0;
  while (step<1.0)
    {
      double nextStep;
      double minClientStep, minPipeStep;
      list<vertex> filledClients;
      list<edge> filledPipes;
      minClientStep=minPipeStep=10000;
      forall(v, myClients)
	{
	  double req=0;
	  double curr=0;
	  forall(f, flowsFor[v])
	    {
	      req+=f->stepFactor*f->bandwidth;
	      curr+=f->sendRate;
	    }
	  if (req==0)
	    continue;
	  double roomLeft=sendRate[v]-curr;
	  if (roomLeft / req < minClientStep)
	    {
	      minClientStep = roomLeft / req;
	      filledClients.clear();
	      filledClients.push_back(v);
	    }
	  else if (roomLeft / req == minClientStep)
	    {
	      filledClients.push_back(v);
	    }
	}
      forall(e, activeEdges)
	{
	  double flowStep=0;
	  forall(pu, users[e])
	    {
	      flowStep += pu.f->stepFactor * pu.f->bandwidth;
	    }
	  if (flowStep == 0)
	    continue;
	  if (room[e] / flowStep < minPipeStep)
	    {
	      minPipeStep = room[e] / flowStep;
	      filledPipes.clear();
	      filledPipes.push_back(e);
	    }
	  else if (room[e] / flowStep == minPipeStep)
	    {
	      filledPipes.push_back(e);
	    }
	}
      if (filledPipes.size() == 0 || fullClients==myClients.size() || activeEdges.size() == 0)
	break;
      nextStep=MIN(1.0-step,MIN(minClientStep, minPipeStep));
      forall(f,flows)
	{
	  f->sendRate+=f->bandwidth*f->stepFactor*nextStep;
	}
      if (minClientStep < minPipeStep)
	{
	  forall(v,filledClients)
	    {
	      forall(f, flowsFor[v])
		{
		  f->stepFactor=0;
		}
	      fullClients++;
	    }
	}
      else
	{
	  numFilledPipes+=filledPipes.size();
	  forall(e,filledPipes)
	    {
	      forall(pu,users[e])
		{
		  pu.f->stepFactor=0;
		}
	      activeEdges.remove(e);
	    }
	}
      step+=nextStep;
      steps++;
    }
  forall_edges(e,g)
    {
      double totalFlow=0;
      forall(pu,users[e])
	{
	  totalFlow+=pu.f->sendRate;
	  if (pu.next.isNil())
	    continue;
	  if (!flowMap[e].count(pu.next))
	    flowMap[e][pu.next]=0;
	  flowMap[e][pu.next]+=pu.f->sendRate;
	}
      eweights[e]=totalFlow;
      vweights[g.target(e)]+=eweights[e];
    }
  double totalSend=0;
  forall (f, flows)
    {
      totalSend+=f->sendRate;
      delete f;
    }
}

double weighted_graph::getStress(edge_array<int>& assignment)
{
  edge e;
  int a;
  double stress = 0;
  double total = 0;
  forall_edges(e,g)
    {
      a=assignment[e];
      pair<edge, double> dummy;
      pair<edge, double>& f=dummy;
      forall(f,flowMap[e])
	{
	  if (assignment[f.first] != a)
	    {
	      stress += f.second;
	    }
	  total += f.second;
	}
    }
  return stress;
}

void weighted_graph::weight_worst_case()
{
  vertex v;
  edge e;
  eweights.init(g,0);
  vweights.init(g,0);
  vertex_array<int> dist(g);
  vertex_array<edge> pred(g);
  edge_array<int> cost(g, 1);
  edge_array<double> bandwidth(g);

  list<vertex>& clients = myClients;

  forall_edges(e, g)
    bandwidth[e]=g.specsFor(e).getDouble("kbps");

  double totalSend = 0;
  forall(v, clients) {
    forall_out_edges(e,v,g) {
      if (bandwidth[e] > 0) {
	totalSend+=bandwidth[e];
	break;
      }
    }
  }
  cerr << "Total entry bandwidth: " << totalSend << endl;

  vertex source, target;
  int clientNum=0;
  forall(source,clients) {
    if (clientNum % 10 == 0) {
      cerr << "\rUsing virtual node " << clientNum << " as source...";
      cerr.flush();
    }
    clientNum++;

    breadth_first_search<MNGraph>(g, pred, dist, source);
    edge_array<double> partialFlow(g, 0);
    edge_array<double> minBandwidthUpTo(g, 0);
    edge_array<int> visited(g,false);
    forall(target,clients) {
      if (pred[target].isNil())
	continue;
      v = target;
      double flow;
      double minBandwidth=bandwidth[pred[v]];
      deque<edge> path;
      e = pred[v];
      while (!e.isNil() && (!visited[e] || partialFlow[e] < MIN(minBandwidth, minBandwidthUpTo[e]))) {
	minBandwidth = MIN(minBandwidth, bandwidth[e]);
	path.push_front(e);
	v = g.source(e);
	e = pred[v];
      }
      double minSoFar;
      if (v == source) {
	flow = minBandwidth;
	minSoFar = bandwidth[path.front()];
      }
      else {
	flow = MIN(minBandwidth, minBandwidthUpTo[e]);
	minSoFar = minBandwidthUpTo[e];
      }
      forall(e,path) {
	minBandwidthUpTo[e]=minSoFar=MIN(minSoFar, bandwidth[e]);
	visited[e] = true;
	partialFlow[e]=MAX(partialFlow[e], flow);
      }
    }
    forall(target, clients) {
      if (pred[v].isNil())
	continue;
      v = target;
      e = pred[v];
      while (!e.isNil() && partialFlow[e]!=0) {
	v=g.source(e);
	if (v != source) {
	  edge into_e = pred[v];
	  if (!flowMap[into_e].count(e))
	    flowMap[into_e][e]=0;
	  double room = MAX(0,bandwidth[e]-flowMap[into_e][e]);
	  flowMap[into_e][e]+=MIN(room,partialFlow[e]);
	  eweights[e]+=MIN(room,partialFlow[e]);
	}
	else
	  eweights[e]=partialFlow[e];
	partialFlow[e]=minBandwidthUpTo[e]=0;
	e = pred[v];
      }   	
    }
  }
  forall_edges(e,g) {
    vweights[g.target(e)]+=eweights[e];
  }
  cerr << endl;
}

void weighted_graph::weight_ones()
{
  edge e;
  vertex v;
  forall_edges(e,g) {
    eweights[e]=1;
  }
  forall_vertices(v,g) {
    vweights[v]=1;
  }
  maxVWeight=maxEWeight=1.0;
}

void weighted_graph::weight_by_flowmap(const char* filename)
{
  ifstream in(filename);
  if (!in) {
    cerr<<"Could not open flowmap file:"<<filename<<endl;
    exit(1);
  }
  edge e;
  int from, to, numFlows, flow;
  while (in>>from>>numFlows) {
    while (numFlows-- && (in>>to>>flow)) {
      if (from)
	flowMap[edge(from)][edge(to)]=flow;
      eweights[edge(to)]+=flow;
      vweights[g.target(edge(to))]+=flow;
    }
  }
  forall_edges(e,g) {
    maxEWeight=MAX(maxEWeight, eweights[e]);
    maxVWeight=MAX(maxVWeight, vweights[g.target(e)]);
  }
  in.close();
}
