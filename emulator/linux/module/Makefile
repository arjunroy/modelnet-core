TARGET = linuxmodelnet
obj-m += $(TARGET).o
MODELNET_SOURCES := mn_pathtable.o ip_modelnet.o mn_remote.o mn_tcpdump.o
$(TARGET)-objs := $(MODELNET_SOURCES)
MODELNET_MODULE = $(TARGET).ko
#EXTRA_CFLAGS += -DMN_TCPDUMP

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
MODELNET_PREFIX ?= /opt/modelnet
INSTALL_DIR = $(MODELNET_PREFIX)/lib

compile:
	make -C $(KERNEL_DIR) M=$(PWD) modules

install: compile
	if [ -d $(INSTALL_DIR) ]; then \
          cp $(MODELNET_MODULE) $(INSTALL_DIR); \
        else \
          mkdir -p $(INSTALL_DIR) && cp $(MODELNET_MODULE) $(INSTALL_DIR); \
        fi

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
