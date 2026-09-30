/*
	Reads a Tiled map (.tmx with CSV layers) and its tileset (.tsx, a
	collection of one image per tile) and writes the stage binary
	(big-endian, native N64 format).

	Layout, every block padded to 8 bytes:

	  header      magic "STG2", version, layer count, grid width and height in
	              cells, row stride in bytes, cell width and height in pixels,
	              tile count, digits of the tile number, length of the base path
	  base path   the ROM path of the tiles up to their number, e.g.
	              "rom:/stages/kenney_pixel-platformer/tile_"; the engine
	              appends the number and ".sprite"
	  collision   256 bytes, one per tile number as the cells count them: the
	              collision of tile n-1 at n, 0 none; from the tileset's
	              "collision" int tile property, or 1 for a "solid" bool one
	  layers      parallax and grid offset from the file start, per layer, in
	              draw order, back to front
	  grids       one byte per cell, row by row, 0 empty, n the tile n-1; each
	              row padded to the stride

	The tile images must be named <prefix><number>.png with the number equal
	to the tile id, which is what the engine relies on to reach a tile from
	the first one plus an offset. They live under assets/stages/<stage>/Tiles/
	or the shared assets/stages/Tiles/, in that folder or one below it; the
	ROM keeps that layout minus "Tiles/".
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LAYERS 16
#define MAX_TILES  255

typedef struct {
	float    parallax;
	uint8_t *cell;
} Layer;


static void fail(const char *message, const char *detail)
{
	fprintf(stderr, "Error: %s%s%s\n", message, detail ? ": " : "", detail ? detail : "");
	exit(1);
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) fail("cannot open", path);
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *data = (char *)malloc(size + 1);
	if (fread(data, 1, size, f) != (size_t)size) fail("cannot read", path);
	data[size] = 0;
	fclose(f);
	return data;
}

/* The directory part of a path, with its trailing slash, or "" when none. */
static void dir_of(const char *path, char *out, size_t size)
{
	const char *slash = strrchr(path, '/');
	size_t n = slash ? (size_t)(slash - path + 1) : 0;
	if (n >= size) fail("path too long", path);
	memcpy(out, path, n);
	out[n] = 0;
}

/* Joins a directory and a relative path and folds every "x/.." out of it,
   so a tileset's "../Tiles/tile.png" names the real folder. */
static void resolve_path(const char *dir, const char *relative, char *out, size_t size)
{
	char joined[2048];
	snprintf(joined, sizeof joined, "%s%s", dir, relative);

	char *segment[256];
	int   count = 0;
	for (char *s = strtok(joined, "/"); s; s = strtok(NULL, "/")) {
		if (strcmp(s, ".") == 0) continue;
		if (strcmp(s, "..") == 0 && count > 0 && strcmp(segment[count - 1], "..") != 0) { count--; continue; }
		if (count == 256) fail("path too deep", relative);
		segment[count++] = s;
	}

	size_t n = 0;
	out[0] = 0;
	for (int i = 0; i < count; i++) {
		int w = snprintf(out + n, size - n, "%s%s", i ? "/" : "", segment[i]);
		if (w < 0 || (size_t)w >= size - n) fail("path too long", relative);
		n += w;
	}
}


/* --- minimal XML reading: attributes of the tag at a position ------------ */

/* Finds the next tag with this name starting at *cursor, advances the cursor
   past its name, and returns where it starts, or NULL. */
static const char *next_tag(const char **cursor, const char *name)
{
	size_t n = strlen(name);
	const char *p = *cursor;
	while ((p = strchr(p, '<'))) {
		if (strncmp(p + 1, name, n) == 0 && (isspace((unsigned char)p[1 + n]) || p[1 + n] == '>' || p[1 + n] == '/')) {
			*cursor = p + 1 + n;
			return p;
		}
		p++;
	}
	return NULL;
}

/* The value of an attribute inside the tag at tag, or NULL when absent. The
   value is copied into out. */
static const char *attribute(const char *tag, const char *name, char *out, size_t size)
{
	const char *end = strchr(tag, '>');
	size_t n = strlen(name);
	const char *p = tag;
	while ((p = strstr(p, name)) && p < end) {
		if (isspace((unsigned char)p[-1]) && p[n] == '=' && p[n + 1] == '"') {
			const char *v = p + n + 2;
			const char *q = strchr(v, '"');
			if (!q || (size_t)(q - v) >= size) fail("attribute too long", name);
			memcpy(out, v, q - v);
			out[q - v] = 0;
			return out;
		}
		p += n;
	}
	return NULL;
}

static int attribute_int(const char *tag, const char *name, int fallback)
{
	char buf[64];
	return attribute(tag, name, buf, sizeof buf) ? atoi(buf) : fallback;
}

static float attribute_float(const char *tag, const char *name, float fallback)
{
	char buf[64];
	return attribute(tag, name, buf, sizeof buf) ? (float)atof(buf) : fallback;
}


/* --- big-endian writing --------------------------------------------------- */

static size_t bytes_written = 0;

static void write_u8(FILE *f, uint8_t v)  { fputc(v, f); bytes_written++; }
static void write_u16(FILE *f, uint16_t v) { write_u8(f, v >> 8); write_u8(f, v & 0xFF); }
static void write_u32(FILE *f, uint32_t v) { write_u16(f, v >> 16); write_u16(f, v & 0xFFFF); }
static void write_f32(FILE *f, float v)
{
	uint32_t bits;
	memcpy(&bits, &v, sizeof bits);
	write_u32(f, bits);
}
static void align8(FILE *f) { while (bytes_written % 8) write_u8(f, 0); }
static size_t padded8(size_t n) { return (n + 7) & ~(size_t)7; }


int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <map.tmx> <out.stage2d>\n", argv[0]);
		return 1;
	}

	const char *tmx_path = argv[1];
	const char *out_path = argv[2];

	char *tmx = read_file(tmx_path);
	char tmx_dir[1024];
	dir_of(tmx_path, tmx_dir, sizeof tmx_dir);

	/* --- map ------------------------------------------------------------- */
	const char *cursor = tmx;
	const char *map = next_tag(&cursor, "map");
	if (!map) fail("no <map> in", tmx_path);

	int width       = attribute_int(map, "width", 0);
	int height      = attribute_int(map, "height", 0);
	int cell_width  = attribute_int(map, "tilewidth", 0);
	int cell_height = attribute_int(map, "tileheight", 0);
	if (!width || !height || !cell_width || !cell_height) fail("map without size", tmx_path);

	char buf[1024];
	if (attribute(map, "infinite", buf, sizeof buf) && strcmp(buf, "1") == 0) fail("infinite maps are not supported", tmx_path);
	if (attribute(map, "orientation", buf, sizeof buf) && strcmp(buf, "orthogonal") != 0) fail("only orthogonal maps are supported", tmx_path);

	/* --- tileset: one, external ----------------------------------------- */
	const char *ts = next_tag(&cursor, "tileset");
	if (!ts) fail("no <tileset> in", tmx_path);
	int firstgid = attribute_int(ts, "firstgid", 1);
	if (!attribute(ts, "source", buf, sizeof buf)) fail("the tileset must be an external .tsx", tmx_path);

	char tsx_path[2048];
	snprintf(tsx_path, sizeof tsx_path, "%s%s", tmx_dir, buf);
	char *tsx = read_file(tsx_path);

	const char *probe = cursor;
	if (next_tag(&probe, "tileset")) fail("more than one tileset in", tmx_path);

	const char *tcur = tsx;
	const char *tileset = next_tag(&tcur, "tileset");
	if (!tileset) fail("no <tileset> in", tsx_path);
	int tile_count = attribute_int(tileset, "tilecount", 0);
	if (tile_count < 1 || tile_count > MAX_TILES) fail("tile count must be 1 to 255", tsx_path);
	if (attribute_int(tileset, "columns", 0) != 0) fail("the tileset must be a collection of images, not a sheet", tsx_path);

	/* Every tile image is <prefix><number>.png with number == id, the number
	   written with the same amount of digits, all in the same folder under
	   some pack's Tiles/. A tile's "collision" property is what the game
	   makes of touching it, 0 none; a "solid" property set is collision 1. */
	char    prefix[256] = "";
	char    folder[512] = "";   /* "<stage>/<below Tiles/>", e.g. "kenney_pixel-platformer/Backgrounds/" */
	int     digits = 0;
	int     seen = 0;
	uint8_t tile_collision[MAX_TILES + 1] = {0};
	const char *tile;
	while ((tile = next_tag(&tcur, "tile"))) {
		int id = attribute_int(tile, "id", -1);
		const char *tile_end = strstr(tcur, "</tile>");
		if (!tile_end) tile_end = tcur + strlen(tcur);

		/* Only this tile's properties: they sit before its </tile>. */
		const char *pcur = tcur;
		const char *prop;
		while ((prop = next_tag(&pcur, "property")) && prop < tile_end) {
			char pname[64], pvalue[64];
			if (!attribute(prop, "name", pname, sizeof pname) || !attribute(prop, "value", pvalue, sizeof pvalue)) continue;

			int value = -1;
			if (strcmp(pname, "collision") == 0) {
				value = atoi(pvalue);
				if (value < 0 || value > 255) fail("tile collision must be 0 to 255 in", tsx_path);
			} else if (strcmp(pname, "solid") == 0 && strcmp(pvalue, "true") == 0) {
				value = 1;
			}
			if (value < 0) continue;

			if (id < 0 || id >= MAX_TILES) fail("tile with a collision out of range in", tsx_path);
			if (tile_collision[id + 1] && tile_collision[id + 1] != value) fail("tile with two collisions in", tsx_path);
			tile_collision[id + 1] = (uint8_t)value;
		}

		const char *icur = tcur;
		const char *image = next_tag(&icur, "image");
		if (!image || image > tile_end || !attribute(image, "source", buf, sizeof buf)) fail("tile without image in", tsx_path);
		tcur = icur;

		/* Where the image lives, from the pack down: the segment before
		   "Tiles/" is the pack, whatever follows it is kept. The source is
		   relative to the tileset, so it is resolved first. */
		char this_folder[512];
		{
			char tsx_dir[1024], full[2048];
			dir_of(tsx_path, tsx_dir, sizeof tsx_dir);
			resolve_path(tsx_dir, buf, full, sizeof full);

			const char *tiles = strstr(full, "/Tiles/");
			if (!tiles) fail("tile image is not under a Tiles/ folder", full);
			const char *pack = tiles;
			while (pack > full && pack[-1] != '/') pack--;
			if (pack == tiles) fail("tile image has no folder before Tiles/", full);
			int pack_len = (int)(tiles - pack) + 1;
			if (pack_len == 7 && strncmp(pack, "stages/", 7) == 0) pack_len = 0;   /* the shared assets/stages/Tiles/ */
			const char *below = tiles + strlen("/Tiles/");
			const char *last  = strrchr(below, '/');
			snprintf(this_folder, sizeof this_folder, "%.*s%.*s", pack_len, pack,
			         last ? (int)(last - below + 1) : 0, below);
		}

		char *name = strrchr(buf, '/');
		name = name ? name + 1 : buf;
		char *dot = strrchr(name, '.');
		if (!dot || strcmp(dot, ".png") != 0) fail("tile image is not a .png", buf);
		*dot = 0;

		char *num = dot;
		while (num > name && isdigit((unsigned char)num[-1])) num--;
		if (num == dot) fail("tile image name does not end in a number", buf);
		if (atoi(num) != id) fail("tile image number differs from its id", buf);

		if (seen == 0) {
			digits = (int)(dot - num);
			snprintf(prefix, sizeof prefix, "%.*s", (int)(num - name), name);
			snprintf(folder, sizeof folder, "%s", this_folder);
		} else if ((int)(dot - num) != digits || strncmp(name, prefix, num - name) != 0 || (size_t)(num - name) != strlen(prefix)) {
			fail("tile image name does not follow the first tile's pattern", buf);
		} else if (strcmp(folder, this_folder) != 0) {
			fail("tile image is not in the first tile's folder", buf);
		}
		seen++;
	}
	if (seen != tile_count) fail("tile count does not match the tiles listed in", tsx_path);

	/* --- layers ------------------------------------------------------------ */
	Layer layers[MAX_LAYERS];
	int   layer_count = 0;
	const char *layer;
	while ((layer = next_tag(&cursor, "layer"))) {
		if (layer_count == MAX_LAYERS) fail("too many layers in", tmx_path);
		if (attribute_int(layer, "width", width) != width || attribute_int(layer, "height", height) != height)
			fail("layer size differs from the map", tmx_path);

		Layer *l = &layers[layer_count++];
		l->parallax = attribute_float(layer, "parallaxx", 1.0f);
		l->cell = (uint8_t *)calloc(width * height, 1);

		const char *dcur = cursor;
		const char *data = next_tag(&dcur, "data");
		if (!data || !attribute(data, "encoding", buf, sizeof buf) || strcmp(buf, "csv") != 0)
			fail("layer data must be CSV", tmx_path);
		const char *p = strchr(data, '>') + 1;

		for (int i = 0; i < width * height; i++) {
			char *end;
			unsigned long gid = strtoul(p, &end, 10);
			if (end == p) fail("layer has fewer cells than the map", tmx_path);
			p = end;
			while (*p == ',' || isspace((unsigned char)*p)) p++;

			if (gid & 0xE0000000) fail("flipped or rotated tiles are not supported", tmx_path);
			if (gid == 0) continue;
			long n = (long)gid - firstgid;
			if (n < 0 || n >= tile_count) fail("cell names a tile outside the tileset", tmx_path);
			l->cell[i] = (uint8_t)(n + 1);
		}
		cursor = p;
	}
	if (!layer_count) fail("no layers in", tmx_path);

	/* --- base path: rom:/stages/<pack>/<below Tiles/><prefix> ---------------- */
	char base[1024];
	snprintf(base, sizeof base, "rom:/stages/%s%s", folder, prefix);
	size_t base_len = strlen(base) + 1;
	if (base_len > 255) fail("base path too long", base);

	/* --- write --------------------------------------------------------------- */
	FILE *out = fopen(out_path, "wb");
	if (!out) fail("cannot write", out_path);

	size_t stride       = padded8(width);
	size_t header_size  = 24;
	size_t base_offset  = header_size;
	size_t collision_offset = base_offset + padded8(base_len);
	size_t table_offset = collision_offset + sizeof tile_collision;
	size_t grid_offset  = table_offset + padded8(layer_count * 8);
	size_t grid_size    = padded8(stride * height);

	fwrite("STG2", 1, 4, out); bytes_written = 4;
	write_u8(out, 3);
	write_u8(out, layer_count);
	write_u16(out, width);
	write_u16(out, height);
	write_u16(out, stride);
	write_u16(out, cell_width);
	write_u16(out, cell_height);
	write_u16(out, tile_count);
	write_u8(out, digits);
	write_u8(out, base_len);
	align8(out);

	fwrite(base, 1, base_len, out); bytes_written += base_len;
	align8(out);

	fwrite(tile_collision, 1, sizeof tile_collision, out); bytes_written += sizeof tile_collision;

	for (int i = 0; i < layer_count; i++) {
		write_f32(out, layers[i].parallax);
		write_u32(out, grid_offset + i * grid_size);
	}
	align8(out);

	for (int i = 0; i < layer_count; i++) {
		for (int y = 0; y < height; y++) {
			fwrite(layers[i].cell + y * width, 1, width, out);
			bytes_written += width;
			for (size_t x = width; x < stride; x++) write_u8(out, 0);
		}
		align8(out);
	}
	fclose(out);

	int collision_count = 0;
	for (int i = 1; i <= tile_count; i++) collision_count += tile_collision[i] != 0;

	printf("%s: %dx%d cells of %dx%d, %d layers, %d tiles (%d with collision), %zu bytes\n",
	       out_path, width, height, cell_width, cell_height, layer_count, tile_count, collision_count, bytes_written);
	return 0;
}
