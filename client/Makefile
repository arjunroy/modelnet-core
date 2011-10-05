# Makes libipaddr.so

TARGET = libipaddr
SO_FILE = $(TARGET).so

LIBIPADDR_SOURCES = libipaddr.c
LIBIPADDR_OBJECTS = $(LIBIPADDR_SOURCES:%.c=%.o)
LIBIPADDR_CFLAGS = -fPIC -DPIC -g
LIBIPADDR_LDFLAGS = -shared -ldl

CC = gcc
MODELNET_PREFIX ?= /opt/modelnet
INSTALL_DIR = $(MODELNET_PREFIX)/lib

compile : $(SO_FILE)

install: $(SO_FILE)
	if [ -d $(INSTALL_DIR) ]; then \
	  cp $(SO_FILE) $(INSTALL_DIR); \
	else \
	  mkdir -p $(INSTALL_DIR) && cp $(SO_FILE) $(INSTALL_DIR); \
	fi

$(LIBIPADDR_OBJECTS): %.o: %.c
	$(CC) -c $(LIBIPADDR_CFLAGS) $< -o $@

$(SO_FILE): $(LIBIPADDR_OBJECTS)
	$(CC) $(LIBIPADDR_LDFLAGS) $(LIBIPADDR_OBJECTS) -o $(SO_FILE)


clean:
	rm -f $(SO_FILE) $(LIBIPADDR_OBJECTS)

