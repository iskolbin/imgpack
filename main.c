/*
 * ImgPack -- simple texture packer v0.5
 *
 * Author: Ilya Kolbin
 * Source: github.com:iskolbin/imgpack
 * License: MIT, see bottom of this file
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define CUTE_FILES_IMPLEMENTATION
#include "cute_files.h"

struct ImgPackContext {
	int (*formatter)(struct ImgPackContext *ctx, FILE *output_file, int argc, char *argv[]);
	int forcePOT;
	int forceSquared;
	int allowMultipack;
	int trimThreshold;
	int maxWidth;
	int maxHeight;
	int padding;
	int extrude;
	int verbose;

	char **imagePaths;
	struct stbrp_rect *sourceRects;
	struct stbrp_rect *packingRects;
	int size;
	int allocated;

	int width;
	int height;
	int scaleNumerator;
	int scaleDenominator;

	char *outputImagePath;
	char *outputDataPath;
};

static int is_image_rotated(struct ImgPackContext *ctx, int id) {
	if (id >= 0 && id < ctx->size) {
		return 0;
	} else {
		return -1;
	}
}

static int is_image_trimmed(struct ImgPackContext *ctx, int id) {
	if (id >= 0 && id < ctx->size) {
		int d = ctx->padding + ctx->extrude;
		return ctx->sourceRects[id].x != 0 || ctx->sourceRects[id].y != 0 || ctx->sourceRects[id].w != ctx->packingRects[id].w-2*d || ctx->sourceRects[id].h != ctx->packingRects[id].h-2*d; 
	} else {
		return -1;
	}
}

static const char *get_output_image_format(struct ImgPackContext *ctx) {
	return "RGBA8888";
}

static struct stbrp_rect get_frame_rect(struct ImgPackContext *ctx, int id) {
	struct stbrp_rect frame = {0};
	if (id >= 0 && id < ctx->size) {
		int d = ctx->padding + ctx->extrude;
		frame.x = ctx->packingRects[id].x + d;
		frame.y = ctx->packingRects[id].y + d;
		frame.w = ctx->packingRects[id].w - 2*d;
		frame.h = ctx->packingRects[id].h - 2*d;
	}
	return frame;
}

#include "formatters/C.h"
#include "formatters/JSON_ARRAY.h"
#include "formatters/JSON_HASH.h"
#include "formatters/RAYLIB.h"

static int parse_format(struct ImgPackContext *ctx, const char *s) {
	if (!strcmp(s, "C")) ctx->formatter = imgpack_formatter_C;
	else if (!strcmp(s, "JSON_ARRAY")) ctx->formatter = imgpack_formatter_JSON_ARRAY;
	else if (!strcmp(s, "JSON_HASH")) ctx->formatter = imgpack_formatter_JSON_HASH;
	else if (!strcmp(s, "RAYLIB")) ctx->formatter = imgpack_formatter_RAYLIB;
	else return 1;
	return 0;
}

static void allocate_images_data(struct ImgPackContext *ctx) {
	int next_size = ctx->allocated * 2;
	if (next_size == 0) next_size = 16;
	ctx->imagePaths = realloc(ctx->imagePaths, next_size * sizeof(*ctx->imagePaths));
	ctx->packingRects = realloc(ctx->packingRects, next_size * sizeof(*ctx->packingRects));
	ctx->sourceRects = realloc(ctx->sourceRects, next_size * sizeof(*ctx->sourceRects));
	ctx->allocated = next_size;
}

static void add_image_data(struct ImgPackContext *ctx, stbi_uc *data, const char *img_path, int width, int height) {
	int id = ctx->size;
	while (id >= ctx->allocated) {
		allocate_images_data(ctx);
	}
	ctx->size++;

	int minY = 0, minX = 0, maxY = height-1, maxX = width-1;
	if (ctx->trimThreshold >= 0) {
		for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) if (data[4*(y*width+x)+3] > ctx->trimThreshold) {
			minY = y;
			y = height;
			break;
		}
		for (int y = height-1; y >= 0; y--) for (int x = 0; x < width; x++) if (data[4*(y*width+x)+3] > ctx->trimThreshold) {
			maxY = y;
			y = -1;
			break;
		}
		for (int x = 0; x < width; x++) for (int y = 0; y < height; y++) if (data[4*(y*width+x)+3] > ctx->trimThreshold) {
			minX = x;
			x = width;
			break;
		}
		for (int x = width-1; x >= 0; x--) for (int y = 0; y < height; y++) if (data[4*(y*width+x)+3] > ctx->trimThreshold) {
			maxX = x;
			x = -1;
			break;
		}
	}
	ctx->sourceRects[id] = (stbrp_rect) {.x = minX, .y = minY, .w = width, .h = height};

	char *path = malloc(strlen(img_path)+1);
	strcpy(path, img_path);
	ctx->imagePaths[id] = path;
	ctx->packingRects[id] = (stbrp_rect) {
		.id = id,
		.w = maxX - minX + 1 + 2*(ctx->padding + ctx->extrude),
		.h = maxY - minY + 1 + 2*(ctx->padding + ctx->extrude),
	};
	if (ctx->verbose) printf("  Added \"%s\" %dx%d(trimmed to %dx%d)\n", path, width, height, maxX - minX + 1, maxY - minY + 1);
}

static int get_images_data(struct ImgPackContext *ctx, const char *path) {
	int count = 0;
	cf_dir_t dir;
	if (cf_dir_open(&dir, path)) {
		if (ctx->verbose) printf("Reading images data from \"%s\" directory\n", path);
		while(dir.has_next) {
			cf_file_t file;
			cf_read_file(&dir, &file);
			if (file.is_reg) {
				if (file.is_dir) {
					if (ctx->verbose) printf("  Traverse %s\n", file.name);
					count += get_images_data(ctx, file.path);
				} else {
					if (ctx->verbose) printf("  Reading %s\n", file.name);
					int width, height, channels;
					stbi_uc *data = stbi_load(file.path, &width, &height, &channels, 4);
					if (data) {
						add_image_data(ctx, data, file.path, width, height);
						count++;
						stbi_image_free(data);
					}
				}
			}
			cf_dir_next(&dir);
		}
		cf_dir_close(&dir);
		if (ctx->verbose) printf("From %s added %d images\n", path, count);
	}
	return count;
}

static unsigned long upper_power_of_two(unsigned long v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static void pack_images(struct ImgPackContext *ctx) {
	int occupied_area = 0;
	int side_add = ctx->padding + ctx->extrude;
	for (int i = 0; i < ctx->size; i++) {
		occupied_area += ((side_add+ctx->packingRects[i].w)*(side_add+ctx->packingRects[i].h));
	}
	if (ctx->verbose) printf("Occupied area is %d\n", occupied_area);
	int assumed_side_size = (int)(1.1*sqrt((double)occupied_area));
	stbrp_node *nodes = malloc(sizeof(*nodes) * ctx->size);
	if (ctx->forcePOT) {
		assumed_side_size = upper_power_of_two(assumed_side_size);
	}
	ctx->width = assumed_side_size;
	ctx->height = assumed_side_size;
	stbrp_context rp_ctx = {0};
	if (ctx->verbose) printf("Trying to pack %d images into %dx%d\n", ctx->size, assumed_side_size, assumed_side_size);
	stbrp_init_target(&rp_ctx, assumed_side_size, assumed_side_size, nodes, ctx->size);
	stbrp_pack_rects(&rp_ctx, ctx->packingRects, ctx->size);
	free(nodes);
}

static int write_atlas_data(struct ImgPackContext *ctx, int argc, char *argv[]) {
	FILE *output_file = stdout;
	if (ctx->outputDataPath) {
		output_file = fopen(ctx->outputDataPath, "w+");
	}
	int status = 1;
	if (output_file) {
		if (ctx->verbose) printf("Writing description data to \"%s\"\n", ctx->outputDataPath);
		status = ctx->formatter(ctx, output_file, argc, argv);
		if (output_file != stdout) fclose(output_file);
	} else {
		printf("Cannot open for writing \"%s\"", ctx->outputDataPath);
	}
	return status;
}

static int write_atlas_image(struct ImgPackContext *ctx) {
	unsigned char *output_data = malloc(4 * ctx->width * ctx->height);
	if (ctx->verbose) printf("Drawing atlas image to \"%s\"\n", ctx->outputImagePath);
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect rect = ctx->packingRects[i];
		int x0 = ctx->sourceRects[i].x, y0 = ctx->sourceRects[i].y;
		int d = ctx->padding + ctx->extrude;
		int w, h, n;
		stbi_uc *image_data = stbi_load(ctx->imagePaths[i], &w, &h, &n, 4);
		if (ctx->verbose) printf(" Drawing %s\n", ctx->imagePaths[i]);
		for (int y = 0; y < rect.h-2*d; y++) {
			for (int x = 0; x < rect.w-2*d; x++) {
				int output_offset = 4*(x+rect.x+d + (y+rect.y+d)*ctx->width);
				for (int j = 0; j < 4; j++) {
					output_data[output_offset+j] = image_data[4*(x+x0+(y+y0)*w)+j];
				}
			}
		}

		if (ctx->extrude > 0) {
			int d = ctx->padding + ctx->extrude;
			if (y0 == 0) {
				for (int y = ctx->padding; y < d; y++) {
					for (int x = 0; x < rect.w-2*d; x++) {
						int output_offset = 4*(x+rect.x+d + (y+rect.y)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x+x0 + (y0)*w) + j];
						}
					}
				}
			}
			if (x0 == 0) {
				for (int y = 0; y < rect.h-2*d; y++) {
					for (int x = ctx->padding; x < d; x++) {
						int output_offset = 4*(x+rect.x + (y+rect.y+d)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0 + (y0+y)*w) + j];
						}
					}
				}
			}
			if (y0 + rect.h - 2*d == h) {
				for (int y = d+h; y < d+h+ctx->extrude; y++) {
					for (int x = 0; x < rect.w-2*d; x++) {
						int output_offset = 4*(x+rect.x+d + (y+rect.y-y0)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0+x + (y0+rect.h-2*d-1)*w) + j];
						}
					}
				}
			}
			if (x0 + rect.w - 2*d == w) {
				for (int y = 0; y < rect.h-2*d; y++) {
					for (int x = d+w; x < d+w+ctx->extrude; x++) {
						int output_offset = 4*(x+rect.x-x0 + (y+rect.y+d)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0+rect.w-2*d-1 + (y0+y)*w) + j];
						}
					}
				}
			}
			if (y0 == 0 && x0 == 0) {
				for (int y = ctx->padding; y < d; y++) {
					for (int x = ctx->padding; x < d; x++) {
						int output_offset = 4*(x+rect.x + (y+rect.y)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0 + (y0)*w) + j];
						}
					}
				}
			}
			if (y0 == 0 && x0 + rect.w - 2*d == w) {
				for (int y = ctx->padding; y < d; y++) {
					for (int x = d+w; x < d+w+ctx->extrude; x++) {
						int output_offset = 4*(x+rect.x-x0 + (y+rect.y)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0+rect.w-2*d-1 + (y0)*w) + j];
						}
					}
				}
			}
			if (y0 + rect.h - 2*d == h && x0 == 0) {
				for (int y = d+h; y < d+h+ctx->extrude; y++) {
					for (int x = ctx->padding; x < d; x++) {
						int output_offset = 4*(x+rect.x + (y+rect.y-y0)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0 + (y0+rect.h-2*d-1)*w) + j];
						}
					}
				}
			}
			if (y0 + rect.h - 2*d == h && x0 + rect.w - 2*d == w) {
				for (int y = d+h; y < d+h+ctx->extrude; y++) {
					for (int x = d+w; x < d+w+ctx->extrude; x++) {
						int output_offset = 4*(x+rect.x + (y+rect.y-y0)*ctx->width);
						for (int j = 0; j < 4; j++) {
							output_data[output_offset+j] = image_data[4*(x0+rect.w-2*d-1 + (y0+rect.h-2*d-1)*w) + j];
						}
					}
				}
			}
		}
		stbi_image_free(image_data);
	}
	stbi_write_png(ctx->outputImagePath, ctx->width, ctx->height, 4, output_data, ctx->width*4);
	free(output_data);
	return 0;
}

static void clear_context(struct ImgPackContext *ctx) {
	for (int i = 0; i < ctx->size; i++) {
		free(ctx->imagePaths[i]);
	};
	free(ctx->imagePaths);
	free(ctx->packingRects);
	free(ctx->sourceRects);
	ctx->imagePaths = NULL;
	ctx->packingRects = NULL;
	ctx->sourceRects = NULL;
	ctx->size = 0;
	ctx->allocated = 0;
}

int main(int argc, char *argv[]) {
	struct ImgPackContext ctx = {
		.scaleNumerator = 1,
		.scaleDenominator = 1,
		.trimThreshold = -1,
	};
	char *imagesPath = argv[argc-1];
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--data")) {
			ctx.outputDataPath = argv[++i];
		} else if (!strcmp(argv[i], "--image")) {
			ctx.outputImagePath = argv[++i];
		} else if (!strcmp(argv[i], "--force-pot")) {
			ctx.forcePOT = 1;
		} else if (!strcmp(argv[i], "--force-squared")) {
			ctx.forceSquared = 1;
		} else if (!strcmp(argv[i], "--trim")) {
			ctx.trimThreshold = strtol(argv[++i], NULL, 10);
		} else if (!strcmp(argv[i], "--padding")) {
			ctx.padding = strtol(argv[++i], NULL, 10);
		} else if (!strcmp(argv[i], "--extrude")) {
			ctx.extrude = strtol(argv[++i], NULL, 10);
		} else if (!strcmp(argv[i], "--verbose")) {
			ctx.verbose = 1;
		} else if (!strcmp(argv[i], "--format")) {
			if (!parse_format(&ctx, argv[++i])) {
				if (ctx.verbose) printf("Using format %s", argv[i]);
			} else {
				printf("Bad format \"%s\"\n", argv[i]);
				return 1;
			}
		}
	}

	if (!ctx.formatter) {
		printf("Specify format\n");
		return 1;
	}
	get_images_data(&ctx, imagesPath);
	pack_images(&ctx);
	if (write_atlas_data(&ctx, argc, argv)) return 1;
	if (ctx.outputImagePath && write_atlas_image(&ctx)) return 1;
	clear_context(&ctx);
	return 0;
}

/*
Copyright (c) 2019 Ilya Kolbin (iskolbin@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
