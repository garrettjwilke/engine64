# --- engine64 ----------------------------------------------------------------
# The whole build of a game on the engine: toolchain, engine sources, asset
# conversion and the ROM. The engine never builds on its own: it is always
# built from the consuming project, into that project's build directory.
#
# Consumer contract, before including this file:
#   PROJECT_NAME  the ROM name, without extension.
#   ENGINE_DIR    path to this repo.
#   src           the game's own .c files.
#   BUILD_DIR     the project's build dir (optional, default build).
#   ENGINE_SKIP   engine units the game replaces (optional), paths relative
#                 to $(ENGINE_DIR)/src.
#   GLTF_FLAGS    flags for the model importer (optional).
#   COL_MESHES    mesh names to keep as collision (optional, default all).
#
# Assets are found by folder under assets/ and land under filesystem/ with
# the same layout: textures, sprites, models, fonts, audio and stages. What
# needs naming is what a folder cannot say:
#   assets_collision  the .collision files wanted, one per .glb that has a
#                     collision mesh, as filesystem/collision/<name>.collision.
#   assets_extra      any other file the game converts with its own rule.
# Per-file converter flags go on the target, as make allows:
#   filesystem/fonts/Xolonium10.font64: MKFONT_FLAGS += --size 10

BUILD_DIR ?= build

# The ROM is the goal whatever target this file declares first.
.DEFAULT_GOAL := all

# The libdragon and tiny3d makefiles, unless the project pulled them in
# already (a project that builds on more than one engine does).
ifeq ($(origin N64_MKSPRITE),undefined)
include $(N64_INST)/include/n64.mk
endif
ifeq ($(origin T3D_GLTF_TO_3D),undefined)
include $(T3D_INST)/t3d.mk
endif

# $(ENGINE_DIR)/src is there to pull an engine unit in with <module/file.cpp>
# for a partial override. No exceptions, no RTTI: neither has a place on the
# console and both cost binary size and unwind tables.
N64_CXXFLAGS += -std=gnu++20 -fno-exceptions -fno-rtti -I$(ENGINE_DIR)/include -I$(ENGINE_DIR)/src

# Profile-guided code layout (MipsFit). Given the absolute path of a generated
# layout-NN.ld, it stands in for libdragon's n64.ld for this link only; a plain
# make keeps linking as always. The .ld names this ELF's .text.* sections, so
# it goes stale as the code moves: recapture the trace and regenerate it.
ifneq ($(strip $(MIPSFIT_LAYOUT_SCRIPT)),)
N64_LDFLAGS := $(filter-out -Tn64.ld,$(N64_LDFLAGS)) -T$(MIPSFIT_LAYOUT_SCRIPT)
$(BUILD_DIR)/$(PROJECT_NAME).elf: $(MIPSFIT_LAYOUT_SCRIPT)
endif

# --- sources -----------------------------------------------------------------
# The order matters, and not for tidiness: the VR4300 icache is 16 KB direct
# mapped, so where each function lands in RAM decides who it evicts. Hot path
# first, cold code last.
ENGINE_ORDER = \
	time \
	math memory physics/geometry physics/shapes physics/collision physics \
	character3d entity player controller model graphics shaders render scene3d \
	camera viewport particles sound game scene2d stage2d character2d ui menu resource debug

engine_listed = $(foreach m,$(ENGINE_ORDER),\
	$(if $(filter %.cpp,$(m)),$(m),\
	  $(patsubst $(ENGINE_DIR)/src/%,%,$(wildcard $(ENGINE_DIR)/src/$(m)/*.cpp))))

engine_src = $(filter-out $(ENGINE_SKIP),$(engine_listed))

# A module the order above misses would vanish from the build without a
# word, so whatever is left over gets appended instead of lost.
engine_all  = $(patsubst $(ENGINE_DIR)/src/%,%,$(shell find $(ENGINE_DIR)/src -name '*.cpp'))
engine_rest = $(filter-out $(ENGINE_SKIP) $(engine_listed),$(engine_all))
engine_src += $(engine_rest)

objects = $(engine_src:%.cpp=$(BUILD_DIR)/engine/%.o) \
          $(src:%.cpp=$(BUILD_DIR)/%.o)

# Engine objects land under the consumer's build/engine, so nothing is ever
# written inside the engine repo.
$(BUILD_DIR)/engine/%.o: $(ENGINE_DIR)/src/%.cpp
	@mkdir -p $(dir $@)
	@echo "    [CXX] $<"
	$(CXX) -c $(CXXFLAGS) -o $@ $<

# --- assets ------------------------------------------------------------------
assets_texture = $(wildcard assets/textures/*.png)
assets_sprite  = $(shell find assets/sprites -name '*.png' 2>/dev/null)
assets_model   = $(wildcard assets/models/*.glb)
assets_font    = $(wildcard assets/fonts/*.ttf)
assets_audio   = $(wildcard assets/audio/*.wav)
assets_stage   = $(wildcard assets/stages/*/Tiled/*.tmx)
assets_tile    = $(wildcard assets/stages/*/Tiles/*.png) $(wildcard assets/stages/*/Tiles/*/*.png)

assets = $(patsubst assets/textures/%.png,filesystem/textures/%.sprite,$(assets_texture)) \
         $(patsubst assets/sprites/%.png,filesystem/sprites/%.sprite,$(assets_sprite)) \
         $(patsubst assets/models/%.glb,filesystem/models/%.t3dm,$(assets_model)) \
         $(patsubst assets/fonts/%.ttf,filesystem/fonts/%.font64,$(assets_font)) \
         $(patsubst assets/audio/%.wav,filesystem/audio/%.wav64,$(assets_audio)) \
         $(patsubst assets/stages/%.tmx,filesystem/stages/%.stage2d,$(subst /Tiled/,/,$(assets_stage))) \
         $(patsubst assets/stages/%.png,filesystem/stages/%.sprite,$(subst /Tiles/,/,$(assets_tile))) \
         $(assets_collision) \
         $(assets_extra)

filesystem/textures/%.sprite: assets/textures/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

# Sprites keep their folder layout in the ROM: assets/sprites/a/b.png gives
# rom:/sprites/a/b.sprite.
filesystem/sprites/%.sprite: assets/sprites/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

# The engine's own copy of tiny3d's gltf importer, built on demand: it writes
# the same .t3dm, so a model converted by either binary works in the same ROM.
# MODEL_IMPORTER goes back to the installed one with
#   MODEL_IMPORTER = $(T3D_GLTF_TO_3D)
# in the project makefile. The binary is a prerequisite of the conversion: a
# change to the importer has to reconvert the models, or a measurement compares
# a new importer against models it never touched.
E64_GLTF_TO_T3D  = $(ENGINE_DIR)/tools/model_importer/gltf_to_t3d
MODEL_IMPORTER  ?= $(E64_GLTF_TO_T3D)

$(E64_GLTF_TO_T3D): $(wildcard $(ENGINE_DIR)/tools/model_importer/src/*.cpp \
                               $(ENGINE_DIR)/tools/model_importer/src/*.h \
                               $(ENGINE_DIR)/tools/model_importer/src/*/*.cpp \
                               $(ENGINE_DIR)/tools/model_importer/src/*/*.h)
	$(MAKE) -C $(ENGINE_DIR)/tools/model_importer

filesystem/models/%.t3dm: assets/models/%.glb $(MODEL_IMPORTER)
	@mkdir -p $(dir $@)
	@echo "    [T3D-MODEL] $@"
	$(MODEL_IMPORTER) --bvh $(GLTF_FLAGS) "$<" $@
	$(N64_BINDIR)/mkasset -c 2 -o $(dir $@) $@

filesystem/fonts/%.font64: assets/fonts/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o $(dir $@) "$<"

filesystem/audio/%.wav64: assets/audio/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o $(dir $@) "$<"

# --- collision ---------------------------------------------------------------
# The engine's own importer, built on demand. Every mesh in the .glb becomes
# collision unless COL_MESHES names the ones wanted.
COLLISION_IMPORTER = $(ENGINE_DIR)/tools/collision_importer/collision_importer

$(COLLISION_IMPORTER): $(ENGINE_DIR)/tools/collision_importer/main.cpp
	$(MAKE) -C $(ENGINE_DIR)/tools/collision_importer

filesystem/collision/%.collision: assets/models/%.glb $(COLLISION_IMPORTER)
	@mkdir -p $(dir $@)
	@echo "    [COLLISION] $@"
	$(COLLISION_IMPORTER) "$<" $@ $(COL_MESHES)
	$(N64_BINDIR)/mkasset -c 1 -o $(dir $@) $@

# --- stages ------------------------------------------------------------------
# A stage is authored in Tiled as assets/stages/<pack>/Tiled/<name>.tmx, with
# its tiles one image each under assets/stages/<pack>/Tiles/, in that folder
# or one below it (Tiles/Backgrounds/). The engine's own importer turns the
# map into filesystem/stages/<pack>/<name>.stage2d, and the tiles go beside
# it as sprites with the same layout minus "Tiles/", which is where the
# stage looks for them at run time.
stage_pack = $(firstword $(subst /, ,$(1)))
stage_tile = $(patsubst $(call stage_pack,$(1))/%,%,$(1))
STAGE_IMPORTER = $(ENGINE_DIR)/tools/stage_importer/stage_importer

$(STAGE_IMPORTER): $(ENGINE_DIR)/tools/stage_importer/main.cpp
	$(MAKE) -C $(ENGINE_DIR)/tools/stage_importer

.SECONDEXPANSION:
filesystem/stages/%.stage2d: assets/stages/$$(dir $$*)Tiled/$$(notdir $$*).tmx $(STAGE_IMPORTER)
	@mkdir -p $(dir $@)
	@echo "    [STAGE] $@"
	$(STAGE_IMPORTER) "$<" $@
	$(N64_BINDIR)/mkasset -c 1 -o $(dir $@) $@

filesystem/stages/%.sprite: assets/stages/$$(call stage_pack,$$*)/Tiles/$$(call stage_tile,$$*).png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

# --- ROM ---------------------------------------------------------------------
all: $(PROJECT_NAME).z64

$(BUILD_DIR)/$(PROJECT_NAME).dfs: $(assets)
$(BUILD_DIR)/$(PROJECT_NAME).elf: $(objects)

$(PROJECT_NAME).z64: N64_ROM_TITLE = "$(PROJECT_NAME)"
$(PROJECT_NAME).z64: $(BUILD_DIR)/$(PROJECT_NAME).dfs

clean:
	rm -rf $(BUILD_DIR) *.z64
	rm -rf filesystem

-include $(objects:%.o=%.d)

.PHONY: all clean
