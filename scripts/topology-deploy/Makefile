MODELNET_PREFIX ?= /opt/modelnet
INSTALL_DIR = $(MODELNET_PREFIX)/bin

COMMON_SCRIPTS = deploy deployhost hostrun vnrun vnrunhost
COMMON_DIR = common/
CSCRIPTS = $(addprefix $(COMMON_DIR), $(COMMON_SCRIPTS))

EMULATOR_SCRIPTS = modelload modelstat
EMULATOR_DIR = emulator/
ESCRIPTS = $(addprefix $(EMULATOR_DIR), $(EMULATOR_SCRIPTS))

install: client emulatorcore

emulatorcore: client
	for script in $(ESCRIPTS); \
        do \
            if [ -d $(INSTALL_DIR) ]; then \
              cp $$script $(INSTALL_DIR); \
            else \
              mkdir -p $(INSTALL_DIR) && cp $$script $(INSTALL_DIR); \
            fi;\
        done;

client:
	for script in $(CSCRIPTS); \
        do \
            if [ -d $(INSTALL_DIR) ]; then \
              cp $$script $(INSTALL_DIR); \
            else \
              mkdir -p $(INSTALL_DIR) && cp $$script $(INSTALL_DIR); \
            fi;\
        done;

