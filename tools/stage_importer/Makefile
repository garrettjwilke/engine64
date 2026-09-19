# Host tool: Tiled .tmx to .stage2d. Built here by the engine; engine.mk
# invokes this Makefile the first time a project needs the binary.
CXX    ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra

stage_importer: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f stage_importer

.PHONY: clean
