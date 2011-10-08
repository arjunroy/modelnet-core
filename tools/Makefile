CFLAGS	= -g -O2 -Wall
INCDIR	= -I. -I../module/ -I../netlog -L./
XMLLIB = -lxerces-c
GRAPH = libmngraph.a
GRAPHLIB = -lmngraph $(XMLLIB)
PROG = hiergen gentargets dissect perfstats buildflows reassign makescript assigndiff assign dynamic blaster server ipgatherer makedynamic traffic avgstats flowgen mirror routes

all: $(PROG)

install:
	cp $(PROG) ../bin/

clean:
	rm -f $(PROG) *.o $(GRAPH)
	make -C assignment clean

logreader.o: logreader.cpp logreader.h
	g++ $(CFLAGS) $(INCDIR) -c logreader.cpp

graph.o: graph.h graph.cpp
	g++ $(CFLAGS) -c graph.cpp

xml.o: xml.h xml.cpp
	g++ -Wall -g -O2 -c xml.cpp

libmngraph.a: mngraph.cpp mngraph.h graph.h graph.o
	g++ -Wall -O2 -g  -c -o mngraph.o mngraph.cpp
	ar rvs libmngraph.a mngraph.o graph.o

ipgatherer: ipgatherer.c
	gcc $(CFLAGS) $(INCDIR) ipgatherer.c -o ipgatherer

hiergen: $(GRAPH) hiergen.cpp
	g++ $(CFLAGS) $(INCDIR) -o hiergen hiergen.cpp $(GRAPHLIB)

flowgen: $(GRAPH) flowgen.cpp assignment/flow.cpp
	g++ $(CFLAGS) $(INCDIR) -o flowgen flowgen.cpp assignment/flow.cpp $(GRAPHLIB)

gentargets: $(GRAPH) gentargets.cpp
	g++ $(CFLAGS) $(INCDIR) -o gentargets gentargets.cpp $(GRAPHLIB)

mirror: $(GRAPH) mirror.cpp
	g++ $(CFLAGS) $(INCDIR) -o mirror mirror.cpp $(GRAPHLIB)

routes: $(GRAPH) routes.cpp
	g++ $(CFLAGS) $(INCDIR) -o routes routes.cpp $(GRAPHLIB)

dissect: logreader.o dissect.cpp
	g++ $(CFLAGS) $(INCDIR) -o dissect dissect.cpp logreader.o

perfstats: logreader.o perfstats.cpp
	g++ $(CFLAGS) $(INCDIR) -o perfstats perfstats.cpp logreader.o

buildflows: logreader.o buildflows.cpp xml.o
	g++ $(CFLAGS) $(INCDIR) -o buildflows buildflows.cpp xml.o logreader.o $(XMLLIB)

reassign: reassign.cpp xml.o
	g++ $(CFLAGS) $(INCDIR) -o reassign reassign.cpp xml.o $(XMLLIB)

makescript: makescript.cpp xml.o
	g++ $(CFLAGS) -o makescript makescript.cpp xml.o $(XMLLIB)

makedynamic: makedynamic.cpp xml.o
	g++ $(CFLAGS) -o makedynamic makedynamic.cpp xml.o $(XMLLIB)

assigndiff: assigndiff.cpp $(GRAPH)
	g++ $(CFLAGS) $(INCDIR) -o assigndiff assigndiff.cpp $(GRAPHLIB)

avgstats: avgstats.cpp
	g++ $(CFLAGS) -o avgstats avgstats.cpp 

assign: $(GRAPH) xml.o assignment/assign
	make -C assignment
	cp assignment/assign .

dynamic: dynamic.sh
	cp dynamic.sh dynamic
	chmod 755 dynamic

blaster: blaster.cpp
	g++ $(CFLAGS) blaster.cpp -o blaster

traffic: traffic.cpp
	g++ $(CFLAGS) traffic.cpp -o traffic

server: server.cpp
	g++ $(CFLAGS) server.cpp -o server

