JOBS ?= $(shell nproc)

GRB_ROOT=vendor/GraphBLAS
GRB_DIST=$(GRB_ROOT)/dist

LGR_ROOT=vendor/LAGraph
LGR_DIST=$(LGR_ROOT)/dist

.PHONY: vendor
vendor: $(GRB_DIST) $(LGR_DIST)

$(GRB_DIST):
	$(MAKE) -C $(GRB_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(GRB_ROOT) install

$(LGR_DIST): $(GRB_DIST)
	$(MAKE) -C $(LGR_ROOT) library JOBS=$(JOBS) \
		CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=$(abspath $@)"
	$(MAKE) -C $(LGR_ROOT) install
