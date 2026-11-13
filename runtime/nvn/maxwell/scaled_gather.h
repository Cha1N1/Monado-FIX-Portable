// Included as a C string in generated GLSL for scaled guest-texel gathers.
// Each component samples the centre of a guest texel, preserving the gather
// ordering (bottom-left, bottom-right, top-right, top-left) after scaling.
static const char scaled_gather_glsl[] =
"vec4 rgather(sampler2D t, vec2 uv, vec2 sc, int component) {\n"
" if (sc == vec2(1.0)) {\n"
"  if (component == 0) return textureGather(t, uv, 0);\n"
"  if (component == 1) return textureGather(t, uv, 1);\n"
"  if (component == 2) return textureGather(t, uv, 2);\n"
"  return textureGather(t, uv, 3);\n"
" }\n"
" vec2 size = vec2(textureSize(t, 0));\n"
" vec2 base = floor(uv * size / sc - 0.5);\n"
" vec2 p0 = (floor((base + vec2(0.5, 1.5)) * sc) + 0.5) / size;\n"
" vec2 p1 = (floor((base + vec2(1.5, 1.5)) * sc) + 0.5) / size;\n"
" vec2 p2 = (floor((base + vec2(1.5, 0.5)) * sc) + 0.5) / size;\n"
" vec2 p3 = (floor((base + vec2(0.5, 0.5)) * sc) + 0.5) / size;\n"
" return vec4(textureLod(t,p0,0.0)[component], textureLod(t,p1,0.0)[component],\n"
"             textureLod(t,p2,0.0)[component], textureLod(t,p3,0.0)[component]);\n"
"}\n";
