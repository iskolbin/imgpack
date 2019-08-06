static int imgpack_formatter_RAYLIB(struct ImgPackContext *ctx, FILE *f) {
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
		fprintf(f, "  %s%s = %d,\n", name, buffer, i);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const char *%sImagePath = \"%s\";\n\n", name, ctx->outputImagePath);

	fprintf(f, "static const Vector2 %sScale = {%d, %d};\n\n", name, ctx->scaleNumerator, ctx->scaleDenominator);
	
	fprintf(f, "static const Rectangle %sFrame[%d] = {\n", name, ctx->size);
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "  {%d, %d, %d, %d},\t/* %d (%s) */\n", frame.x, frame.y, frame.w, frame.h, i, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const Vector2 %sOffset[%d] = {\n", name, ctx->size);
	for (int i = 0; i < ctx->size; i++) {
		fprintf(f, "  {%d, %d},\t/* %d (%s) */\n", ctx->images[i].source.x, ctx->images[i].source.y, i, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const Vector2 %sSourceSize[%d] = {\n", name, ctx->size);
	for (int i = 0; i < ctx->size; i++) {
		fprintf(f, "  {%d, %d},\t/* %d (%s) */\n", ctx->images[i].source.w, ctx->images[i].source.h, i, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "Texture %sTexture = {0};\n\n", name);

	fprintf(f, "void %sLoad(void) {\n", name);
	fprintf(f, "  %sTexture = LoadTexture(%sImagePath);\n", name, name);
	fprintf(f, "}\n\n");

	fprintf(f, "void %sUnload(void) {\n", name);
	fprintf(f, "  UnloadTexture(%sTexture);\n", name);
	fprintf(f, "  %sTexture = (Texture) {0};\n", name);
	fprintf(f, "}\n\n");

	fprintf(f, "void %sDrawEx(enum %sIds id, float x, float y, float xscl, float yscl, float rot, Color color) {\n", name, name);
	fprintf(f, "  Rectangle dest = %sFrame[id];\n", name);
	fprintf(f, "  Vector2 offset = %sOffset[id];\n", name);
	fprintf(f, "  Vector2 origin = %sSourceSize[id];\n", name);
	fprintf(f, "  float w = dest.width * xscl, h = dest.height * yscl;\n");
	fprintf(f, "  origin.x = 0.5*(origin.x+w-dest.width); origin.y = 0.5*(origin.y+h-dest.height);\n");
	fprintf(f, "  dest.x = x + offset.x; dest.y = y + offset.y; dest.width *= xscl; dest.height *= yscl;\n");
	fprintf(f, "  DrawTexturePro(%sTexture, %sFrame[id], dest, origin, rot, color);\n", name, name);
	fprintf(f, "}\n\n");

	fprintf(f, "void %sDraw(enum %sIds id, float x, float y, Color color) {\n", name, name);
	fprintf(f, "  Rectangle dest = %sFrame[id];\n", name);
	fprintf(f, "  Vector2 offset = %sOffset[id];\n", name);
	fprintf(f, "  dest.x = x + offset.x; dest.y = y + offset.y;\n");
	fprintf(f, "  DrawTexturePro(%sTexture, %sFrame[id], dest, (Vector2){0,0}, 0, color);\n", name, name);
	fprintf(f, "}\n");
	return 0;
}
