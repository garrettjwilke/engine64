# Host tool: Tiled .tmx to .stage2d. Built here by the engine; engine.mk
# invokes this Makefile the first time a project needs the binary.
CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

stage_importer: main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f stage_importer

.PHONY: clean
