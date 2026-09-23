# Origen

Copia de `tools/gltf_importer` de tiny3d, commit
`49569ffb25a45e7f7839e985cb0418263c494837` (2026-09-14), tomada del árbol en
`/home/zonca/Programacion/n64/librerias/tiny3d`.

El binario se sigue llamando `gltf_to_t3d` y escribe el mismo formato `.t3dm`
que el de `/opt/libdragon/bin`: un `.glb` convertido por cualquiera de los dos
sirve en la misma ROM.

# Por qué la copia

Los modelos con esqueleto no reciben stripificación. `optimizeModelChunk` en
`src/optimizer/meshOptimizer.cpp` saltea todo chunk con `boneCount > 0`, con un
`@TODO: handle this`. En el personaje del benchmark eso son 578 triángulos por
frame como 578 comandos `t3d_tri_draw` sueltos.

# Cambios locales

Cada uno lleva el comentario `e64:` en el código.

- `src/converter/meshConverter.cpp`: `vertsByBone` pasa de `unordered_map` a
  `map`. Su orden de iteración fija el `vertexDestOffset` de cada subchunk, así
  que sin esto el mismo `.glb` puede dar dos archivos distintos y ninguna
  medición se compara con otra.
- `src/converter/meshConverter.cpp`: asserts de la invariante en la que se
  apoya el DMA de strips, que dentro de un grupo solo el último subchunk lleva
  índices, y de que todo índice cae en los slots del grupo.
- `src/optimizer/meshOptimizer.cpp`: con `--skin-strips`, los chunks con hueso
  entran a la stripificación en lugar de saltearse. El mapa de uso de slots
  admite un índice más que el tamaño del cache.
- `src/writer.cpp`: assert del largo de strip, que sale al archivo como un
  byte, y la línea de `--chunk-stats`.
- `src/main.cpp`, `src/structs.h`: las dos banderas nuevas.

# Para rebasear

Traer el `gltf_importer` del commit nuevo de tiny3d y reaplicar lo que esta
lista enumere, que es exactamente lo que `grep -rn 'e64:' src/` encuentra.
