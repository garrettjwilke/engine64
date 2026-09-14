PROJECT_NAME = 02_character_3d

# Path to the engine64 checkout. Set it to wherever the engine lives; this
# example sits two levels down inside the repo.
ENGINE_DIR = ../..

# main.c, and one file per piece of the scene.
src = $(wildcard *.c) $(wildcard scene/*.c)

# How much the models grow on the way in. They are authored in metres and the
# engine works at 64 units per metre, which is what this puts them at.
MODEL_SCALE = 64

# --asset-path: the importer looks every material's texture up by FILE NAME,
# recursively, under this path, and builds the path left in the .t3dm by
# replacing this prefix with "rom:/".
GLTF_FLAGS = '--base-scale=$(MODEL_SCALE)' '--asset-path=assets'

# The room is the only model that also needs a collision mesh: it is what the
# character stands on.
assets_collision = filesystem/collision/room.collision

include $(ENGINE_DIR)/engine.mk
