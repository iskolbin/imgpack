static int imgpack_formatter_C(struct ImgPackContext *ctx, FILE *f) {
	fprintf(f, "enum %sIds = {\n", ctx->name);
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
		fprintf(f, "\t%s%s = %d,\n", ctx->name, buffer, i);
	}
	fprintf(f, "};\n\n");
	fprintf(f, "static const char * %sPaths[%d] = {\n", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		fprintf(f, "\t\"%s\",\n", ctx->imagePaths[i]);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static stbrp_rect %sRects[%d] = {\n", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "\t{%d, %d, %d, %d, %d},\n", i, frame.x, frame.y, frame.w, frame.h);
	}
	fprintf(f, "};");
	return 0;
}
