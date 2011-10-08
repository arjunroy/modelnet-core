///////////////////////
// This is the regress test script for linux version of modelnet
// Developed by Xuanran Zong (zxr88826@gmail.com) with the help from Kenneth Yocum
//////////////////////


HOW TO RUN THE SCRIPT
--------------

1. Set up dependent software properly, such as gexec, etc.

2. Set up GEXEC_SVRS to appropriate value

3. Run ./reg_test.pl

4. You can view your comparison diagram for second test by checking
mncapacity/compare.png


Notes: If you want to run the ordering test, modify the Makefile for linux module by uncomment CFLAG with DORDER_TEST and recompile the code.

If you want to restore back to original version, just redo what you have done and recompile the module.


MACHINES CONFIGURATION
--------------

All these experiment is performed on three machines, two of them are nodes and
one of them is core.

ModelNet core machines has one Intel Xeon 2.7GHz CPU, 1GB RAM

One node has dual-core with Intel Xeon 2.7GHz CPU, 1GB RAM

The other node has one Intel Xeon 2.7GHz CPU, 1GB Ram

All of them are connected in a 1Gb LAN.


TEST SCRIPT INCLUDED
---------------


Bandwidth Test
----

Test if Modelnet behaives well under bursty traffic, where the sending rate is larger than pipe bandwidth. We performed this test by sending a network flow through a link with bandwidth K in a constant rate of 1.2*K. Thus, we can measure down how does ModelNet performs under saturated links.


Capacity Test
----

Test Modelnet throughput as number of nodes and number of hops increases. We
define the throuput as the total number of packets forwarded by the core in
certain time interval. The result of this experiment can show how is 
ModelNet core bounded by CPU rate.


Ordering Test
----

Test if Modelnet emulated packet flows follow exactly the same order. We send
a flow to ModelNet core and distinguish if the incoming packet order
is different from the outgoing packet order.

