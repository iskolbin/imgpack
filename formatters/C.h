static int imgpack_formatter_C(struct ImgPackContext *ctx, FILE *f) {
	char *path = ctx->outputImagePath;
	char name[128];
	for (int i = 0; path[i] && i < 127; i++) {
		name[i] = path[i];
		if (!isalnum(path[i]) && path[i] != '_') {
			name[i] = '_';
			name[i+1] = '\0';
			break;
		} else if (path[i] >= 'a' && path[i] <= 'z') {
			name[i] -= 32;
		}
	}
	fprintf(f, "enum %sIds {\n", name);
	for (int i = 0; i < ctx->size; i++) {
		char *image_path = ctx->images[i].path;
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
		fprintf(f, "\t%s%s = %d,\n", name, buffer, i);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const char *%sImage = \"%s\";\n\n", name, ctx->outputImagePath);

	fprintf(f, "static const int %sScale[2] = {%d, %d};\n\n", name, ctx->scaleNumerator, ctx->scaleDenominator);

	fprintf(f, "static const int %sFrame[%d][4] = {\n", name, ctx->size);
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "\t{%d, %d, %d, %d},\n", frame.x, frame.y, frame.w, frame.h);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const int %sSourceSize[%d][2] = {\n", name, ctx->size);
	for (int i = 0; i < ctx->size; i++) {
		fprintf(f, "\t{%d, %d},\n", ctx->images[i].source.w, ctx->images[i].source.h);
	}
	fprintf(f, "};\n");
	return 0;
}
