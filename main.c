#include <math.h>
#include <stdlib.h>
#include <dirent.h>
#include <libgen.h>

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

struct ImgPackContext {
	int forceSquare;
	int forcePOT;
	int trim;
	int allowMultipack;
	int maxWidth;
	int maxHeight;
	int padding;
	int extrude;

	char **imagePaths;
	struct stbrp_rect *imageTrimmedRects;
	struct stbrp_rect *imageRects;
	int imagesUsed;
	int imagesAllocated;
	int width;
	int height;

	char *outputImagePath;
	char *outputDescPath;
	char *idPrefix;
};

static void allocate_images_data(struct ImgPackContext *ctx) {
	int nextSize = ctx->imagesAllocated * 2;
	if (nextSize == 0) nextSize = 16;
	ctx->imagePaths = realloc(ctx->imagePaths, nextSize * sizeof(*ctx->imagePaths));
	ctx->imageRects = realloc(ctx->imageRects, nextSize * sizeof(*ctx->imageRects));
	ctx->imageTrimmedRects = realloc(ctx->imageRects, nextSize * sizeof(*ctx->imageTrimmedRects));
	ctx->imagesAllocated = nextSize;
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
		ctx->imageTrimmedRects[id] = (stbrp_rect) {
			.x = minX,
			.y = minY,
			.w = maxX-minX,
			.h = maxY-minY,
		};
	}
	char *path = malloc(strlen(imgPath)+1);
	strcpy(path, imgPath);
	ctx->imagePaths[id] = path;
	if (ctx->trim) {
		ctx->imageRects[id] = (stbrp_rect) {
			.id = id,
			.w = maxX - minX + 2*(ctx->padding + ctx->extrude),
			.h = maxY - minY + 2*(ctx->padding + ctx->extrude),
		};
		printf("  Trim \"%s\" %dx%d => %dx%d\n", imgPath, width, height, maxX - minX, maxY - minY);
	} else {
		ctx->imageRects[id] = (stbrp_rect) {
			.id = id,
			.w = width + 2*(ctx->padding + ctx->extrude),
			.h = height + 2*(ctx->padding + ctx->extrude),
		};
	}
	printf("  Added image \"%s\": %dx%d\n", imgPath, width, height);
}

static int get_images_data(struct ImgPackContext *ctx, const char *dirPath) {
	char buffer[1024];
	struct dirent *ent;
	int count = 0;
	DIR *dir = opendir(dirPath);
	if (dir) {
		printf("Reading images data from \"%s\" directory\n", dirPath);
		while((ent = readdir(dir)) != NULL) {
			//if (get_images_data(ctx, ent->d_name) == -1) {
				int width, height, channels;
				printf("  Reading %s\n", ent->d_name);
				strcpy(buffer, dirPath);
				strcat(buffer, "/");
				strcat(buffer, ent->d_name);
				stbi_uc *data = stbi_load(buffer, &width, &height, &channels, 4);
				if (data) {
					add_image_data(ctx, data, buffer, width, height);
					count++;
					stbi_image_free(data);
				}
			//}
		}
		closedir(dir);
		printf("From %s added %d images\n", dirPath, count);
		return count;
	} else {
		return -1;
	}
}

unsigned long upper_power_of_two(unsigned long v) {
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
	printf("Occupied area is %d\n", occupied_area);
	int assumed_side_size = (int)(1.25*sqrt((double)occupied_area));
	if (ctx->forcePOT) {
		assumed_side_size = upper_power_of_two(assumed_side_size);
	}
	ctx->width = assumed_side_size;
	ctx->height = assumed_side_size;
	stbrp_context rp_ctx = {0};
	stbrp_node *nodes = malloc(sizeof(*nodes) * ctx->imagesUsed);
	printf("Trying to pack %d images into %dx%d\n", ctx->imagesUsed, assumed_side_size, assumed_side_size);
	stbrp_init_target(&rp_ctx, assumed_side_size, assumed_side_size, nodes, ctx->imagesUsed);
	stbrp_pack_rects(&rp_ctx, ctx->imageRects, ctx->imagesUsed);
	free(nodes);
}

static void write_atlas_desc(struct ImgPackContext *ctx) {
	FILE *outputFile = fopen(ctx->outputDescPath, "w+");
	if (outputFile) {
		printf("Writing description data to \"%s\"\n", ctx->outputDescPath);
		fprintf(outputFile, "enum %sIds = {\n", ctx->idPrefix);
		for (int i = 0; i < ctx->imagesUsed; i++) {
			char *name = basename(ctx->imagePaths[i]);
			char buffer[1024];
			int j = 0;
			for (j = 0; name[i]; j++) {
				if (name[j] == '.') break;
				else if (name[j] >= 'a' && name[j] <= 'z') buffer[j] = name[j]-32;
				else buffer[j] = name[j];
			}
			buffer[j] = '\0';
			fprintf(outputFile, "\t%s%s = %d,\n", ctx->idPrefix, buffer, i);
		}
		fprintf(outputFile, "};\n\n");
		fprintf(outputFile, "static stbrp_rect %sRects[%d] = {\n", ctx->idPrefix, ctx->imagesUsed);
		for (int i = 0; i < ctx->imagesUsed; i++) {
			fprintf(outputFile, "\t{%d, %d, %d, %d, %d},\n", i, ctx->imageRects[i].x, ctx->imageRects[i].y, ctx->imageRects[i].w, ctx->imageRects[i].h);
		}
		fprintf(outputFile, "};");
		fclose(outputFile);
	}
}

static void write_atlas_image(struct ImgPackContext *ctx) {
	char *rgba = malloc(4 * ctx->width * ctx->height);
	printf("Drawing atlas image to \"%s\"\n", ctx->outputImagePath);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		struct stbrp_rect trimmedRect = ctx->imageTrimmedRects[i];
		struct stbrp_rect rect = ctx->imageRects[i];
		int w, h, n;
		stbi_uc *data = stbi_load(ctx->imagePaths[i], &w, &h, &n, 4);
		if (!ctx->trim) {
			trimmedRect = (stbrp_rect) {.x = 0, .y = 0, .w = w, .h = h};
		}
		for (int y = 0; y < trimmedRect.h; y++) {
			for (int x = 0; x < trimmedRect.w; x++) {
				int offset = 4*(x+rect.x+(y+rect.y)*ctx->width);
				for (int j = 0; j < 4; j++) {
					rgba[offset+j] = data[4*(x+trimmedRect.x+(y+trimmedRect.y)*w)+j];
				}
			}
		}
		stbi_image_free(data);
	}
	stbi_write_png(ctx->outputImagePath, ctx->width, ctx->height, 4, rgba, ctx->width*4);
	free(rgba);
}

static void clear_context(struct ImgPackContext *ctx) {
	for (int i = 0; i < ctx->imagesUsed; i++) {
		free(ctx->imagePaths[i]);
	};
	free(ctx->imagePaths);
	free(ctx->imageRects);
	free(ctx->imageTrimmedRects);
	ctx->imagePaths = NULL;
	ctx->imageRects = NULL;
	ctx->imagesUsed = 0;
	ctx->imagesAllocated = 0;
}

int main(int argc, char *argv[]) {
	struct ImgPackContext ctx = {0};
	char *imagesPath = argv[argc-1];
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--name")) {
			ctx.idPrefix = argv[++i];
		} else if (!strcmp(argv[i], "--out-desc")) {
			ctx.outputDescPath = argv[++i];
		} else if (!strcmp(argv[i], "--out-image")) {
			ctx.outputImagePath = argv[++i];
		} else if (!strcmp(argv[i], "--force-pot")) {
			ctx.forcePOT = 1;
		} else if (!strcmp(argv[i], "--trim")) {
			ctx.trim = 1;
		}
	}

	get_images_data(&ctx, imagesPath);
	pack_images(&ctx);
	if (ctx.outputDescPath) write_atlas_desc(&ctx);
	if (ctx.outputImagePath) write_atlas_image(&ctx);
	clear_context(&ctx);
	return 0;
}
