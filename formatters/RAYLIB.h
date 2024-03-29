static void imgpack_formatter_RAYLIB__generate_name(const char *image_path, char *out_buffer) {
	int j, point_pos, start_pos;
	for (point_pos = strlen(image_path)-1; point_pos > 0 && image_path[point_pos] != '.'; point_pos--) {}
	for (start_pos = point_pos-1; start_pos > 0 && (isalnum(image_path[start_pos-1]) || image_path[start_pos-1] == '_'); start_pos--) {}
	for (j = 0; j < point_pos-start_pos; j++) {
		char ch = image_path[start_pos+j];
		if (ch >= 'a' && ch <= 'z') out_buffer[j] = ch - 32;
		else out_buffer[j] = ch;
	}
	out_buffer[j] = '\0';
}

static int imgpack_formatter_RAYLIB(struct ImgPackContext *ctx, FILE *f) {
	char *name = ctx->name;
	fprintf(f, "#ifndef %s_H_\n", name);
	fprintf(f, "#define %s_H_\n", name);
	fprintf(f, "/* Generated by imgpack %s */\n", ISLIP_VERSION);
	fprintf(f, "#ifndef %s_STRCMP\n", name);
	fprintf(f, "int strcmp(const char *, const char *);\n");
	fprintf(f, "#define %s_STRCMP strcmp\n", name);
	fprintf(f, "#endif\n");
	fprintf(f, "enum %s_Id {\n", name);
	fprintf(f, "  %s_NONE = 0,\n", name);
	for (int i = 0; i < ctx->size; i++) {
		char buffer[512];
		imgpack_formatter_RAYLIB__generate_name(ctx->images[i].path, buffer);
		fprintf(f, "  %s_%s = %d,\n", name, buffer, i+1);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "#define %s_PATH \"%s\"\n\n", name, ctx->outputImagePath);

	fprintf(f, "#ifndef %s_DEF\n", name);
	fprintf(f, "#define %s_DEF static\n", name);
	fprintf(f, "#endif\n\n");

	fprintf(f, "#ifdef %s_NAMESPACED_ANCHOR\n", name);
	fprintf(f, "#define %s_CENTER 0\n", name);
	fprintf(f, "#define %s_LEFT 1\n", name);
	fprintf(f, "#define %s_RIGHT 2\n", name);
	fprintf(f, "#define %s_TOP 4\n", name);
	fprintf(f, "#define %s_BOTTOM 8\n", name);
	fprintf(f, "#else\n");
	fprintf(f, "#ifndef CENTER\n#define CENTER 0\n#endif\n");
	fprintf(f, "#ifndef LEFT\n#define LEFT 1\n#endif\n");
	fprintf(f, "#ifndef RIGHT\n#define RIGHT 2\n#endif\n");
	fprintf(f, "#ifndef TOP\n#define TOP 4\n#endif\n");
	fprintf(f, "#ifndef BOTTOM\n#define BOTTOM 8\n#endif\n");
	fprintf(f, "#endif\n");

	fprintf(f, "%s_DEF void %s_Load(void);\n", name, name);
	fprintf(f, "%s_DEF void %s_Unload(void);\n", name, name);
	fprintf(f, "%s_DEF int %s_Draw(enum %s_Id id, float x, float y, Color color, int anchor, const Vector2 *point);\n", name, name, name);
	fprintf(f, "%s_DEF int %s_DrawEx(enum %s_Id id, float x, float y, float rotation, float scale, Color color, int anchor, const Vector2 *point);\n", name, name, name);
	fprintf(f, "%s_DEF Texture %s_GetTexture(void);\n", name, name);
	fprintf(f, "%s_DEF const Vector2 %s_GetScale(void);\n", name, name);
	fprintf(f, "%s_DEF const Rectangle %s_GetFrame(enum %s_Id id);\n", name, name, name);
	fprintf(f, "%s_DEF const Vector2 %s_GetOffset(enum %s_Id id);\n", name, name, name);
	fprintf(f, "%s_DEF const Vector2 %s_GetSourceSize(enum %s_Id id);\n", name, name, name);
	fprintf(f, "%s_DEF const Vector2 %s_GetOrigin(enum %s_Id id);\n", name, name, name);
	fprintf(f, "%s_DEF enum %s_Id %s_IdFromPath(const char *path);\n", name, name, name);
	fprintf(f, "%s_DEF enum %s_Id %s_IdFromStr(const char* str);\n", name, name, name);
	fprintf(f, "%s_DEF const char* %s_StrId(enum %s_Id id);\n", name, name, name);
	fprintf(f, "\n");
	fprintf(f, "#endif\n\n");

	fprintf(f, "#ifdef %s_IMPLEMENTATAION\n", name);
	fprintf(f, "#ifndef %s_IMPLEMENTATAION_ONCE\n", name);
	fprintf(f, "#define %s_IMPLEMENTATAION_ONCE\n", name);

	fprintf(f, "static const Vector2 %s_Scale = {%d, %d};\n\n", name, ctx->scaleNumerator, ctx->scaleDenominator);
	
	fprintf(f, "static const Rectangle %s_Frame[%d] = {\n  {0, 0, 0, 0},\t /* (NONE) */\n", name, ctx->size+1);
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect frame = get_frame_rect(ctx, i);
		fprintf(f, "  {%d, %d, %d, %d},\t/* %d (%s) */\n", frame.x, frame.y, frame.w, frame.h, i + 1, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const Vector2 %s_Offset[%d] = {\n  {0, 0},\t /* (NONE) */\n", name, ctx->size+1);
	for (int i = 0; i < ctx->size; i++) {
		fprintf(f, "  {%d, %d},\t/* %d (%s) */\n", ctx->images[i].source.x, ctx->images[i].source.y, i + 1, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const Vector2 %s_SourceSize[%d] = {\n  {0, 0},\t /* (NONE) */\n", name, ctx->size+1);
	for (int i = 0; i < ctx->size; i++) {
		fprintf(f, "  {%d, %d},\t/* %d (%s) */\n", ctx->images[i].source.w, ctx->images[i].source.h, i + 1, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static const Vector2 %s_Origin[%d] = {\n  {0, 0},\t /* (NONE) */\n", name, ctx->size+1);
	for (int i = 0; i < ctx->size; i++) {
		struct stbrp_rect source = ctx->images[i].source;
		fprintf(f, "  {%d, %d},\t/* %d (%s) */\n", source.w/2 - source.x, source.h/2 - source.y, i + 1, ctx->images[i].path);
	}
	fprintf(f, "};\n\n");

	fprintf(f, "static Texture %s_Texture = {0};\n\n", name);

	fprintf(f, "void %s_Load(void) {\n", name);
	fprintf(f, "  %s_Texture = LoadTexture(%s_PATH);\n", name, name);
	fprintf(f, "}\n\n");

	fprintf(f, "void %s_Unload(void) {\n", name);
	fprintf(f, "  UnloadTexture(%s_Texture);\n", name);
	fprintf(f, "  %s_Texture = (Texture) {0};\n", name);
	fprintf(f, "}\n\n");

	fprintf(f, "enum %s_Id %s_IdFromPath(const char *path) {\n", name, name);
	for (int i = 0; i < ctx->size; i++) {
		char buffer[512];
		imgpack_formatter_RAYLIB__generate_name(ctx->images[i].path, buffer);
		fprintf(f, "  if (%s_STRCMP(path, \"%s\") == 0) return %s_%s;\n", name, ctx->images[i].path, name, buffer);
	}
	fprintf(f, "  return %s_NONE;\n", name);
	fprintf(f, "}\n\n");

	fprintf(f, "enum %s_Id %s_IdFromStr(const char *id_str) {\n", name, name);
	for (int i = 0; i < ctx->size; i++) {
		char buffer[512];
		imgpack_formatter_RAYLIB__generate_name(ctx->images[i].path, buffer);
		fprintf(f, "  if (%s_STRCMP(id_str, \"%s_%s\") == 0) return %s_%s;\n", name, name, buffer, name, buffer);
	}
	fprintf(f, "  return %s_NONE;\n", name);
	fprintf(f, "}\n\n");

	fprintf(f, "const char* %s_StrId(enum %s_Id id) {\n", name, name);
	fprintf(f, "  switch(id) {\n");
	fprintf(f, "  case %s_NONE: return %s_NONE;\n", name, name);
	for (int i = 0; i < ctx->size; i++) {
		char buffer[512];
		imgpack_formatter_RAYLIB__generate_name(ctx->images[i].path, buffer);
		fprintf(f, "    case %s_%s: return \"%s_%s\";\n", name, buffer, name, buffer);
	}
	fprintf(f, "  }\n");
	fprintf(f, "  return \"%s_NONE\";\n", name);
	fprintf(f, "}\n\n");

	fprintf(f, "int %s_Draw(enum %s_Id id, float x, float y, Color color, int anchor, const Vector2 *point) {\n", name, name);
	fprintf(f, "  if (id) {\n");
	fprintf(f, "    x += (anchor & 1 ? 0 : anchor & 2 ? -%s_SourceSize[id].x : -%s_Origin[id].x - %s_Offset[id].x);\n", name, name, name);
	fprintf(f, "    y += (anchor & 4 ? 0 : anchor & 8 ? -%s_SourceSize[id].y : -%s_Origin[id].y - %s_Offset[id].y);\n", name, name, name);
	fprintf(f, "    Rectangle destRec = {x + %s_Offset[id].x, y + %s_Offset[id].y, %s_Frame[id].width, %s_Frame[id].height};\n", name, name, name, name);
	fprintf(f, "    DrawTexturePro(%s_Texture, %s_Frame[id], destRec, (Vector2){0,0}, 0, color);\n", name, name);
	fprintf(f, "    if (point) {\n");
	fprintf(f, "      Rectangle collisionRec = {x, y, %s_SourceSize[id].x, %s_SourceSize[id].y};\n", name, name);
	fprintf(f, "      return CheckCollisionPointRec(*point, collisionRec);\n");
	fprintf(f, "    }\n");
	fprintf(f, "  }\n");
	fprintf(f, "  return 0;\n");
	fprintf(f, "}\n\n");

	fprintf(f, "int %s_DrawEx(enum %s_Id id, float x, float y, float rotation, float scale, Color color, int anchor, const Vector2 *point) {\n", name, name);
	fprintf(f, "  if (id) {\n");
	fprintf(f, "    Rectangle sourceRec = %s_Frame[id];\n", name);
	fprintf(f, "    Vector2 origin = %s_Origin[id];\n", name);
	fprintf(f, "    if (anchor & 1) origin.x = 0; else if (anchor & 2) origin.x = %s_SourceSize[id].x;\n", name);
	fprintf(f, "    if (anchor & 4) origin.y = 0; else if (anchor & 8) origin.y = %s_SourceSize[id].y;\n", name);
	fprintf(f, "    origin.x *= scale; origin.y *= scale;\n");
	fprintf(f, "    Rectangle destRec = {x, y, sourceRec.width * scale, sourceRec.height * scale};\n");
	fprintf(f, "    DrawTexturePro(%s_Texture, %s_Frame[id], destRec, origin, rotation, color);\n", name, name);
	fprintf(f, "    if (point) {\n");
	fprintf(f, "      destRec.x -= origin.x;\n");
	fprintf(f, "      destRec.y -= origin.y;\n");
	fprintf(f, "      return CheckCollisionPointRec(*point, destRec);\n");
	fprintf(f, "    }\n");
	fprintf(f, "  }\n");
	fprintf(f, "  return 0;\n");
	fprintf(f, "}\n\n");

	fprintf(f, "Texture %s_GetTexture(void) {\n  return %s_Texture;\n}\n\n", name, name);
	fprintf(f, "const Vector2 %s_GetScale(void) {\n  return %s_Scale;\n}\n\n", name, name);
	fprintf(f, "const Rectangle %s_GetFrame(enum %s_Id id) {\n  return %s_Frame[id];\n}\n\n", name, name, name);
	fprintf(f, "const Vector2 %s_GetOffset(enum %s_Id id) {\n  return %s_Offset[id];\n}\n\n", name, name, name);
	fprintf(f, "const Vector2 %s_GetSourceSize(enum %s_Id id) {\n  return %s_SourceSize[id];\n}\n\n", name, name, name);
	fprintf(f, "const Vector2 %s_GetOrigin(enum %s_Id id) {\n  return %s_Origin[id];\n}\n\n", name, name, name);

	fprintf(f, "#endif\n");
	fprintf(f, "#endif\n");

	return 0;
}
