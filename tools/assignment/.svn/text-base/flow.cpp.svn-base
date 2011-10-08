/* Ethan Eade, 8/30/02
 * Implementation of flows.  See flow.h
 */

#include "flow.h"

// A utility function to eat the rest of a line in an input stream.
void eatline(istream& in)
{
    char c=0;
    while (c!='\n' && in.read(&c,1));
}

// For each client, pick a random peer and create a flow to it.
// Flows aim for the minimum bandwidth of the path.
void random_peer_pattern::constructFlows(MNGraph& g, list<vertex>& clients, list<flow*>& flows)
{
    edge e;
    vertex v,target;
    flow* f;
    vector<vertex> clientArray(clients.size());
    int i=0;
    forall(v,clients)
	clientArray[i++]=v;
    forall(v,clients) {
	target=v;
	do { target=clientArray[rand()%clients.size()];} while (target==v);
	for (i=0;i<1;i++) {
	  f=new flow();
	  f->source=v;
	  f->target=target;
	  forall_out_edges(e,v,g)
	    f->bandwidth=(int)g.specsFor(e).getDouble("kbps");
	  f->sendRate=0;
	  f->stepFactor=1.0;
	  flows.push_back(f);
	}
    }
}
//Parse a client class
int user_flow_pattern::read_class(istream& in, client_type* ct)
{
  *ct=client_type();
  typed_flow tf;
    
  string s,line;
  if (!(in>>s))
    return false;
  ct->name=s;
  while (in>>s) {
    if (s[0]=='#') {
      eatline(in);
      continue;
    }
    if (s=="end")
      return true;
    if (s[s.length()-1]==':')
      s=s.substr(0,s.length()-1);
    if (s=="weight") {
      if (!(in>>ct->probability))
	return false;
    }
    else if (s=="link") {
      if (!(in>>ct->link_low))
	return false;
	ct->link_high=ct->link_low;
    }
    else if (s=="link_low") {
      if (!(in>>ct->link_low))
	return false;
    }
    else if (s=="link_high") {
      if (!(in>>ct->link_high))
	return false;
    }
    else if (s=="flow") {
      if (!read_flow(in, tf))
	return false;
      ct->flows.push_back(tf);
    }
  }
  return false;
}

//Parse a flow declaration
int user_flow_pattern::read_flow(istream& in, typed_flow& tf)
{
    tf=typed_flow();
    string s;
    while (in>>s) {
      if (s[0]=='#') {
	eatline(in);
	continue;
      }
      if (s=="end")
	return true;
      if (s[s.length()-1]==':')
	s=s.substr(0,s.length()-1);
      if (s=="to") {
	if (!(in>>tf.target_type))
	  return false;
      }
      else if (s=="number") {
	if (!(in>>tf.mult_low))
	  return false;
	tf.mult_high=tf.mult_low;
      }
      else if (s=="number_low")	{
	if (!(in>>tf.mult_low))
	  return false;
      }
      else if (s=="number_high") {
	if (!(in>>tf.mult_high))
	  return false;
      }
      else if (s=="percent") {
	if (!(in>>tf.bw_pct_low))
	  return false;
	tf.bw_pct_high=tf.bw_pct_low;
      }
      else if (s=="percent_low") {
	if (!(in>>tf.bw_pct_low))
	  return false;	    
      }
      else if (s=="percent_high") {
	if (!(in>>tf.bw_pct_high))
	  return false;	    
      }
    }
    return false;
}

//Parse a flow pattern description
int user_flow_pattern::read_pattern(istream& in)
{
    client_type* ct;
    typed_flow tf;
    string s;
    while (in>>s) {
      if (s[0]=='#') {
	eatline(in);
	continue;
      }
      if (s == "class")	{
	ct=new client_type();
	if (read_class(in,ct))
	  types.push_back(ct);
	else {
	  delete ct;
	  cerr<<"Parse error reading class "<<ct->name<<"."<<endl;
	  return false;
	}
      }
      else {
	cerr<<"Parse error: "<<s<<" invalid keyword."<<endl;
	return false;
      }
    }
    if (types.size() == 0) {
      cerr << "Error: Zero classes specified." <<endl;
      return 0;
    }
    set<string> typeNames;
    forall(ct,types)
      typeNames.insert(ct->name);
    set<string> invalidTargets;
    forall(ct, types) {
      forall(tf, ct->flows) {
	if ( !typeNames.count(tf.target_type))
	  invalidTargets.insert(tf.target_type);
      }
    }
    if (invalidTargets.size()) {
      cerr<<"Parse error:  "<<invalidTargets.size()<<"invalid flow targets."<<endl;
      forall (s, invalidTargets)
	cerr<<"\t"<<s<<endl;
      return false;
    }
    double totalWeight=0;
    forall(ct, types)
      totalWeight+=ct->probability;
    forall(ct,types)
      ct->probability=ct->probability/totalWeight;
    return true;
}

//Select a type based on their relative probabilities
client_type* user_flow_pattern::chooseType()
{
  double p=(double(rand())/RAND_MAX);
  double sum=0;
  client_type* ct;
  forall(ct, types) {
    sum+=ct->probability;
    if (p<=sum)
      return ct;
  }
  return ct;
}

//Utility to select a random value in a range.
double chooseVal(double low, double high)
{
  double r=double(rand())/RAND_MAX;
  return low + r*(high-low);
}

// Create flows as specified by this pattern.  The "flows" argument
// is an output parameter.
void user_flow_pattern::constructFlows(MNGraph& g, list<vertex>& clients, list<flow*>& flows)
{
  edge e;
  vertex v;
  typed_flow tf;
  client_type* ct;
  map<string,vector<vertex> > group;
  vertex_array<client_type*> typeOf(g);
  vertex_array<double> sendRate(g);
  cerr<<"Designing flows..."<<endl;
  forall(v,clients) {
    client_type* ct=chooseType();
    g[v].setString("class",ct->name);
    typeOf[v]=ct;
    double bw=0;
    forall_out_edges(e,v,g) {
      bw=g[e].getInteger("bw");
      if (bw > 0)
	break;
    }
    double val = chooseVal(ct->link_low,ct->link_high)*bw;
    sendRate[v] = MIN(val, bw);
    group[ct->name].push_back(v);
  }
  forall(v,clients) {
    ct=typeOf[v];
    forall(tf, ct->flows) {
      flow* f;
      int i;
      int mult=(int)(chooseVal(tf.mult_low,tf.mult_high) + 0.5);
      for (i=0;i<mult;i++) {
	f=new flow();
	f->source=v;
	vector<vertex>& nodes=group[tf.target_type];
	f->target=v;
	while (f->target==v)
	  f->target=nodes[rand() % nodes.size()];
	f->bandwidth=chooseVal(tf.bw_pct_low,tf.bw_pct_high)/100.0 * sendRate[v];
	flows.push_back(f);
      }
    }
  }
}
