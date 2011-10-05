# Makes allpairs

CC = g++
TARGET = allpairs

ALLPAIRS_HEADERS = mngraph.h
ALLPAIRS_SOURCES = allpairs.cc io.cc mngraph.cc
ALLPAIRS_LDADD = -lxerces-c
ALLPAIRS_LDFLAGS = -pthread

MODELNET_PREFIX ?= /opt/modelnet
INSTALL_DIR = $(MODELNET_PREFIX)/bin

allpairs : $(TARGET)

$(TARGET): $(ALLPAIRS_SOURCES) $(ALLPAIRS_HEADERS)
	$(CC) -o $(TARGET) $(ALLPAIRS_SOURCES) $(ALLPAIRS_LDADD) $(ALLPAIRS_LDFLAGS)

install: $(TARGET)
	if [ -d $(INSTALL_DIR) ]; then \
          cp $(TARGET) $(INSTALL_DIR); \
        else \
          mkdir -p $(INSTALL_DIR) && cp $(TARGET) $(INSTALL_DIR); \
        fi

clean:
	rm -f $(TARGET)

