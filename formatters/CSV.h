static int imgpack_formatter_CSV(struct ImgPackContext *ctx, FILE *f) {
	fprintf(f, "id, path, x, y, width, height, src_width, src_height, image, format, scale\n"); 
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "%d, \"%s\", %d, %d, %d, %d, %d, %d, \"%s\", \"%s\", \"%.5g\"\n", i,
				ctx->images[i].path, frame.x, frame.y, frame.w, frame.h, ctx->images[i].source.w, ctx->images[i].source.h,
				ctx->outputImagePath, get_output_image_format(ctx), ((1.0*ctx->scaleNumerator)/ctx->scaleDenominator));
	}
	return 0;
}
