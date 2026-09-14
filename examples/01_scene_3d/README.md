# 01 scene 3d

Smallest 3D world engine64 can put on screen: a room, a lamp post standing in it, and a camera to look around with.

Everything in `main.c` is content. Assets, where they are placed, the light, the fog, the camera and the one game state that draws them are all data the game hands over; the engine supplies the rest.

### Scene

A `Scene3DDef` is the whole world: a placement table plus light, fog and camera. Entering the game state that names it builds the scene, leaving frees it.

Prefabs carry no transform, so one prefab can fill any number of rows in the placement table. Rows are built in order, and the live entities keep that order.

### Model parts

`lamp_post.glb` holds two objects. Listing their names in the prefab makes each one a part that can be drawn or skipped at runtime, and gives it a position of its own inside the entity. Whatever the list does not name is grouped into one remaining part, always drawn. Up to seven names.

### Camera

Scene definition carries the whole camera: lens, clipping planes, arm length, angles, and the settings the arm runs on. No defaults exist, so a value left out is zero and the camera behaves accordingly.

Control is a binding: the game names a button per action and the engine applies the motion every frame. What the camera orbits is the one thing the game decides, handed over once per frame.

### Controls

| Action | Button |
| --- | --- |
| Move the point the camera orbits | Stick |
| Turn and tilt | C buttons |
| Pull in and out | L and R |
| Narrow and widen the lens | D-Up and D-Down |
| Show and hide the lamp | A |
| Show and hide the post | B |

### Building

`ENGINE_DIR` at the top of the `Makefile` points at the engine checkout, two levels up from here. With the Libdragon toolchain and Tiny3D installed:

```
make
```
