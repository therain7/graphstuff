JOBS ?= $(shell nproc)
CFLAGS := -Wall -Wextra -Werror -O2 $(CFLAGS)
BLD := build

GRB_ROOT := vendor/GraphBLAS
GRB_DIST := $(GRB_ROOT)/dist

LGR_ROOT := vendor/LAGraph
LGR_DIST := $(LGR_ROOT)/dist

BINS := prim_lagr

.PHONY: build vendor
build: $(addprefix $(BLD)/, $(BINS))
vendor: $(GRB_DIST) $(LGR_DIST)

$(GRB_DIST):
	$(MAKE) -C $(GRB_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(GRB_ROOT) install

$(LGR_DIST): $(GRB_DIST)
	$(MAKE) -C $(LGR_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(LGR_ROOT) install

$(BLD)/prim_lagr: prim/prim_lagr.c $(GRB_DIST) $(LGR_DIST)
	$(CC) $(CFLAGS) $< -o $@ -lomp \
		-I$(GRB_DIST)/include/suitesparse -L$(GRB_DIST)/lib64 -lgraphblas \
		-Wl,-rpath=$(realpath $(GRB_DIST)/lib64) \
		-I$(LGR_DIST)/include/suitesparse -L$(LGR_DIST)/lib64 -l:liblagraph.a
