PACER_DIR = $(DRIVER_DIR)/pacer

VPATH += $(PACER_DIR)
SRC += pacer.c

PERIPHERALS += pit

INCLUDES += -I$(PACER_DIR)

