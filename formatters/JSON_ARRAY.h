static int imgpack_formatter_JSON_ARRAY(struct ImgPackContext *ctx, FILE *f) {
	fprintf(f, "{\"frames\": [\n\n");
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "{\n");
		switch (ctx->naming) {
			case IMGPACK_FULL_PATH: fprintf(f, "\t\"filename\": \"%s\",\n", ctx->images[i].path); break;
			case IMGPACK_NAME_NO_EXT: fprintf(f, "\t\"filename\": \"%s\",\n", ctx->images[i].name); break;
			case IMGPACK_NAME_WITH_EXT: fprintf(f, "\t\"filename\": \"%s%s\",\n", ctx->images[i].name, ctx->images[i].ext); break;
		}
		fprintf(f, "\t\"frame\": {\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},\n", frame.x, frame.y, frame.w, frame.h);
		fprintf(f, "\t\"rotated\": %s,\n", is_image_rotated(ctx, i) ? "true" : "false");
		fprintf(f, "\t\"trimmed\": %s,\n", is_image_trimmed(ctx, i) ? "true" : "false");
		fprintf(f, "\t\"spriteSourceSize\": {\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d},\n", ctx->images[i].source.x, ctx->images[i].source.y, frame.w, frame.h);
		fprintf(f, "\t\"sourceRects\": {\"w\":%d,\"h\":%d},\n", ctx->images[i].source.w, ctx->images[i].source.h);
		fprintf(f, "\t\"pivot\": {\"x\":0.5,\"y\":0.5}\n");
		fprintf(f, "}%s\n", i == ctx->size-1 ? "]," : ",");
	}
	fprintf(f, "\"meta\": {\n");
	fprintf(f, "\t\"app\": \"https://github.com/iskolbin/imgpack\",\n");
	fprintf(f, "\t\"version\": \"%s\",\n", ISLIP_VERSION);
	fprintf(f, "\t\"image\": \"%s\",\n", ctx->outputImagePath);
	fprintf(f, "\t\"format\": \"%s\",\n", get_output_image_format(ctx));
	fprintf(f, "\t\"size\": {\"w\":%d,\"h\":%d},\n", ctx->width, ctx->height);
	fprintf(f, "\t\"scale\": \"%.5g\"\n", ((1.0*ctx->scaleNumerator)/ctx->scaleDenominator));
	fprintf(f, "}\n}");
	return 0;
}
