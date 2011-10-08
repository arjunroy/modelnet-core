/* Ethan Eade, 8/30/02
 * This header defines a weighted_graph, which
 * encapsulates an mn_graph with weight edges and
 * nodes.
 */
#ifndef __WEIGHT_H_
#define __WEIGHT_H_

#include <stdlib.h>
#include <mngraph.h>
#include "util.h"
#include "flow.h"

// pipeUser is a linked list of all flows passing through a given pipe.
struct pipeUser
{
    flow* f;
    edge next;
};

class weighted_graph
{
public:
    weighted_graph(MNGraph& G);
    // Given a list of clients and a list of flows,
    // figure out which flows use which pipes.
    void buildFlowLists(list<vertex>& clients, list<flow*>& flows, edge_array<list<pipeUser> >& users);

    //Weight the graph using a flow pattern.  See flow.h
    void weight_by_pattern(flow_pattern* pattern);

    // Weight the graph using worst case TCP weighting.
    // In this case, all clients send to all other clients,
    // sending as much as possible, assuming all flows obey
    // TCP characteristics.
    void weight_worst_case();

    // Give all edges and vertices weight of 1.0
    void weight_ones();

    // Use a flowmap stored at the given path to weight the graph.
    // Flowmap files are constructed from modelnet packet logs.
    void weight_by_flowmap(const char* filename);

    // Scale edge and vertex weights to minimize quantization.
    void scaleWeights();

    //Return scaled weights
    double scaledWeight(edge e);
    double scaledWeight(vertex v);

    //Return unscaled weights
    double weight(edge e);
    double weight(vertex v);

    //Return the flow from one pipe to another
    double flowAcross(edge s, edge t);

    //Return the number of flows coming out of edge e
    int flowsFrom(edge e);

    //Return the scaling factors.
    double edgeScale();
    double nodeScale();

    //Calculate the total intercore flow
    double getStress(edge_array<int>& assignment);

    //Get the mirror edge of e
    edge mirror(edge e);

    //The underlying mn_graph
    MNGraph& g;
    const list<vertex>& getClients() const {return myClients;}

    //Maximum weights in the graph
    double maxVWeight;
    double maxEWeight;
protected:
    void calculateMirrors();
    edge_array<map<edge,double> > flowMap;
    edge_array<double> eweights;
    vertex_array<double> vweights;
    edge_array<edge> mirrors;
    list<vertex> myClients;
    double vscale,escale;
private:
};

#endif
