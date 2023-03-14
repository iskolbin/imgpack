#define ISLIP_VERSION "0.10.1"

/*
 * ImgPack -- simple texture packer
 *
 * Author: Ilya Kolbin
 * Source: github.com:iskolbin/imgpack
 * License: MIT, see bottom of this file
 *
 */

#include <ctype.h>
#include <inttypes.h>

#ifndef ISLIP_NOSTDLIB
#include <stdlib.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef ISLIP_MALLOC
#define ISLIP_MALLOC malloc
#endif

#ifndef ISLIP_REALLOC
#define ISLIP_REALLOC realloc
#endif

#ifndef ISLIP_FREE
#define ISLIP_FREE free
#endif

#include "external/isl_args.h"

#define STB_IMAGE_IMPLEMENTATION 
#include "external/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "external/stb_image_resize.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "external/stb_rect_pack.h"

#define CUTE_FILES_IMPLEMENTATION
#include "external/cute_files.h"

enum ImgPackColorFormat {
	IMGPACK_RGBA8888,
};

enum ImgPackNaming {
	IMGPACK_NAME_WITH_EXT,
	IMGPACK_NAME_NO_EXT,
	IMGPACK_FULL_PATH,
};

struct ImgPackImage {
	int id;
	char *name;
	char *path;
	char *ext;
	uint64_t hash;
	stbrp_rect source;
	stbi_uc *data;
	int copyOf;
};

struct ImgPackContext {
	enum ImgPackColorFormat colorFormat;
	enum ImgPackNaming naming;
	int (*formatter)(struct ImgPackContext *ctx, FILE *output_file);
	int forcePOT;
	int forceSquared;
	int allowMultipack;
	int trimThreshold;
	double sideGrowCoefficient;
	int maxWidth;
	int maxHeight;
	int padding;
	int extrude;
	int unique;
	int verbose;

	struct ImgPackImage *images;
	struct stbrp_rect *packingRects;
	int size;
	int allocated;

	int width;
	int height;
	int scaleNumerator;
	int scaleDenominator;

	char *outputImagePath;
	char *outputDataPath;
	char *name;

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
		int rid = ctx->images[id].id;
		return ctx->images[id].source.x != 0 ||
			ctx->images[id].source.y != 0 ||
			ctx->images[id].source.w != ctx->packingRects[rid].w-2*d ||
			ctx->images[id].source.h != ctx->packingRects[rid].h-2*d;
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
		int rid = ctx->images[id].id;
		if (ctx->images[id].copyOf >= 0) {
			rid = ctx->images[id].copyOf;
		}
		frame.x = ctx->packingRects[rid].x + d;
		frame.y = ctx->packingRects[rid].y + d;
		frame.w = ctx->packingRects[rid].w - 2*d;
		frame.h = ctx->packingRects[rid].h - 2*d;
	}
	return frame;
}

#include "formatters/CSV.h"
#include "formatters/JSON_ARRAY.h"
#include "formatters/JSON_HASH.h"
#include "formatters/RAYLIB.h"

static int parse_data_format(struct ImgPackContext *ctx, const char *s) {
	if (!strcmp(s, "CSV")) ctx->formatter = imgpack_formatter_CSV;
	else if (!strcmp(s, "JSON_ARRAY")) ctx->formatter = imgpack_formatter_JSON_ARRAY;
	else if (!strcmp(s, "JSON_HASH")) ctx->formatter = imgpack_formatter_JSON_HASH;
	else if (!strcmp(s, "RAYLIB")) ctx->formatter = imgpack_formatter_RAYLIB;
	else return 1;
	return 0;
}

static int parse_naming(struct ImgPackContext *ctx, const char *s) {
	if (!strcmp(s, "FULL_PATH")) ctx->naming = IMGPACK_FULL_PATH;
	else if (!strcmp(s, "NAME_NO_EXT")) ctx->naming = IMGPACK_NAME_NO_EXT;
	else if (!strcmp(s, "NAME_WITH_EXT")) ctx->naming = IMGPACK_NAME_WITH_EXT;
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
	ctx->packingRects = ISLIP_REALLOC(ctx->packingRects, next_size * sizeof(*ctx->packingRects));
	ctx->images = ISLIP_REALLOC(ctx->images, next_size * sizeof(*ctx->images));
	ctx->allocated = next_size;
}

static void add_image_data(struct ImgPackContext *ctx, stbi_uc *data, const char *img_path,
		const char *img_name, const char *img_ext, int width, int height) {
	int id = ctx->size;
	while (id >= ctx->allocated) {
		allocate_images_data(ctx);
	}
	ctx->size++;

	if (ctx->scaleNumerator != 1 || ctx->scaleDenominator != 1) {
		int resized_width = ctx->scaleNumerator * width / ctx->scaleDenominator;
		int resized_height = ctx->scaleNumerator * height / ctx->scaleDenominator;
		stbi_uc *resized_data = ISLIP_MALLOC(sizeof(*resized_data) * resized_width * resized_height * 4);
		stbir_resize_uint8(data, width, height, 0,  resized_data, resized_width, resized_height, 0, 4);
		if (ctx->verbose) printf("//  Resize \"%s\" (%d, %d) => (%d, %d)\n", img_path, width, height, resized_width, resized_height);
		stbi_image_free(data);
		data = resized_data;
		width = resized_width;
		height = resized_height;
	}

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

	// FNV hash function C99 adapation
	// for more info visit http://www.isthe.com/chongo/tech/comp/fnv/
	uint64_t hash = 0xcbf29ce484222325ULL;
	for (int y = minY; y < maxY; y++) {
		for (int x = minX; x < maxX; x++) {
			hash = (hash ^ data[4*(y*width+x)]) * 0x100000001b3ULL;
			hash = (hash ^ data[4*(y*width+x)+1]) * 0x100000001b3ULL;
			hash = (hash ^ data[4*(y*width+x)+2]) * 0x100000001b3ULL;
			hash = (hash ^ data[4*(y*width+x)+3]) * 0x100000001b3ULL;
		}
	}
	if (ctx->verbose) printf("//  Hash of \"%s\" is %" PRIx64 "\n", img_path, hash);

	char *path = ISLIP_MALLOC(strlen(img_path)+1);
	strcpy(path, img_path);
	int name_len = strlen(img_name) - strlen(img_ext);
	char *name = ISLIP_MALLOC(name_len + 1);
	name[name_len] = '\0';
	for (int i = 0; i < name_len; i++) {
		name[i] = img_name[i];
	}
	char *ext = ISLIP_MALLOC(strlen(img_ext)+1);
	strcpy(ext, img_ext);
	ctx->images[id] = (struct ImgPackImage) {
		.id = id,
		.path = path,
		.hash = hash,
		.name = name,
		.ext = ext,
		.source = (stbrp_rect) {.x = minX, .y = minY, .w = width, .h = height},
		.data = data,
		.copyOf = -1,
	};

	ctx->packingRects[id] = (stbrp_rect) {
		.id = id,
		.w = maxX - minX + 1 + 2*(ctx->padding + ctx->extrude),
		.h = maxY - minY + 1 + 2*(ctx->padding + ctx->extrude),
	};

	if (ctx->unique) {
		// TODO probably should use hashtable (stb_ds?) to avoid O(n^2)
		for (int i = id-1; i >= 0; i--) {
			if (ctx->images[i].copyOf < 0) {
				if (ctx->images[i].hash == hash) {
					// Is this enough? Well in most cases yes
					if (ctx->images[i].source.x == ctx->images[id].source.x &&
							ctx->images[i].source.y == ctx->images[id].source.y &&
							ctx->images[i].source.w == ctx->images[id].source.w &&
							ctx->images[i].source.h == ctx->images[id].source.h &&
							ctx->packingRects[i].w == ctx->packingRects[id].w &&
							ctx->packingRects[i].h == ctx->packingRects[id].h) {
						ctx->images[id].copyOf = i;
						ctx->packingRects[id].w = 0;
						ctx->packingRects[id].h = 0;
						break;
					}
				}
			}
		}
	}
	if (ctx->verbose) printf("//  Added \"%s\" %dx%d(trimmed to %dx%d)\n", path, width, height, maxX - minX + 1, maxY - minY + 1);
}

static int get_images_data(struct ImgPackContext *ctx, const char *path) {
	int count = 0;
	cf_dir_t dir;
	if (cf_dir_open(&dir, path)) {
		if (ctx->verbose) printf("// Reading images data from \"%s\" directory\n", path);
		while(dir.has_next) {
			cf_file_t file;
			cf_read_file(&dir, &file);
			if (file.is_dir && file.name[0] != '.') {
				if (ctx->verbose) printf("//  Traverse %s\n", file.name);
				count += get_images_data(ctx, file.path);
			} else if (file.is_reg) {
				if (ctx->verbose) printf("//  Reading %s\n", file.name);
				int width, height, channels;
				stbi_uc *data = stbi_load(file.path, &width, &height, &channels, 4);
				if (data) {
					add_image_data(ctx, data, file.path, file.name, file.ext, width, height);
					count++;
				}
			}
			cf_dir_next(&dir);
		}
		cf_dir_close(&dir);
		if (ctx->verbose) printf("// From %s added %d images\n", path, count);
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
	if (ctx->verbose) printf("// Occupied area is %d\n", occupied_area);
	if (occupied_area <= 0) return 1;
	int assumed_side_size = (int)sqrt((double)occupied_area);
	ctx->width = assumed_side_size;
	ctx->height = assumed_side_size;
	stbrp_node *nodes = ISLIP_MALLOC(sizeof(*nodes) * ctx->size);
	if (!nodes) return 1;
	stbrp_context rp_ctx = {0};
	do {
		if (ctx->forcePOT) {
			int pot_side_size = upper_power_of_two(assumed_side_size);
			if (pot_side_size >= (ctx->sideGrowCoefficient*assumed_side_size)) {
				assumed_side_size = pot_side_size;
			} else {
				assumed_side_size = upper_power_of_two(pot_side_size+1);
			}
		} else {
			assumed_side_size = ctx->sideGrowCoefficient*assumed_side_size;
		}

		// TODO Wrong logic, we need non-square textures implementation
		if ((ctx->maxWidth > 0 && assumed_side_size >= ctx->maxWidth) ||
				(ctx->maxHeight > 0 && assumed_side_size >= ctx->maxHeight)) {
			printf("Exceeded max size constraints %d x %d\n", ctx->maxWidth, ctx->maxHeight);
			return 1;
		}

		if (ctx->maxWidth > 0) {
			if (ctx->width < ctx->maxWidth) {
				ctx->width = ctx->maxWidth > assumed_side_size ? assumed_side_size : ctx->maxWidth;
			}
		} else {
			ctx->width = assumed_side_size;
		}
		if (ctx->maxHeight > 0) {
			if (ctx->height < ctx->maxHeight) {
				ctx->height = ctx->maxHeight > assumed_side_size ? assumed_side_size : ctx->maxHeight;
			}
		} else {
			ctx->height = assumed_side_size;
		}

		if (ctx->verbose) printf("// Trying to pack %d images into %dx%d\n", ctx->size, assumed_side_size, assumed_side_size);
		stbrp_init_target(&rp_ctx, assumed_side_size, assumed_side_size, nodes, ctx->size);
	} while (!stbrp_pack_rects(&rp_ctx, ctx->packingRects, ctx->size));
	ISLIP_FREE(nodes);
	return 0;
}

static int write_atlas_data(struct ImgPackContext *ctx) {
	FILE *output_file = stdout;
	if (ctx->outputDataPath) {
		output_file = fopen(ctx->outputDataPath, "w+");
	}
	int status = 1;
	if (output_file) {
		if (ctx->verbose) printf("// Writing description data to \"%s\"\n", ctx->outputDataPath ? ctx->outputDataPath : "stdout");
		status = ctx->formatter(ctx, output_file);
		if (ctx->verbose) printf("// Written\n");
		if (output_file != stdout) fclose(output_file);
	} else {
		printf("// Cannot open for writing \"%s\"", ctx->outputDataPath);
	}
	return status;
}

static int write_atlas_image(struct ImgPackContext *ctx) {
	unsigned char *output_data = ISLIP_MALLOC(4 * ctx->width * ctx->height);
	if (ctx->verbose) printf("// Drawing atlas image to \"%s\"\n", ctx->outputImagePath);
	for (int i = 0; i < ctx->size; i++) {
		int rid = ctx->images[i].id;
		struct stbrp_rect rect = ctx->packingRects[rid];
		if (rect.w == 0 || rect.h == 0) continue;
		int x0 = ctx->images[i].source.x, y0 = ctx->images[i].source.y;
		int d = ctx->padding + ctx->extrude;
		int w = ctx->images[i].source.w, h = ctx->images[i].source.h;
		stbi_uc *image_data = ctx->images[i].data;
		if (ctx->verbose) printf("// Drawing %s\n", ctx->images[i].path);
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
	}
	stbi_write_png(ctx->outputImagePath, ctx->width, ctx->height, 4, output_data, ctx->width*4);
	ISLIP_FREE(output_data);
	return 0;
}

static void clear_context(struct ImgPackContext *ctx) {
	for (int i = 0; i < ctx->size; i++) {
		ISLIP_FREE(ctx->images[i].path);
		stbi_image_free(ctx->images[i].data);
	}
	ISLIP_FREE(ctx->packingRects);
	ISLIP_FREE(ctx->images);
	ctx->packingRects = NULL;
	ctx->images = NULL;
	ctx->size = 0;
	ctx->allocated = 0;
}

static int parse_scale(struct ImgPackContext *ctx, const char *scale) {
	char numerator[128];
	char denominator[128] = "1";
	int i = 0, j = 0;
	int num = 1, den = 1;
	for (j = 0; j < 127 && scale[i] != '/' && scale[i] != '\0'; i++, j++) {
		numerator[j] = scale[i];
	}
	numerator[j+1] = '\0';
	if (scale[i] == '\0') {
		num = strtol(numerator, NULL, 10);
	} else if (scale[i] == '/') {
		i++;
		for (j = 0; j < 127 && scale[i] != '\0'; i++, j++) {
			denominator[j] = scale[i];
		}
		denominator[j] = '\0';
	}
	num = strtol(numerator, NULL, 10);
	den = strtol(denominator, NULL, 10);
	if (num > 0 && den > 0) {
		int gcd = 1;
		for(int k = 1; k <= num && k <= den; k++) {
			if (num % k == 0 && den % k == 0) {
				gcd = k;
			}
    }
		ctx->scaleNumerator = num / gcd;
		ctx->scaleDenominator = den / gcd;
		return 0;
	} else {
		return 1;
	}
}

static int compare_images(const void *a, const void *b) {
	return strcmp(((struct ImgPackImage *)a)->path, ((struct ImgPackImage *)b)->path);
}

static void sort_images(struct ImgPackContext *ctx) {
	qsort(ctx->images, ctx->size, sizeof(struct ImgPackImage), compare_images);
}

int main(int argc, char *argv[]) {
	struct ImgPackContext ctx = {
		.scaleNumerator = 1,
		.scaleDenominator = 1,
		.sideGrowCoefficient = 1.2,
		.trimThreshold = -1,
		.argc = argc,
		.argv = argv,
	};
	char *imagesPath = argv[argc-1];
	char *format_data = "C";
	char *format_color = "RGBA8888";
	char *scale = "1";
	char *naming = "NAME_WITH_EXT";
	int sorting = 0;
	IA_BEGIN(argc, argv, "--help", "-?", "ImgPack texture packer v0.8\n"
		"Copyright 2019 Ilya Kolbin <iskolbin@gmail.com>\n\n"
		"Usage : imgpack <OPTIONS> <images folder>\n\n"
		"| Key          |    | Value   | Description\n"
		"|--------------|----|---------|----------------------------------------------\n"
		"| --data       | -d | string  | output file path, if ommited `stdout` is used\n"
		"| --image      | -i | string  | output image path (NEEDED)\n"
		"| --name       | -n | string  | name\n"
		"| --format     | -f | string  | output atlas data format (CSV, JSON_HASH, JSON_ARRAY, RAYLIB)\n"
		"| --naming     | -N | string  | frames naming for JSON_HASH/ARRAY: FULL_PATH, NAME_NO_EXT, NAME_WITH_EXT(default)\n"
		"| --trim       | -t | int     | alpha threshold for trimming image with transparent border, should be 0-255\n"
		"| --padding    | -p | int     | adds transparent padding\n"
		"| --exturde    | -e | int     | adds copied pixels on image borders, which helps with texture bleeding\n"
		"| --max-width  | -w | int     | maximum atlas width\n"
		"| --max-height | -h | int     | maximum atlas height\n"
		"| --scale      | -x | int/int | scaling ratio int form \"A/B\" or just \"K\"\n"
		"| --color      | -c | string  | format (only RGBA8888 currently supported)\n"
		"| --unique     | -u |         | remove identical images (after trimming)\n"
		"| --force-pot  | -2 |         | force power of two texture output\n"
		"| --sort       | -s |         | sorting by path name (ascending)\n"
		//"| --multipack  | -m |         | allow multipacking\n"
		"| --verbose    | -v |         | print debug messages during the packing process\n"
		"| --help       | -? |         | prints this memo\n\n")
		IA_STR("--data", "-d", ctx.outputDataPath)
		IA_STR("--image", "-i", ctx.outputImagePath)
		IA_STR("--name", "-n", ctx.name)
		IA_STR("--format", "-f", format_data)
		IA_STR("--naming", "-N", naming)
		IA_INT("--trim", "-t", ctx.trimThreshold)
		IA_INT("--padding", "-p", ctx.padding)
		IA_INT("--extrude", "-e", ctx.extrude)
		IA_INT("--max-width", "-w", ctx.maxWidth)
		IA_INT("--max-height", "-h", ctx.maxHeight)
		IA_STR("--scale", "-x", scale)
		IA_STR("--color", "-c", format_data)
		IA_FLAG("--unique", "-u", ctx.unique)
		IA_FLAG("--force-pot", "-2", ctx.forcePOT)
		IA_FLAG("--sort", "-s", sorting)
		//IA_FLAG("--multipack", "-m", ctx.allowMultipack)
		IA_FLAG("--verbose", "-v", ctx.verbose)
		IA_FLAG("--force-squared", "-sq", ctx.forceSquared)
	IA_END

	if (!ctx.outputImagePath) {
		printf("Specify output image path using --image/-i <image path>");
		return 1;
	}

	if (!parse_data_format(&ctx, format_data)) {
		if (ctx.verbose) printf("// Using data format %s\n", format_data);
	} else {
		printf("Bad data format \"%s\"\n", format_data);
		return 1;
	}

	if (!parse_data_color(&ctx, format_color)) {
		if (ctx.verbose) printf("// Using color format %s\n", format_color);
	} else {
		printf("Bad color format \"%s\"\n", format_color);
		return 1;
	}

	if (!parse_naming(&ctx, naming)) {
		if (ctx.verbose) printf("// Using naming %s\n", naming);
	} else {
		printf("Bad naming \"%s\"\n", naming);
		return 1;
	}

	if (!parse_scale(&ctx, scale)) {
		if (ctx.verbose) printf("// Set scaling to %d/%d\n", ctx.scaleNumerator, ctx.scaleDenominator);
	} else {
		printf("Bad scale \"%s\"\n", scale);
		return 1;
	}

	if (ctx.maxWidth > 0 || ctx.maxHeight > 0) {
		if (ctx.verbose) printf("// Size constraints: %d x %d\n", ctx.maxWidth, ctx.maxHeight);
	}

	get_images_data(&ctx, imagesPath);	

	if (!pack_images(&ctx)) {
		if (ctx.verbose) printf("// Images have been packed\n");
	} else {
		printf("Cannot pack images\n");
		return 1;
	}

	if (sorting) {
		if (ctx.verbose) printf("// Sorting images\n");
		sort_images(&ctx);
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
