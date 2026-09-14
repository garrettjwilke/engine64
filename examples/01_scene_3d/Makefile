PROJECT_NAME = 01_scene_3d

# Path to the engine64 checkout. Set it to wherever the engine lives; this
# example sits two levels down inside the repo.
ENGINE_DIR = ../..

src = $(wildcard *.c)

# How much the models grow on the way in. They are authored in metres and the
# engine works at 64 units per metre, which is what this puts them at.
MODEL_SCALE = 64

# --asset-path: the importer looks every material's texture up by FILE NAME,
# recursively, under this path, and builds the path left in the .t3dm by
# replacing this prefix with "rom:/".
GLTF_FLAGS = '--base-scale=$(MODEL_SCALE)' '--asset-path=assets'

include $(ENGINE_DIR)/engine.mk
