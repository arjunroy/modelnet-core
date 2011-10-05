# Makes allpairs

CC = g++
TARGET = allpairs

ALLPAIRS_HEADERS = mngraph.h
ALLPAIRS_SOURCES = allpairs.cc io.cc mngraph.cc
ALLPAIRS_LDADD = -lxerces-c
ALLPAIRS_LDFLAGS = -pthread

allpairs: $(ALLPAIRS_SOURCES) $(ALLPAIRS_HEADERS)
	$(CC) -o $(TARGET) $(ALLPAIRS_SOURCES) $(ALLPAIRS_LDADD) $(ALLPAIRS_LDFLAGS)

clean:
	rm -f $(TARGET)

