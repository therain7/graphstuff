JOBS ?= $(shell nproc)
CFLAGS := -Wall -Wextra -Werror -O2 -I. $(CFLAGS)
BLD := build

GRB_ROOT := vendor/GraphBLAS
GRB_DIST := $(GRB_ROOT)/dist

LGR_ROOT := vendor/LAGraph
LGR_DIST := $(LGR_ROOT)/dist

SPLA_ROOT := vendor/spla
SPLA_DIST := $(SPLA_ROOT)/build

BINS := prim_lagr prim_spla msbfs_lagr

.PHONY: build vendor
build: $(addprefix $(BLD)/, $(BINS))
vendor: $(GRB_DIST) $(LGR_DIST) $(SPLA_DIST)

$(GRB_DIST):
	$(MAKE) -C $(GRB_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(GRB_ROOT) install

$(LGR_DIST): $(GRB_DIST)
	$(MAKE) -C $(LGR_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(LGR_ROOT) install

$(SPLA_DIST):
	git -C $(SPLA_ROOT) reset --hard origin
	git -C $(SPLA_ROOT) quiltimport --patches ../spla.patches

	cmake -S $(SPLA_ROOT) -B $(SPLA_DIST) -DCMAKE_BUILD_TYPE=Release \
		-DSPLA_BUILD_OPENCL=YES -DSPLA_BUILD_TESTS=NO -DSPLA_BUILD_EXAMPLES=NO
	cmake --build $(SPLA_DIST) -j$(JOBS)

LD_GRB_LGR := \
	-I$(GRB_DIST)/include/suitesparse -L$(GRB_DIST)/lib64 -lgraphblas \
	-Wl,-rpath=$(realpath $(GRB_DIST)/lib64) \
	-I$(LGR_DIST)/include/suitesparse -L$(LGR_DIST)/lib64 \
	-l:liblagraphx.a -l:liblagraph.a -lgomp

LD_SPLA := \
	-I$(SPLA_ROOT)/include -L$(SPLA_DIST) -lspla_x64 \
	-Wl,-rpath=$(shell realpath $(SPLA_DIST))

$(BLD)/prim_lagr: prim/lagr.c $(GRB_DIST) $(LGR_DIST)
	$(CC) $(CFLAGS) $< -o $@ $(LD_GRB_LGR)

$(BLD)/prim_spla: prim/spla.c $(SPLA_DIST) $(GRB_DIST) $(LGR_DIST)
	$(CC) $(CFLAGS) $< -o $@ $(LD_GRB_LGR) $(LD_SPLA)

$(BLD)/msbfs_lagr: msbfs/lagr.c $(GRB_DIST) $(LGR_DIST)
	$(CC) $(CFLAGS) $< -o $@ $(LD_GRB_LGR)
