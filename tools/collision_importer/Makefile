# Host tool: collision mesh of a .glb to .collision. Built here by the
# engine; engine.mk invokes this Makefile the first time a project needs the
# binary. cgltf.h comes with tiny3d's own importer, so T3D_INST must be set.
CXX    ?= g++
CXXFLAGS ?= -O2

collision_importer: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -I$(T3D_INST)/tools/gltf_importer/src/lib -lm

clean:
	rm -f collision_importer

.PHONY: clean
