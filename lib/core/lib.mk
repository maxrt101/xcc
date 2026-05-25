
CORE_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

XCC_FLAGS += -I $(CORE_DIR)
XCC_SRC += $(CORE_DIR)/alloc.xc
