PIT_DIR = $(MAT91LIB_DIR)/$(FAMILY)/pit

VPATH += $(PIT_DIR)
INCLUDES += -I$(PIT_DIR)

SRC += pit.c

