obj-m += linuxmodelnet.o
linuxmodelnet-objs := mn_pathtable.o ip_modelnet.o mn_remote.o mn_tcpdump.o
#EXTRA_CFLAGS += -DMN_TCPDUMP

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
