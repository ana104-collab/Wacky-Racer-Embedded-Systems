SPI_DIR = $(MAT91LIB_DIR)/spi

VPATH += $(SPI_DIR)

SRC += spi.c

INCLUDES += -I$(SPI_DIR)
