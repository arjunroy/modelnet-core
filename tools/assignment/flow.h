/* Ethan Eade, 8/30
 * This header declares the objects necessary for description
 * of flow patterns.
 */
#ifndef __FLOW_H_
#define __FLOW_H_

#include <stdlib.h>
#include "mngraph.h"
#include "util.h"
#include <iostream>

// This encapsulates a point-to-point flow.
struct flow
{
  flow(){
    source=target=vertex::nil;
    bandwidth=sendRate=0;
    stepFactor=1.0;
  }
  vertex source;
  vertex target;
  float bandwidth;
  float sendRate;
  float stepFactor;
};

// This is the base class for all flow patterns.
// A flow pattern is responsible for constructing client-client
// flows given a topology and a list of client nodes.
class flow_pattern
{
public:
    virtual ~flow_pattern(){}
    virtual void constructFlows(MNGraph& g, list<vertex>& clients, list<flow*>& flows)=0;
};

// This flow pattern creates exchanges between random pairs of clients.  
// Each client sends to one other client. Some may receive from more than one.
class random_peer_pattern : public flow_pattern
{
public:
    void constructFlows(MNGraph& g, list<vertex>& clients, list<flow*>& flows);
};

// A typed_flow is a template for flows between certain types of clients.
struct typed_flow
{
    typed_flow(){
	mult_low=mult_high=1;
	bw_pct_low=bw_pct_high=1;
    }
    string target_type;
    double mult_low;
    double mult_high;
    double bw_pct_low;
    double bw_pct_high;
};

//client_type describes a class of clients.
struct client_type
{
    client_type(){
	probability=1;
	link_low=link_high=1;
    }
    //Name of this class of clients.
    string name;

    //Probability that a given client falls in this class (1.0 == 100%)
    double probability;

    //List of typed_flow's for this class
    list<typed_flow> flows;
    double link_low;
    double link_high;
};


// A user flow pattern consists of client classes and flows for those classes.
// See example .pattern files.
class user_flow_pattern : public flow_pattern
{
 public:
  void constructFlows(MNGraph& g, list<vertex>& clients, list<flow*>& flows);
  int read_pattern(istream& in);
 protected:
  int read_flow(istream& in, typed_flow& tf);
  int read_class(istream& in, client_type* ct);
  client_type* chooseType();
  list<client_type*> types;
};

#endif
