PWM_DIR = $(MAT91LIB_DIR)/pwm

VPATH += $(PWM_DIR)
INCLUDES += -I$(PWM_DIR)

SRC += pwm.c

