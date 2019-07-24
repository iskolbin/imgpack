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
	int forceSquare;
	int forcePOT;
	int trim;
	int allowMultipack;
	int maxWidth;
	int maxHeight;
	int padding;
	int extrude;
	int verbose;

	char **imagePaths;
	int *imageOffsetX;
	int *imageOffsetY;
	struct stbrp_rect *imageRects;
	int imagesUsed;
	int imagesAllocated;
	int width;
	int height;

	char *outputTexturePath;
	char *outputDataPath;
	char *idPrefix;
};

static void allocate_images_data(struct ImgPackContext *ctx) {
	int next_size = ctx->imagesAllocated * 2;
	if (next_size == 0) next_size = 16;
	ctx->imagePaths = realloc(ctx->imagePaths, next_size * sizeof(*ctx->imagePaths));
	ctx->imageRects = realloc(ctx->imageRects, next_size * sizeof(*ctx->imageRects));
	ctx->imageOffsetX = realloc(ctx->imageOffsetX, next_size * sizeof(*ctx->imageOffsetX));
	ctx->imageOffsetY = realloc(ctx->imageOffsetY, next_size * sizeof(*ctx->imageOffsetY));
	ctx->imagesAllocated = next_size;
}

static void add_image_data(struct ImgPackContext *ctx, stbi_uc *data, const char *imgPath, int width, int height) {
	int id = ctx->imagesUsed;
	while (id >= ctx->imagesAllocated) {
		allocate_images_data(ctx);
	}
	ctx->imagesUsed++;
	int minY = 0, minX = 0, maxY = height-1, maxX = width-1;
	if (ctx->trim) {
		for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) if (data[4*(y*width+x)+3] != 0) {
			minY = y;
			y = height;
			break;
		}
		for (int y = height-1; y >= 0; y--) for (int x = 0; x < width; x++) if (data[4*(y*width+x)+3] != 0) {
			maxY = y;
			y = -1;
			break;
		}
		for (int x = 0; x < width; x++) for (int y = 0; y < height; y++) if (data[4*(y*width+x)+3] != 0) {
			minX = x;
			x = width;
			break;
		}
		for (int x = width-1; x >= 0; x--) for (int y = 0; y < height; y++) if (data[4*(y*width+x)+3] != 0) {
			maxX = x;
			x = -1;
			break;
		}
		ctx->imageOffsetX[id] = minX;
		ctx->imageOffsetY[id] = minY;
	} else {
		ctx->imageOffsetX[id] = 0;
		ctx->imageOffsetY[id] = 0;
	}
	char *path = malloc(strlen(imgPath)+1);
	strcpy(path, imgPath);
	ctx->imagePaths[id] = path;
	ctx->imageRects[id] = (stbrp_rect) {
		.id = id,
		.w = maxX - minX + 1 + 2*(ctx->padding + ctx->extrude),
		.h = maxY - minY + 1 + 2*(ctx->padding + ctx->extrude),
	};
	if (ctx->verbose) printf("  Added \"%s\" %dx%d(trimmed to %dx%d)\n", imgPath, width, height, maxX - minX + 1, maxY - minY + 1);
}

static int get_images_data(struct ImgPackContext *ctx, const char *dirPath) {
	char buffer[512];
	int count = 0;
	cf_dir_t dir;
	if (cf_dir_open(&dir, dirPath)) {
		if (ctx->verbose) printf("Reading images data from \"%s\" directory\n", dirPath);
		while(dir.has_next) {
			//if (get_images_data(ctx, ent->d_name) == -1) {
				int width, height, channels;
				cf_file_t file;
				cf_read_file (&dir, &file);
				if (ctx->verbose) printf("  Reading %s\n", file.name);
				strcpy(buffer, dirPath);
				strcat(buffer, "/");
				strcat(buffer, file.name);
				stbi_uc *data = stbi_load(buffer, &width, &height, &channels, 4);
				if (data) {
					add_image_data(ctx, data, buffer, width, height);
					count++;
					stbi_image_free(data);
				}
				cf_dir_next(&dir);
			//}
		}
		cf_dir_close(&dir);
		if (ctx->verbose) printf("From %s added %d images\n", dirPath, count);
		return count;
	} else {
		return -1;
	}
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
	for (int i = 0; i < ctx->imagesUsed; i++) {
		occupied_area += ((side_add+ctx->imageRects[i].w)*(side_add+ctx->imageRects[i].h));
	}
	if (ctx->verbose) printf("Occupied area is %d\n", occupied_area);
	int assumed_side_size = (int)(1.1*sqrt((double)occupied_area));
	stbrp_node *nodes = malloc(sizeof(*nodes) * ctx->imagesUsed);
	if (ctx->forcePOT) {
		assumed_side_size = upper_power_of_two(assumed_side_size);
	}
	ctx->width = assumed_side_size;
	ctx->height = assumed_side_size;
	stbrp_context rp_ctx = {0};
	if (ctx->verbose) printf("Trying to pack %d images into %dx%d\n", ctx->imagesUsed, assumed_side_size, assumed_side_size);
	stbrp_init_target(&rp_ctx, assumed_side_size, assumed_side_size, nodes, ctx->imagesUsed);
	stbrp_pack_rects(&rp_ctx, ctx->imageRects, ctx->imagesUsed);
	free(nodes);
}

static void write_atlas_data(struct ImgPackContext *ctx) {
	FILE *output_file = fopen(ctx->outputDataPath, "w+");
	if (output_file) {
		if (ctx->verbose) printf("Writing description data to \"%s\"\n", ctx->outputDataPath);
		fprintf(output_file, "enum %sIds = {\n", ctx->idPrefix);
		for (int i = 0; i < ctx->imagesUsed; i++) {
			char *image_path = ctx->imagePaths[i];
			int point_pos, start_pos;
			for (point_pos = strlen(image_path)-1; point_pos > 0 && image_path[point_pos] != '.'; point_pos--) {}
			for (start_pos = point_pos-1; start_pos > 0 && (isalnum(image_path[start_pos-1]) || image_path[start_pos-1] == '_'); start_pos--) {}
			char buffer[512];
			int j;
			for (j = 0; j < point_pos-start_pos; j++) {
				char ch = image_path[start_pos+j];
				if (ch >= 'a' && ch <= 'z') buffer[j] = ch - 32;
				else buffer[j] = ch;
			}
			buffer[j] = '\0';
			fprintf(output_file, "\t%s%s = %d,\n", ctx->idPrefix, buffer, i);
		}
		fprintf(output_file, "};\n\n");
		fprintf(output_file, "static const char * %sPaths[%d] = {\n", ctx->idPrefix, ctx->imagesUsed);
		for (int i = 0; i < ctx->imagesUsed; i++) {
			fprintf(output_file, "\t\"%s\",\n", ctx->imagePaths[i]);
		}
		fprintf(output_file, "};\n\n");

		fprintf(output_file, "static stbrp_rect %sRects[%d] = {\n", ctx->idPrefix, ctx->imagesUsed);
		for (int i = 0; i < ctx->imagesUsed; i++) {
			fprintf(output_file, "\t{%d, %d, %d, %d, %d},\n", i, ctx->imageRects[i].x, ctx->imageRects[i].y, ctx->imageRects[i].w, ctx->imageRects[i].h);
		}
		fprintf(output_file, "};");
		fclose(output_file);
	}
}

static void write_atlas_texture(struct ImgPackContext *ctx) {
	unsigned char *output_data = malloc(4 * ctx->width * ctx->height);
	if (ctx->verbose) printf("Drawing atlas image to \"%s\"\n", ctx->outputTexturePath);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		struct stbrp_rect rect = ctx->imageRects[i];
		int x0 = ctx->imageOffsetX[i], y0 = ctx->imageOffsetY[i];
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
	stbi_write_png(ctx->outputTexturePath, ctx->width, ctx->height, 4, output_data, ctx->width*4);
	free(output_data);
}

static void clear_context(struct ImgPackContext *ctx) {
	for (int i = 0; i < ctx->imagesUsed; i++) {
		free(ctx->imagePaths[i]);
	};
	free(ctx->imagePaths);
	free(ctx->imageRects);
	free(ctx->imageOffsetX);
	free(ctx->imageOffsetY);
	ctx->imagePaths = NULL;
	ctx->imageRects = NULL;
	ctx->imageOffsetX = NULL;
	ctx->imageOffsetY = NULL;
	ctx->imagesUsed = 0;
	ctx->imagesAllocated = 0;
}

int main(int argc, char *argv[]) {
	struct ImgPackContext ctx = {0};
	char *imagesPath = argv[argc-1];
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--name")) {
			ctx.idPrefix = argv[++i];
		} else if (!strcmp(argv[i], "--data")) {
			ctx.outputDataPath = argv[++i];
		} else if (!strcmp(argv[i], "--texture")) {
			ctx.outputTexturePath = argv[++i];
		} else if (!strcmp(argv[i], "--force-pot")) {
			ctx.forcePOT = 1;
		} else if (!strcmp(argv[i], "--trim")) {
			ctx.trim = 1;
		} else if (!strcmp(argv[i], "--padding")) {
			ctx.padding = strtol(argv[++i], NULL, 10);
		} else if (!strcmp(argv[i], "--extrude")) {
			ctx.extrude = strtol(argv[++i], NULL, 10);
		} else if (!strcmp(argv[i], "--verbose")) {
			ctx.verbose = 1;
		}
	}

	get_images_data(&ctx, imagesPath);
	pack_images(&ctx);
	if (ctx.outputDataPath) write_atlas_data(&ctx);
	if (ctx.outputTexturePath) write_atlas_texture(&ctx);
	clear_context(&ctx);
	return 0;
}
