TWI_DIR = $(MAT91LIB_DIR)/twi

VPATH += $(TWI_DIR)
INCLUDES += -I$(TWI_DIR)

SRC += twi.c

