/*
 * ImgPack -- simple texture packer v0.6.2
 *
 * Author: Ilya Kolbin
 * Source: github.com:iskolbin/imgpack
 * License: MIT, see bottom of this file
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "isl_args.h"

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define CUTE_FILES_IMPLEMENTATION
#include "cute_files.h"

enum ImgPackColorFormat {
	IMGPACK_RGBA8888,
};

struct ImgPackContext {
	enum ImgPackColorFormat colorFormat;
	int (*formatter)(struct ImgPackContext *ctx, FILE *output_file);
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

	int argc;
	char **argv;
	char *colorFormatString;
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
	return ctx->colorFormatString;
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
#include "formatters/CSV.h"
#include "formatters/JSON_ARRAY.h"
#include "formatters/JSON_HASH.h"
#include "formatters/RAYLIB.h"

static int parse_data_format(struct ImgPackContext *ctx, const char *s) {
	if (!strcmp(s, "C")) ctx->formatter = imgpack_formatter_C;
	else if (!strcmp(s, "CSV")) ctx->formatter = imgpack_formatter_CSV;
	else if (!strcmp(s, "JSON_ARRAY")) ctx->formatter = imgpack_formatter_JSON_ARRAY;
	else if (!strcmp(s, "JSON_HASH")) ctx->formatter = imgpack_formatter_JSON_HASH;
	else if (!strcmp(s, "RAYLIB")) ctx->formatter = imgpack_formatter_RAYLIB;
	else return 1;
	return 0;
}

static int parse_data_color(struct ImgPackContext *ctx, const char *s) {
	if (!strcmp(s, "RGBA8888")) ctx->colorFormat = IMGPACK_RGBA8888;
	else return 1;
	ctx->colorFormatString = (char *)s;
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
			if (file.is_dir && file.name[0] != '.') {
				if (ctx->verbose) printf("  Traverse %s\n", file.name);
				count += get_images_data(ctx, file.path);
			} else if (file.is_reg) {
				if (ctx->verbose) printf("  Reading %s\n", file.name);
				int width, height, channels;
				stbi_uc *data = stbi_load(file.path, &width, &height, &channels, 4);
				if (data) {
					add_image_data(ctx, data, file.path, width, height);
					count++;
					stbi_image_free(data);
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

static int pack_images(struct ImgPackContext *ctx) {
	int occupied_area = 0;
	int side_add = ctx->padding + ctx->extrude;
	for (int i = 0; i < ctx->size; i++) {
		occupied_area += ((side_add+ctx->packingRects[i].w)*(side_add+ctx->packingRects[i].h));
	}
	if (ctx->verbose) printf("Occupied area is %d\n", occupied_area);
	if (occupied_area <= 0) return 1;
	int assumed_side_size = (int)(1.1*sqrt((double)occupied_area));
	stbrp_node *nodes = malloc(sizeof(*nodes) * ctx->size);
	if (!nodes) return 1;
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
	return 0;
}

static int write_atlas_data(struct ImgPackContext *ctx) {
	FILE *output_file = stdout;
	if (ctx->outputDataPath) {
		output_file = fopen(ctx->outputDataPath, "w+");
	}
	int status = 1;
	if (output_file) {
		if (ctx->verbose) printf("Writing description data to \"%s\"\n", ctx->outputDataPath ? ctx->outputDataPath : "stdout");
		status = ctx->formatter(ctx, output_file);
		printf("Written\n");
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

static int parse_scale(struct ImgPackContext *ctx, const char *scale) {
	char numerator[128];
	char denominator[128] = "1";
	int i = 0, j = 0;
	int num = 1, den = 1;
	for (j = 0; j < 127 && scale[i] != '/'; i++, j++) {
		numerator[j] = scale[i];
	}
	numerator[j+1] = '\0';
	if (scale[i] == '\0') {
		num = strtol(numerator, NULL, 10);
	} else if (scale[i] == '/') {
		for (j = 0; j < 127 && scale[i] != '\0'; i++, j++) {
			denominator[j] = scale[i];
		}
		denominator[j] = '\0';
	}
	num = strtol(numerator, NULL, 10);
	den = strtol(numerator, NULL, 10);
	if (num > 0 && den > 0) {
		ctx->scaleNumerator = num;
		ctx->scaleDenominator = den;
		return 0;
	} else {
		return 1;
	}
}

int main(int argc, char *argv[]) {
	struct ImgPackContext ctx = {
		.scaleNumerator = 1,
		.scaleDenominator = 1,
		.trimThreshold = -1,
		.argc = argc,
		.argv = argv,
	};
	char *imagesPath = argv[argc-1];
	char *format_data = "C";
	char *format_color = "RGBA8888";
	char *scale = "1";
	IA_BEGIN(argc, argv, "--help", "-?", "ImgPack texture packer v0.6\n"
		"Copyright 2019 Ilya Kolbin <iskolbin@gmail.com>\n\n"
		"Usage : imgpack <OPTIONS> <images folder>\n\n"
		" Key         |    | Value  | Description\n"
		"-------------+----+--------+----------------------------------------------\n"
		" --data      | -d | string | output file path, if ommited `stdout` is used\n"
		" --image     | -i | string | output image path\n"
		" --format    | -f | string | output atlas data format\n"
		" --trim      | -t | int    | alpha threshold for trimming image with transparent border, should be 0-255\n"
		" --padding   | -p | int    | adds transparent padding\n"
		" --force-pot | -p |        | force power of two texture output\n"
		" --exturde   | -e | int    | adds copied pixels on image borders, which helps with texture bleeding\n"
		" --verbose   | -v |        | print debug messages during the packing process\n"
		" --help      | -? |        | prints this memo\n\n")
		IA_STR("--data", "-d", ctx.outputDataPath)
		IA_STR("--image", "-i", ctx.outputImagePath)
		IA_FLAG("--force-pot", "-2", ctx.forcePOT)
		IA_FLAG("--force-squared", "-sq", ctx.forceSquared)
		IA_FLAG("--multipack", "-m", ctx.allowMultipack)
		IA_INT("--trim", "-t", ctx.trimThreshold)
		IA_INT("--padding", "-p", ctx.padding)
		IA_INT("--extrude", "-e", ctx.extrude)
		IA_STR("--format", "-f", format_data) 
		IA_STR("--color", "-c", format_data) 
		IA_STR("--scale", "-s", scale);
		IA_FLAG("--verbose", "-v", ctx.verbose)
	IA_END

	if (!ctx.outputImagePath) {
		printf("Specify output image path using --image/-i <image path>");
		return 1;
	}

	if (!parse_data_format(&ctx, format_data)) {
		if (ctx.verbose) printf("Using data format %s\n", format_data);
	} else {
		printf("Bad data format \"%s\"\n", format_data);
		return 1;
	}

	if (!parse_data_color(&ctx, format_color)) {
		if (ctx.verbose) printf("Using color format %s\n", format_color);
	} else {
		printf("Bad color format \"%s\"\n", format_color);
		return 1;
	}

	if (!parse_scale(&ctx, scale)) {
		if (ctx.verbose) printf("Set scaling to %d/%d\n", ctx.scaleNumerator, ctx.scaleDenominator);
	} else {
		printf("Bad scale \"%s\"\n", scale);
		return 1;
	}

	get_images_data(&ctx, imagesPath);

	if (!pack_images(&ctx)) {
		if (ctx.verbose) printf("Images have been packed\n");
	} else {
		printf("Cannot pack images\n");
		return 1;
	}

	if (write_atlas_data(&ctx)) return 1;
	if (write_atlas_image(&ctx)) return 1;
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
