PROJECT_NAME = 03_character_3d_collision

# Path to the engine64 checkout. Set it to wherever the engine lives; this
# example sits two levels down inside the repo.
ENGINE_DIR = ../..

src = $(wildcard *.cpp) $(wildcard prefabs/*.cpp) $(wildcard scene/*.cpp) $(wildcard controls/*.cpp)

# --asset-path: the importer looks every material's texture up by FILE NAME,
# recursively, under this path, and builds the path left in the .t3dm by
# replacing this prefix with "rom:/".
GLTF_FLAGS = '--asset-path=assets'

# Models that also need a collision mesh: the room is what the body walks on.
assets_collision = filesystem/collision/room.collision

include $(ENGINE_DIR)/engine.mk
