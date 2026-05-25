
STDC_DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

XCC_FLAGS += -I $(STDC_DIR)
