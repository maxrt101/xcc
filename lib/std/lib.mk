
STD_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

XCC_FLAGS += -I $(STD_DIR)
XCC_SRC += $(STD_DIR)/str.xc
