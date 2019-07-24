static int imgpack_formatter_C(struct ImgPackContext *ctx, FILE *f) {
	fprintf(f, "enum %sIds {\n", ctx->name);
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

	fprintf(f, "static const char *%sImage = \"%s\";\n\n", ctx->name, ctx->outputImagePath);

	fprintf(f, "static const int %sFrame[%d][6] = {\n", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "\t{%d, %d, %d, %d},\n", frame.x, frame.y, frame.w, frame.h);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const int %sSourceSize[%d][2] = {\n", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) {
		fprintf(f, "\t{%d, %d},\n", ctx->imageSourceW[i], ctx->imageSourceH[i]);
	}
	fprintf(f, "};\n");
/*
	fprintf(f, "static const int %sY[%d] = {", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) fprintf(f, "\t%d,\n", get_frame_rect(ctx, i).y);
	fprintf(f, "};\n");

	fprintf(f, "static const int %sW[%d] = {", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) fprintf(f, "%d,", get_frame_rect(ctx, i).w);
	fprintf(f, "};\n");

	fprintf(f, "static const int %sH[%d] = {", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) fprintf(f, "%d,", get_frame_rect(ctx, i).h);
	fprintf(f, "};\n");
	
	fprintf(f, "static const int %sSourceW[%d] = {", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) fprintf(f, "%d,", ctx->imageSourceW[id]);
	fprintf(f, "};\n");

	fprintf(f, "static const int %sSourceH[%d] = {", ctx->name, ctx->imagesUsed);
	for (int i = 0; i < ctx->imagesUsed; i++) fprintf(f, "%d,", ctx->imageSourceH[id]);
	fprintf(f, "};");
	*/
	return 0;
}
