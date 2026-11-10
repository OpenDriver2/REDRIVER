/*
 * D3DGlide/glide_gl.c - [A] Glide on OpenGL 3.1, the device of grSetDevice
 *
 * Not in the original, it replaces glide2x.c, the game's Glide on Direct3D 6 (still on master).
 * Follows the behaviour of that path, not of real Glide:
 * - the engine stays on the _D3D polygon tables (using_glide 0), so vertices are D3D's transformed
 *   vertices: oow is the D3D rhw and the texture coordinates are scaled by it (MakeVertex);
 * - rendering goes to two frame buffer objects that swap places, as the flip chain does, so the back
 *   buffer holds the frame before last after a swap. The window only gets a copy of the finished
 *   frame, letterboxed, which leaves the resolution of the game free of the window's;
 * - the frame buffer objects are kept upside down for OpenGL: row 0 is the top row of the screen.
 *   Then y needs no flipping anywhere, in glScissor, glReadPixels or the vertices, only in
 *   PresentBuffer.
 * - the LFB is a copy in memory, read back on lock and put back on unlock;
 * - polygons are batched per texture and drawn on the same state changes as FlushBatchedPrims, in the
 *   same order, so what overlaps comes out the same. Lines don't wait for the batches;
 * - 8 bit textures are always emulated (emulate_8bit 1): one 16 bit copy per palette. What else the
 *   D3D path left empty stays empty here, but for the pixel count of the sun and the fog: the board's
 *   table fog, which the D3D path didn't have.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dr_types.h"
#include "debug.h"
#include "dr_globals.h"
#include "shell.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"

#include "d3dglide.h"
#include "glide_ptrs.h"
#include "glide_gl.h"

#include <glad/glad.h>
#include <SDL.h>

#define MAX_TEXTURES	32		// as the D3D path: texture memory is 32 surfaces of 64k
#define MAX_VERTICES	4500	// as the D3D path
#define TEXTURE_SIZE	256

#define ATTRIB_POSITION	0
#define ATTRIB_COLOUR	1
#define ATTRIB_TEXCOORD	2
#define ATTRIB_LAMP		3

#define MAX_LAMPS		20		// as lamps[] in object.c

// what the texture stage does, the values of u_mode
#define MODE_DIFFUSE	0		// D3DTOP_DISABLE
#define MODE_TEXTURE	1		// D3DTOP_MODULATE of texture and diffuse
#define MODE_DIFFUSE2	2		// the same with D3DTA_DIFFUSE for D3DTA_TEXTURE: diffuse squared, the alpha still comes from the texture

#define ALPHA_REF_OFF	256.0f	// passes any alpha

#define VIDEO_WIDTH		640
#define VIDEO_HEIGHT	480

// the part of the way to far_clip that polygons fade out on
#define FAR_FADE_PART	0.15f

// not in the glad we have: core since 3.2, and GL_ARB_depth_clamp before that
#define GL_DEPTH_CLAMP	0x864F

typedef struct GLVERTEX
{
	float x;
	float y;
	float z;
	float rhw;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
	float u;
	float v;
	float lx;					// [A] OPENDRIVER_PIXEL_LIGHTS: the point in camera space (FillLampVertex)
	float ly;
	float lz;
	float lit;					// 1 the lamps light it, 0 not
} GLVERTEX;

// [A] the falloff of a lamp in the shader, x = d^2 / r^2 up to 1, as in AddLampLight (polydraw.c)
#define LAMP_STR2(x)	#x
#define LAMP_STR(x)		LAMP_STR2(x)

#ifdef OPENDRIVER_SMOOTH_LAMPS
#define LAMP_FALLOFF	"		x = (1.0 - x * x) * (1.0 - x * x) / (1.0 + " LAMP_STR(LAMP_INV_SQUARE) " * x);\n"
#else
#define LAMP_FALLOFF	"		x = 1.0 - x;\n"
#endif

// D3D has its pixel centres on whole numbers, OpenGL half way between them. A little less than 0.5:
// a vertex exactly on a pixel centre is a tie that D3D gives to the pixel, and OpenGL may not.
static const char *vertex_shader =
	"#version 140\n"
	"in vec4 a_position;\n"
	"in vec4 a_colour;\n"
	"in vec2 a_texcoord;\n"
	"in vec4 a_lamp;\n"
	"out vec4 v_colour;\n"
	"out vec2 v_texcoord;\n"
	"out vec4 v_lamp;\n"
	"uniform vec2 u_scale;\n"
	"uniform float u_flip;\n"
	"void main()\n"
	"{\n"
	"	float w = 1.0 / a_position.w;\n"
	"	vec2 xy = (a_position.xy + 0.49) * u_scale - 1.0;\n"
	"	xy.y *= u_flip;\n"
	"	gl_Position = vec4(xy * w, (a_position.z * 2.0 - 1.0) * w, w);\n"
	"	v_colour = a_colour;\n"
	"	v_texcoord = a_texcoord;\n"
	"	v_lamp = a_lamp;\n"
	"}\n";

static const char *fragment_shader =
	"#version 140\n"
	"in vec4 v_colour;\n"
	"in vec2 v_texcoord;\n"
	"in vec4 v_lamp;\n"
	"out vec4 o_colour;\n"
	"uniform sampler2D u_texture;\n"
	"uniform int u_mode;\n"
	"uniform float u_alpha_ref;\n"
	"uniform float u_fog_table[64];\n"
	"uniform vec3 u_fog_colour;\n"
	"uniform float u_fog_scale;\n"
	"uniform float u_fade_scale;\n"
	"uniform float u_fade_start;\n"
	"uniform float u_fade_range;\n"
	"uniform float u_fade_cell;\n"
	"uniform int u_lamp_count;\n"
	"uniform vec4 u_lamp_position[20];\n"
	"uniform vec3 u_lamp_colour[20];\n"
	// a 4x4 Bayer matrix: the order the pixels go in as the polygon fades out
	"const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);\n"
	// the inverse of guFogTableIndexToW, between the entries as well: four to each power of two
	"float FogAmount(float w)\n"
	"{\n"
	"	float k = floor(log2(w));\n"
	"	float i = clamp(4.0 * k + 8.0 - pow(2.0, 3.0 + k) / w, 0.0, 63.0);\n"
	"	int i0 = int(i);\n"
	"	return mix(u_fog_table[i0], u_fog_table[min(i0 + 1, 63)], i - float(i0));\n"
	"}\n"
	// [A] OPENDRIVER_PIXEL_LIGHTS: the colour plus the light of the lamps at this point, as
	// AddLampLight does per vertex. u_lamp_position: xyz in camera space, w 1 / range^2.
	"vec3 LampLight(vec3 colour)\n"
	"{\n"
	"	vec3 light = vec3(0.0);\n"
	"	for (int i = 0; i < u_lamp_count; i++)\n"
	"	{\n"
	"		vec3 d = u_lamp_position[i].xyz - v_lamp.xyz;\n"
	"		float x = min(dot(d, d) * u_lamp_position[i].w, 1.0);\n"
	LAMP_FALLOFF
	"		light += x * u_lamp_colour[i];\n"
	"	}\n"
	"	return min(colour + light * v_lamp.w, 1.0);\n"
	"}\n"
	"void main()\n"
	"{\n"
	"	if (u_fade_range > 0.0)\n"
	"	{\n"
	"		ivec2 p = ivec2(gl_FragCoord.xy / u_fade_cell) & 3;\n"
	"		if ((u_fade_scale / gl_FragCoord.w - u_fade_start) / u_fade_range >= (bayer[p.y * 4 + p.x] + 0.5) / 16.0)\n"
	"			discard;\n"
	"	}\n"
	"	vec4 diffuse = v_colour;\n"
	"	if (v_lamp.w > 0.0)\n"
	"		diffuse.rgb = LampLight(diffuse.rgb);\n"
	"	vec4 colour = diffuse;\n"
	"	if (u_mode == 1)\n"
	"		colour = texture(u_texture, v_texcoord) * vec4(diffuse.rgb, 1.0);\n"
	"	else if (u_mode == 2)\n"
	"		colour = vec4(diffuse.rgb * diffuse.rgb, texture(u_texture, v_texcoord).a);\n"
	"	if (colour.a * 255.0 >= u_alpha_ref)\n"
	"		discard;\n"
	"	if (u_fog_scale != 0.0)\n"
	"		colour.rgb = mix(colour.rgb, u_fog_colour, FogAmount(max(u_fog_scale / gl_FragCoord.w, 1.0)));\n"
	"	o_colour = colour;\n"
	"}\n";

// a 256x256 ARGB1555 texture with the filter it was last set to
typedef struct GLIMAGE
{
	GLuint texture;
	GrTextureFilterMode_t min_filter;
	GrTextureFilterMode_t mag_filter;
} GLIMAGE;

// As EMULTEXTURE: a list (prev) of texture sets, each a list (next) of palette copies
typedef struct GLEMULTEXTURE GLEMULTEXTURE;

struct GLEMULTEXTURE
{
	GLIMAGE image;
	int tset;
	int palette;
	unsigned char *palette_data;
	GLEMULTEXTURE *next;
	GLEMULTEXTURE *prev;
	unsigned char *pixels;		// the 8 bit data of the set, kept by the first copy
	unsigned char palette_copy[1024];	// the palette the copy was made with
	int replaced16;				// 16 bit data was put in, the copy is not made again from pixels
};

typedef struct GLTEXTURE
{
	GLIMAGE image;
	int emulated;
	int palette2;				// the palette image2 was chosen for
	GLEMULTEXTURE *emultexture;
	GLIMAGE *image2;
} GLTEXTURE;

static SDL_GLContext gl_context;
static int gl_width;
static int gl_height;

// frame buffer
static GLuint gl_colour[2];
static GLuint gl_framebuffer[2];
static GLuint gl_depth;
static int gl_back;				// which of the two is the back buffer
static int gl_swap_interval;

static GLuint gl_video_texture;
static GLuint gl_video_framebuffer;
static int gl_video_width;
static int gl_video_height;
static int gl_colour_masked;
static int gl_depth_masked;
static GLuint gl_query;
static int gl_query_active;
static int gl_fog_on;
static int gl_fog_full;
static int gl_flipped;			// polygons upside down, the board's lower left origin

// LFB
static void *gl_lfb;
static int gl_lfb_locked;		// frame buffer index + 1
static int gl_lfb_depth;

// drawing
static int gl_active;
static GLuint gl_program;
static GLuint gl_vertex_array;
static GLuint gl_vertex_buffer;
static GLint gl_uniform_mode;
static GLint gl_uniform_alpha_ref;
static GLint gl_uniform_fog_table;
static GLint gl_uniform_fade_scale;
static GLint gl_uniform_fade_start;
static GLint gl_uniform_fade_range;
static GLint gl_uniform_fade_cell;
static GLint gl_uniform_fog_colour;
static GLint gl_uniform_fog_scale;
static GLint gl_uniform_lamp_count;
static GLint gl_uniform_lamp_position;
static GLint gl_uniform_lamp_colour;
static int gl_lamp_count;		// [A] the lamps given to the shader (SyncLamps), -1 none yet
static float gl_lamp_position[MAX_LAMPS][4];
static float gl_lamp_colour[MAX_LAMPS][3];
static int gl_shader_mode;
static GLVERTEX gl_vb[MAX_TEXTURES + 1][MAX_VERTICES];
static int gl_vbcount[MAX_TEXTURES + 1];
static float gl_depth_bias;
static float gl_screen_scale = 1.0f;
static float gl_screen_scale64 = 64.0f;
static float gl_far_fade;
static int gl_vertex_alpha;		// the polygons blend by the alpha of their vertices (gl_SetVertexAlpha)

// textures
static GLTEXTURE gl_textures[MAX_TEXTURES];
static GLEMULTEXTURE *gl_emulated_textures;
static int gl_current_palette;
static unsigned char gl_palettes_seen[64][1024];	// each palette as last selected
static int gl_palettes_new[64];	// a copy was made with the palette after it was last selected
static GrTextureFilterMode_t gl_min_filter;
static GrTextureFilterMode_t gl_mag_filter;
static int gl_tset;				// MAX_TEXTURES: no texture, as in glide2x.c
static int gl_last_tset;

// the state glide2x.c keeps in the device
static int gl_modulate;			// D3DTSS_COLOROP
static int gl_colorarg_diffuse;	// D3DTSS_COLORARG1
static int gl_alpha_test;		// D3DRENDERSTATE_ALPHATESTENABLE
static int gl_alpha_less;		// D3DRENDERSTATE_ALPHAFUNC: D3DCMP_LESS or D3DCMP_ALWAYS
static GrAlpha_t gl_alpha_ref;

static int gl_alphacompare_less;
static int gl_alphacompare_changed;
static GrCombineOther_t gl_last_colorcombine_mode;
static GrAlphaBlendFnc_t gl_last_alphablend_sf;
static GrAlphaBlendFnc_t gl_last_alphablend_df;
static GrCombineLocal_t gl_last_alphacombine_local;
static GrCombineOther_t gl_last_alphacombine_other;
static GrCmpFnc_t gl_last_alphatest_function;
static GrCmpFnc_t gl_last_depthbuffer_function;
static GrDepthBufferMode_t gl_last_depthbuffer_mode;

// for emulated 8 bit textures, ARGB1555 maps to itself
static unsigned short gl_texlookup16[65536];
static unsigned short gl_mipmap[TEXTURE_SIZE * TEXTURE_SIZE];

//------------------------------------------------------------------------------------------
// Set up

static void LogError(const char *what, const char *text)
{
	char line[1100];

	sprintf(line, "OpenGL %s: %.1024s", what, text);
	LogQuick(line);
}

static GLuint CompileShader(GLenum type, const char *source)
{
	GLuint shader;
	GLint status;
	char log[1024];

	shader = glCreateShader(type);

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (!status)
	{
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		LogError("shader", log);
	}

	return shader;
}

static void UpdateFarFade(void);

static int CreateProgram(void)
{
	GLuint vs;
	GLuint fs;
	GLint status;
	char log[1024];

	vs = CompileShader(GL_VERTEX_SHADER, vertex_shader);
	fs = CompileShader(GL_FRAGMENT_SHADER, fragment_shader);

	gl_program = glCreateProgram();

	glAttachShader(gl_program, vs);
	glAttachShader(gl_program, fs);

	glBindAttribLocation(gl_program, ATTRIB_POSITION, "a_position");
	glBindAttribLocation(gl_program, ATTRIB_COLOUR, "a_colour");
	glBindAttribLocation(gl_program, ATTRIB_TEXCOORD, "a_texcoord");
	glBindAttribLocation(gl_program, ATTRIB_LAMP, "a_lamp");

	glLinkProgram(gl_program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	glGetProgramiv(gl_program, GL_LINK_STATUS, &status);

	if (!status)
	{
		glGetProgramInfoLog(gl_program, sizeof(log), NULL, log);
		LogError("program", log);
		return 0;
	}

	glUseProgram(gl_program);

	glUniform2f(glGetUniformLocation(gl_program, "u_scale"), 2.0f / (float)gl_width, 2.0f / (float)gl_height);
	glUniform1f(glGetUniformLocation(gl_program, "u_flip"), gl_flipped ? -1.0f : 1.0f);
	glUniform1i(glGetUniformLocation(gl_program, "u_texture"), 0);

	gl_uniform_mode = glGetUniformLocation(gl_program, "u_mode");
	gl_uniform_alpha_ref = glGetUniformLocation(gl_program, "u_alpha_ref");
	gl_uniform_fog_table = glGetUniformLocation(gl_program, "u_fog_table");
	gl_uniform_fog_colour = glGetUniformLocation(gl_program, "u_fog_colour");
	gl_uniform_fog_scale = glGetUniformLocation(gl_program, "u_fog_scale");
	gl_uniform_fade_scale = glGetUniformLocation(gl_program, "u_fade_scale");
	gl_uniform_fade_start = glGetUniformLocation(gl_program, "u_fade_start");
	gl_uniform_fade_range = glGetUniformLocation(gl_program, "u_fade_range");
	gl_uniform_fade_cell = glGetUniformLocation(gl_program, "u_fade_cell");
	gl_uniform_lamp_count = glGetUniformLocation(gl_program, "u_lamp_count");
	gl_uniform_lamp_position = glGetUniformLocation(gl_program, "u_lamp_position");
	gl_uniform_lamp_colour = glGetUniformLocation(gl_program, "u_lamp_colour");

	gl_lamp_count = -1;

	UpdateFarFade();

	return 1;
}

static int CreateFramebuffers(void)
{
	int i;

	glGenTextures(2, gl_colour);
	glGenFramebuffers(2, gl_framebuffer);
	glGenRenderbuffers(1, &gl_depth);

	glBindRenderbuffer(GL_RENDERBUFFER, gl_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, gl_width, gl_height);

	for (i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, gl_colour[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gl_width, gl_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_colour[i], 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl_depth);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			return 0;
	}

	gl_back = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer[gl_back]);

	return 1;
}

static void CreateImage(GLIMAGE *image)
{
	glGenTextures(1, &image->texture);
	glBindTexture(GL_TEXTURE_2D, image->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);

	image->min_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
	image->mag_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
}

static void CreateTextures(void)
{
	int i;

	for (i = 0; i < MAX_TEXTURES; i++)
	{
		CreateImage(&gl_textures[i].image);

		gl_textures[i].emulated = 1;
		gl_textures[i].palette2 = -1;
		gl_textures[i].emultexture = NULL;
		gl_textures[i].image2 = NULL;
	}

	gl_min_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
	gl_mag_filter = GR_TEXTUREFILTER_POINT_SAMPLED;
	gl_current_palette = -1;
	gl_tset = 0;
	gl_last_tset = 0;
}

static void CreateVertexBuffer(void)
{
	glGenVertexArrays(1, &gl_vertex_array);
	glGenBuffers(1, &gl_vertex_buffer);

	glBindVertexArray(gl_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buffer);

	glEnableVertexAttribArray(ATTRIB_POSITION);
	glEnableVertexAttribArray(ATTRIB_COLOUR);
	glEnableVertexAttribArray(ATTRIB_TEXCOORD);
	glEnableVertexAttribArray(ATTRIB_LAMP);

	glVertexAttribPointer(ATTRIB_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(GLVERTEX), (void *)offsetof(GLVERTEX, x));
	glVertexAttribPointer(ATTRIB_COLOUR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLVERTEX), (void *)offsetof(GLVERTEX, r));
	glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(GLVERTEX), (void *)offsetof(GLVERTEX, u));
	glVertexAttribPointer(ATTRIB_LAMP, 4, GL_FLOAT, GL_FALSE, sizeof(GLVERTEX), (void *)offsetof(GLVERTEX, lx));
}

// As SetupRenderStates. The last_ values go too: unlike the device's state they would outlive the
// context, and keep the game's first call from reaching it.
static void SetupRenderStates(void)
{
	gl_modulate = 1;
	gl_colorarg_diffuse = 0;
	gl_alpha_test = 0;
	gl_alpha_less = 0;
	gl_alpha_ref = 0;
	gl_alphacompare_less = 0;
	gl_alphacompare_changed = 0;
	gl_depth_bias = 0.0f;
	gl_fog_on = 0;
	gl_fog_full = 0;
	gl_shader_mode = -1;

	gl_last_colorcombine_mode = -1;
	gl_last_alphablend_sf = -1;
	gl_last_alphablend_df = -1;
	gl_last_alphacombine_local = -1;
	gl_last_alphacombine_other = -1;
	gl_last_alphatest_function = -1;
	gl_last_depthbuffer_function = -1;
	gl_last_depthbuffer_mode = -1;

	memset(gl_vbcount, 0, sizeof(gl_vbcount));

	glUniform1f(gl_uniform_alpha_ref, ALPHA_REF_OFF);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_BLEND);

	// The game clips at z = 50 and z here reaches 0 at w = 64. Direct3D takes a transformed vertex of
	// z < 0 as it is, OpenGL would cut the polygon at the near plane and leave a hole.
	glEnable(GL_DEPTH_CLAMP);
}

static void FX_CALL gl_SstWinClose(void);
static void FX_CALL gl_BufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth);
static void FX_CALL gl_BufferSwap(int swap_interval);
static void FlushBatchedPrims(void);

// What oow and x, y scale by, as ResetViewport has it
static float ViewScale(void)
{
	if (gl_width * 3 > gl_height * 4)
		return (float)gl_height / 480.0f;

	return (float)gl_width / 640.0f;
}

// The pattern keeps the size of a pixel at 640x480, whatever the resolution
static void UpdateFarFade(void)
{
	float scale;

	scale = ViewScale();

	glUniform1f(gl_uniform_fade_scale, gl_screen_scale * scale);
	glUniform1f(gl_uniform_fade_start, gl_far_fade * (1.0f - FAR_FADE_PART));
	glUniform1f(gl_uniform_fade_range, gl_far_fade * FAR_FADE_PART);
	glUniform1f(gl_uniform_fade_cell, scale < 1.0f ? 1.0f : (float)floor(scale + 0.5f));
}

// The window is the SDL one, its size is not ours
static FxBool OpenDevice(int width, int height)
{
	if (gl_active)
		gl_SstWinClose();

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

	gl_context = SDL_GL_CreateContext(gWindow);

	if (gl_context == NULL)
	{
		LogError("context", SDL_GetError());
		return FXFALSE;
	}

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress) || !GLAD_GL_VERSION_3_1)
	{
		LogError("version", "3.1 is not available");
		gl_SstWinClose();
		return FXFALSE;
	}

	gl_width = width;
	gl_height = height;

	gl_lfb = malloc(gl_width * gl_height * 4);

	if (gl_lfb == NULL || !CreateFramebuffers() || !CreateProgram())
	{
		LogError("set up", "no frame buffer");
		gl_SstWinClose();
		return FXFALSE;
	}

	CreateTextures();
	CreateVertexBuffer();

	glGenQueries(1, &gl_query);

	gl_swap_interval = -1;
	gl_colour_masked = 0;
	gl_depth_masked = 0;
	gl_query_active = 0;
	gl_lfb_locked = 0;

	SetupRenderStates();

	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, gl_width, gl_height);

	// the context starts with the viewport of the window, which is not our size in full screen
	glViewport(0, 0, gl_width, gl_height);

	gl_active = 1;

	gl_BufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	gl_BufferSwap(0);
	gl_BufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	gl_BufferSwap(0);

	return FXTRUE;
}

// hWnd is not used, the resolution is display_width x display_height in both modes.
// With the lower left origin the polygons and the clip window turn upside down, and what goes through
// the LFB stays upright, as on the board: the game locks it with the upper left origin.
static FxBool FX_CALL gl_SstWinOpen(FxU32 hWnd, GrScreenResolution_t screen_resolution, GrScreenRefresh_t refresh_rate,
	GrColorFormat_t color_format, GrOriginLocation_t origin_location, int nColBuffers, int nAuxBuffers)
{
	gl_flipped = origin_location == GR_ORIGIN_LOWER_LEFT;

	return OpenDevice(display_width, display_height);
}

// Everything goes with the context. The window stays, full screen too.
static void FX_CALL gl_SstWinClose(void)
{
	gl_ReleaseEmulatedTextures();

	gl_active = 0;
	gl_video_texture = 0;
	gl_video_framebuffer = 0;

	if (gl_context)
	{
		SDL_GL_DeleteContext(gl_context);
		gl_context = NULL;
	}

	free(gl_lfb);
	gl_lfb = NULL;

	gl_width = 0;
	gl_height = 0;
}

static void FX_CALL gl_GlideShutdown(void)
{
	gl_SstWinClose();
}

// Reports a single Voodoo board with one TMU, as the D3D path did.
static FxBool FX_CALL gl_SstQueryHardware(GrHwConfiguration *hwconfig)
{
	hwconfig->num_sst = 1;
	hwconfig->SSTs[0].type = GR_SSTTYPE_VOODOO;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.fbRam = 2;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.fbiRev = 2;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.nTexelfx = 1;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.sliDetect = FXFALSE;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRev = 1;
	hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRam = 2;

	return FXTRUE;
}

static FxBool FX_CALL gl_SstQueryBoards(GrHwConfiguration *hwconfig)
{
	return gl_SstQueryHardware(hwconfig);
}

static FxBool FX_CALL gl_SstControl(FxU32 code)
{
	return FXTRUE;
}

static FxU32 FX_CALL gl_SstScreenWidth(void)
{
	return gl_width;
}

static FxU32 FX_CALL gl_SstScreenHeight(void)
{
	return gl_height;
}

static void FX_CALL gl_GlideGetVersion(char version[80])
{
	sprintf(version, "2.43 ( OpenGL )");
}

//------------------------------------------------------------------------------------------
// Frame buffer

// Only the clip window, the scissor test sees to that
static void FX_CALL gl_BufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
{
	GLbitfield mask;

	if (!gl_active)
		return;

	mask = 0;

	if (!gl_colour_masked)
	{
		glClearColor(((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, (color & 0xFF) / 255.0f, 0.0f);
		mask |= GL_COLOR_BUFFER_BIT;
	}

	if (!gl_depth_masked)
	{
		if (depth != GR_WDEPTHVALUE_FARTHEST && depth != 0)
			glClearDepth(0.0);
		else
			glClearDepth(1.0);

		mask |= GL_DEPTH_BUFFER_BIT;
	}

	if (mask)
		glClear(mask);
}

// Scaled to fit the window, keeping the shape of the picture
static void PresentBuffer(GLuint framebuffer)
{
	int window_width;
	int window_height;
	int width;
	int height;
	int x;
	int y;

	SDL_GL_GetDrawableSize(gWindow, &window_width, &window_height);

	if (window_width * gl_height > window_height * gl_width)
	{
		height = window_height;
		width = window_height * gl_width / gl_height;
	}
	else
	{
		width = window_width;
		height = window_width * gl_height / gl_width;
	}

	x = (window_width - width) / 2;
	y = (window_height - height) / 2;

	glDisable(GL_SCISSOR_TEST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
	glBlitFramebuffer(0, 0, gl_width, gl_height, x, y + height, x + width, y, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	glEnable(GL_SCISSOR_TEST);
}

// NOTE: swap_interval decides the wait for the screen in both modes, which is what the flip of the
// D3D path does. The fades of the splash screens are timed by it.
static void FX_CALL gl_BufferSwap(int swap_interval)
{
	if (!gl_active)
		return;

	FlushBatchedPrims();

	swap_interval = swap_interval != 0;

	if (swap_interval != gl_swap_interval)
	{
		gl_swap_interval = swap_interval;
		SDL_GL_SetSwapInterval(swap_interval);
	}

	PresentBuffer(gl_framebuffer[gl_back]);

	SDL_GL_SwapWindow(gWindow);

	gl_back ^= 1;

	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer[gl_back]);
}

// Shows the last swapped frame again, at the size the window has now. The game may be anywhere in
// a frame, what it has drawn so far stays in the back buffer.
void gl_RepaintWindow(void)
{
	if (!gl_active)
		return;

	PresentBuffer(gl_framebuffer[gl_back ^ 1]);

	SDL_GL_SwapWindow(gWindow);

	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer[gl_back]);
}

// Gives the device another size, for the game display that follows the window. Only the frame
// buffers and the LFB copy are made again, the textures and the state stay with the context. Both
// buffers start black. 0 when it fails, the old size stays then.
int gl_ResizeDevice(int width, int height)
{
	void *lfb;
	int i;

	if (!gl_active || gl_lfb_locked)
		return 0;

	if (width == gl_width && height == gl_height)
		return 1;

	lfb = malloc(width * height * 4);

	if (lfb == NULL)
		return 0;

	FlushBatchedPrims();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(2, gl_framebuffer);
	glDeleteTextures(2, gl_colour);
	glDeleteRenderbuffers(1, &gl_depth);

	free(gl_lfb);
	gl_lfb = lfb;

	gl_width = width;
	gl_height = height;

	if (!CreateFramebuffers())
	{
		LogError("resize", "no frame buffer");
		return 0;
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glUniform2f(glGetUniformLocation(gl_program, "u_scale"), 2.0f / (float)gl_width, 2.0f / (float)gl_height);

	glViewport(0, 0, gl_width, gl_height);
	glScissor(0, 0, gl_width, gl_height);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	for (i = 1; i >= 0; i--)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer[i]);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	UpdateFarFade();

	return 1;
}

//------------------------------------------------------------------------------------------
// Video

// The videos are always 640x480, whatever the resolution of the game. The device is open for them
// between ShutdownDrawingSystem and InitDrawingSystem, so the window is repainted as ever.
int gl_StartVideo(void)
{
	return OpenDevice(VIDEO_WIDTH, VIDEO_HEIGHT);
}

// Leaves the window black
void gl_StopVideo(void)
{
	gl_BufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
	gl_BufferSwap(0);

	gl_SstWinClose();
}

// The texture and frame buffer go with the context
int gl_SetVideoSize(int width, int height)
{
	glGenTextures(1, &gl_video_texture);
	glBindTexture(GL_TEXTURE_2D, gl_video_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenFramebuffers(1, &gl_video_framebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_video_framebuffer);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_video_texture, 0);

	gl_video_width = width;
	gl_video_height = height;

	return glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

// As StepVideo does: the full width, double height, in the middle. data is 32 bit BGRA, top row
// first, which is how the frame buffers are kept.
// NOTE: filtered; the DirectDraw blit was whatever the card did when it stretched
void gl_ShowVideoFrame(const void *data)
{
	int y;

	glBindTexture(GL_TEXTURE_2D, gl_video_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl_video_width, gl_video_height, GL_BGRA, GL_UNSIGNED_BYTE, data);

	y = (VIDEO_HEIGHT - 2 * gl_video_height) >> 1;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_video_framebuffer);
	glBlitFramebuffer(0, 0, gl_video_width, gl_video_height, 0, y, VIDEO_WIDTH, y + 2 * gl_video_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	gl_BufferSwap(1);
}

static void FX_CALL gl_ClipWindow(FxU32 minx, FxU32 miny, FxU32 maxx, FxU32 maxy)
{
	if (gl_active)
	{
		FlushBatchedPrims();

		glScissor(minx, gl_flipped ? gl_height - maxy : miny, maxx - minx, maxy - miny);
	}
}

static void FX_CALL gl_ColorMask(FxBool rgb, FxBool a)
{
	gl_colour_masked = rgb != FXTRUE;
}

static void FX_CALL gl_DepthMask(FxBool mask)
{
	if (gl_active)
	{
		FlushBatchedPrims();

		gl_depth_masked = mask != FXTRUE;
		glDepthMask(mask == FXTRUE);
	}
}

//------------------------------------------------------------------------------------------
// Linear frame buffer

// GR_LFBWRITEMODE_8888 is what grLfbLock asks for: B, G, R, 0 in memory, which fits the frontend's
// copy routines. Anything else is RGB565, which general.c (debug text, screenshot) and debris.c write
// and read directly.
static FxBool FX_CALL gl_LfbLock(GrLock_t type, GrBuffer_t buffer, GrLfbWriteMode_t writeMode, GrOriginLocation_t origin, FxBool pixelPipeline, GrLfbInfo_t *info)
{
	int index;

	if (!gl_active)
		return FXFALSE;

	FlushBatchedPrims();

	if (buffer == GR_BUFFER_BACKBUFFER)
		index = gl_back;
	else if (buffer == GR_BUFFER_FRONTBUFFER)
		index = gl_back ^ 1;
	else
		return FXFALSE;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_framebuffer[index]);

	if (writeMode == GR_LFBWRITEMODE_8888)
	{
		gl_lfb_depth = 32;

		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		glReadPixels(0, 0, gl_width, gl_height, GL_BGRA, GL_UNSIGNED_BYTE, gl_lfb);

		info->strideInBytes = gl_width * 4;
		info->writeMode = GR_LFBWRITEMODE_8888;
	}
	else
	{
		gl_lfb_depth = 16;

		glPixelStorei(GL_PACK_ALIGNMENT, 2);
		glReadPixels(0, 0, gl_width, gl_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, gl_lfb);

		info->strideInBytes = gl_width * 2;
		info->writeMode = GR_LFBWRITEMODE_565;
	}

	info->lfbPtr = gl_lfb;
	info->origin = GR_ORIGIN_UPPER_LEFT;

	gl_lfb_locked = index + 1;

	return FXTRUE;
}

static FxBool FX_CALL gl_LfbUnlock(GrLock_t type, GrBuffer_t buffer)
{
	if (!gl_active)
		return FXFALSE;

	if (gl_lfb_locked && type != GR_LFB_READ_ONLY)
	{
		glBindTexture(GL_TEXTURE_2D, gl_colour[gl_lfb_locked - 1]);

		if (gl_lfb_depth == 32)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl_width, gl_height, GL_BGRA, GL_UNSIGNED_BYTE, gl_lfb);
		}
		else
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl_width, gl_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, gl_lfb);
		}

		// a line can draw with whatever is bound, and this is what it draws to
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	gl_lfb_locked = 0;

	return FXTRUE;
}

// grLfbLock of the OpenGL device
FxBool gl_LfbLockInfo(GrBuffer_t buffer, LFB_INFO *info)
{
	if (gl_LfbLock(GR_LFB_WRITE_ONLY, buffer, GR_LFBWRITEMODE_8888, GR_ORIGIN_UPPER_LEFT, FXFALSE, &info->info) != FXTRUE)
		return FXFALSE;

	info->depth = 32;
	info->rbits = 0;
	info->rshift = 16;
	info->gbits = 0;
	info->gshift = 8;
	info->bbits = 0;
	info->bshift = 0;

	return FXTRUE;
}

//------------------------------------------------------------------------------------------
// Textures

static void UploadImage(GLIMAGE *image, const void *data)
{
	glBindTexture(GL_TEXTURE_2D, image->texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_SIZE, TEXTURE_SIZE, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, data);
}

static void SelectImage(GLIMAGE *image)
{
	if (image == NULL)
	{
		glBindTexture(GL_TEXTURE_2D, 0);
		return;
	}

	glBindTexture(GL_TEXTURE_2D, image->texture);

	if (image->min_filter != gl_min_filter)
	{
		image->min_filter = gl_min_filter;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_min_filter == GR_TEXTUREFILTER_POINT_SAMPLED ? GL_NEAREST : GL_LINEAR);
	}

	if (image->mag_filter != gl_mag_filter)
	{
		image->mag_filter = gl_mag_filter;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_mag_filter == GR_TEXTUREFILTER_POINT_SAMPLED ? GL_NEAREST : GL_LINEAR);
	}
}

// As BlitMipmap8. The original has a transparent palette index in it, but set to -1, which a byte
// never is, so the alpha bit stays 0 and the alpha test lets everything through.
static void BlitMipmap8(const unsigned char *src, const unsigned char *palette, unsigned short *dst)
{
	const unsigned char *entry;
	int i;

	for (i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++)
	{
		entry = palette + src[i] * 4;
		dst[i] = (unsigned short)(((entry[2] >> 3) << 10) | ((entry[1] >> 3) << 5) | (entry[0] >> 3));
	}
}

static GLEMULTEXTURE *GetEmulatedTexture(int tset)
{
	GLEMULTEXTURE *et;

	for (et = gl_emulated_textures; et; et = et->prev)
	{
		if (et->tset == tset)
			return et;
	}

	return NULL;
}

// The 16 bit copy of an 8 bit texture set with one palette: a new texture set, or one more palette of
// a known one. As the D3D path did.
int gl_CreateEmulatedTexture(int tset, unsigned char *data, int palette, char *palette_data)
{
	GLEMULTEXTURE *set;
	GLEMULTEXTURE *copy;

	if (!gl_active)
		return 0;

	copy = (GLEMULTEXTURE *)malloc(sizeof(GLEMULTEXTURE));

	if (copy == NULL)
		return 0;

	CreateImage(&copy->image);

	copy->tset = tset;
	copy->palette = palette;
	copy->palette_data = (unsigned char *)palette_data;
	copy->next = NULL;
	copy->prev = NULL;
	copy->pixels = NULL;
	copy->replaced16 = 0;
	memcpy(copy->palette_copy, palette_data, sizeof(copy->palette_copy));

	if (palette >= 0 && palette < 64)
		gl_palettes_new[palette] = 1;

	set = GetEmulatedTexture(tset);

	if (set)
	{
		while (set->next)
			set = set->next;

		set->next = copy;
	}
	else
	{
		copy->prev = gl_emulated_textures;
		gl_emulated_textures = copy;

		copy->pixels = (unsigned char *)malloc(TEXTURE_SIZE * TEXTURE_SIZE);

		if (copy->pixels)
			memcpy(copy->pixels, data, TEXTURE_SIZE * TEXTURE_SIZE);
	}

	BlitMipmap8(data, copy->palette_data, gl_mipmap);
	UploadImage(&copy->image, gl_mipmap);

	return 1;
}

// As the D3D path, but the texture memory doesn't go on pointing at what was freed.
void gl_ReleaseEmulatedTextures(void)
{
	GLEMULTEXTURE *set;
	GLEMULTEXTURE *copy;
	GLEMULTEXTURE *next;
	GLEMULTEXTURE *prev;
	int i;

	FlushBatchedPrims();

	for (set = gl_emulated_textures; set; set = prev)
	{
		prev = set->prev;

		for (copy = set; copy; copy = next)
		{
			next = copy->next;

			glDeleteTextures(1, &copy->image.texture);
			free(copy->pixels);
			free(copy);
		}
	}

	gl_emulated_textures = NULL;
	gl_current_palette = -1;

	for (i = 0; i < MAX_TEXTURES; i++)
	{
		gl_textures[i].palette2 = -1;
		gl_textures[i].emultexture = NULL;
		gl_textures[i].image2 = NULL;
	}
}

// Texture memory is emulated as on the D3D path: the address selects the surface.
// NOTE: an 8 bit texture with no emulated copy is left out, in glide2x.c it is written as it is into
// a 16 bit surface
static void FX_CALL gl_TexDownloadMipMap(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
{
	FxU32 surface;

	if (!gl_active)
		return;

	FlushBatchedPrims();

	if (info->format == GR_TEXFMT_P_8)
		return;

	surface = 2 * (startAddress >> 17);

	if (surface >= MAX_TEXTURES || info->largeLod != GR_LOD_256)
		return;

	UploadImage(&gl_textures[surface].image, info->data);

	gl_textures[surface].emulated = 0;
	gl_textures[surface].palette2 = -1;
	gl_textures[surface].emultexture = NULL;
	gl_textures[surface].image2 = NULL;
}

// As the D3D path: an 8 bit texture only marks its surface, the copies are there already.
void gl_TexDownloadMipMapSet(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	GLEMULTEXTURE *set;
	FxU32 surface;

	if (!gl_active)
		return;

	FlushBatchedPrims();

	if (info->format != GR_TEXFMT_P_8)
	{
		gl_TexDownloadMipMap(tmu, startAddress, evenOdd, info);
		return;
	}

	set = GetEmulatedTexture(tset);
	surface = startAddress >> 16;

	if (set && surface < MAX_TEXTURES)
	{
		gl_textures[surface].emulated = 1;
		gl_textures[surface].palette2 = -1;
		gl_textures[surface].emultexture = set;
		gl_textures[surface].image2 = NULL;
	}
}

// New data for a texture set in texture memory
void gl_TexDownload8(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info, int tset)
{
	GLEMULTEXTURE *copy;

	if (!gl_active)
		return;

	FlushBatchedPrims();

	copy = info->format == GR_TEXFMT_P_8 ? GetEmulatedTexture(tset) : NULL;

	if (copy == NULL)
	{
		gl_TexDownloadMipMap(tmu, startAddress, evenOdd, info);
		return;
	}

	if (copy->pixels)
		memcpy(copy->pixels, info->data, TEXTURE_SIZE * TEXTURE_SIZE);

	for (; copy; copy = copy->next)
	{
		memcpy(copy->palette_copy, copy->palette_data, sizeof(copy->palette_copy));
		copy->replaced16 = 0;

		if (copy->palette >= 0 && copy->palette < 64)
			gl_palettes_new[copy->palette] = 1;

		BlitMipmap8((unsigned char *)info->data, copy->palette_data, gl_mipmap);
		UploadImage(&copy->image, gl_mipmap);
	}
}

// New 16 bit data for the copies with the current palette
void gl_TexDownload16(int tset, void *data)
{
	GLEMULTEXTURE *copy;

	if (!gl_active)
		return;

	FlushBatchedPrims();

	for (copy = GetEmulatedTexture(tset); copy; copy = copy->next)
	{
		if (copy->palette == gl_current_palette)
		{
			UploadImage(&copy->image, data);
			copy->replaced16 = 1;
		}
	}
}

// The palette number picks the copy of the emulated textures. A copy whose palette changed since it
// was made (DoNeonColourCycle rotates palette 19, used in LA) is made again: on Glide the palette is
// downloaded to the card and the textures follow it.
void gl_TexDownloadPalette(int palette, void *data)
{
	GLEMULTEXTURE *set;
	GLEMULTEXTURE *copy;

	if (!gl_active)
		return;

	FlushBatchedPrims();

	gl_current_palette = palette;

	if (data == NULL || palette < 0 || palette >= 64)
		return;

	if (!gl_palettes_new[palette] && memcmp(gl_palettes_seen[palette], data, sizeof(gl_palettes_seen[palette])) == 0)
		return;

	memcpy(gl_palettes_seen[palette], data, sizeof(gl_palettes_seen[palette]));
	gl_palettes_new[palette] = 0;

	for (set = gl_emulated_textures; set; set = set->prev)
	{
		if (set->pixels == NULL)
			continue;

		for (copy = set; copy; copy = copy->next)
		{
			if (copy->palette != palette || copy->replaced16 ||
				memcmp(copy->palette_copy, copy->palette_data, sizeof(copy->palette_copy)) == 0)
				continue;

			memcpy(copy->palette_copy, copy->palette_data, sizeof(copy->palette_copy));

			BlitMipmap8(set->pixels, copy->palette_data, gl_mipmap);
			UploadImage(&copy->image, gl_mipmap);
		}
	}
}

static void FX_CALL gl_TexDownloadTable(GrChipID_t tmu, GrTexTable_t type, void *data)
{
	FlushBatchedPrims();
}

static void SetEmulatedTexture(int tset)
{
	GLTEXTURE *texture;
	GLEMULTEXTURE *copy;

	texture = &gl_textures[tset];

	if (texture->palette2 != gl_current_palette)
	{
		texture->image2 = NULL;

		for (copy = texture->emultexture; copy; copy = copy->next)
		{
			if (copy->palette == gl_current_palette)
			{
				texture->image2 = &copy->image;
				break;
			}
		}

		if (texture->image2 == NULL && texture->emultexture)
			texture->image2 = &texture->emultexture->image;

		texture->palette2 = gl_current_palette;
	}

	SelectImage(texture->image2);
}

static void FX_CALL gl_TexSource(GrChipID_t tmu, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
{
	FxU32 surface;

	if (!gl_active)
		return;

	if (info->format == GR_TEXFMT_P_8)
		surface = startAddress >> 16;
	else
		surface = 2 * (startAddress >> 17);

	if (surface < MAX_TEXTURES)
	{
		gl_tset = surface;
		gl_last_tset = surface;
	}
}

static FxU32 FX_CALL gl_TexMaxAddress(GrChipID_t tmu)
{
	if (tmu != GR_TMU0)
		return 0;

	return 0x1FFFF8;	// 32 surfaces of 64k
}

static void FX_CALL gl_TexFilterMode(GrChipID_t tmu, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode)
{
	FlushBatchedPrims();

	gl_min_filter = minfilter_mode;
	gl_mag_filter = magfilter_mode;
}

//------------------------------------------------------------------------------------------
// State

static void UpdateAlphaTest(void)
{
	glUniform1f(gl_uniform_alpha_ref, gl_alpha_test && gl_alpha_less ? (float)gl_alpha_ref : ALPHA_REF_OFF);
}

// Fog goes by w as it is at 640 wide.
// NOTE: [A] oow grows with the width of the screen, so Glide's fog thins out at higher resolutions.
// Here it is the same at all of them, the one of 640x480.
// On the board fog doesn't change the alpha, so it leaves the shadows that darken by it alone. These
// go by the colour here (gl_colorarg_diffuse), which fog would change.
static void UpdateFog(void)
{
	float scale;

	scale = 0.0f;

	// a scale that takes every w past the end of the table, where it is all fog
	if (gl_fog_on && !gl_colorarg_diffuse)
		scale = gl_fog_full ? 1000000.0f : ViewScale();

	glUniform1f(gl_uniform_fog_scale, scale);
}

static void SetBlend(GLenum src, GLenum dst)
{
	glBlendFunc(src, dst);
	glEnable(GL_BLEND);

	gl_alpha_test = 1;
}

// The modes of the D3D path with everything a device may lack taken as there, and the one of the
// searchlights, which that path didn't have. The combinations it didn't name change nothing.
static void FX_CALL gl_AlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf, GrAlphaBlendFnc_t rgb_df, GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df)
{
	if (!gl_active || (rgb_sf == gl_last_alphablend_sf && rgb_df == gl_last_alphablend_df))
		return;

	gl_last_alphablend_sf = rgb_sf;
	gl_last_alphablend_df = rgb_df;

	FlushBatchedPrims();

	if (gl_alphacompare_changed)
	{
		gl_alpha_less = gl_alphacompare_less;
		gl_colorarg_diffuse = 0;
		gl_alphacompare_changed = 0;
	}

	if (rgb_sf == GR_BLEND_SRC_COLOR)
	{
		SetBlend(GL_DST_COLOR, GL_ONE);
	}
	else if (rgb_sf == GR_BLEND_ONE)
	{
		if (rgb_df == GR_BLEND_ZERO)
			glDisable(GL_BLEND);
		else if (rgb_df == GR_BLEND_ONE)
			SetBlend(GL_ONE, GL_ONE);
		else if (rgb_df == GR_BLEND_ONE_MINUS_SRC_COLOR)
			SetBlend(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	else if (rgb_sf == GR_BLEND_ZERO)
	{
		// the shadow of a textured polygon: the colour comes from the vertex, the shape from the alpha
		if (rgb_df == GR_BLEND_ONE_MINUS_SRC_ALPHA)
		{
			gl_colorarg_diffuse = 1;
			gl_alpha_less = 1;
			gl_alphacompare_changed = 1;
		}

		SetBlend(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	}

	UpdateAlphaTest();
	UpdateFog();
}

// Only the alpha test follows: on unless the alpha is the constant colour
static void FX_CALL gl_AlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
{
	if (!gl_active || (local == gl_last_alphacombine_local && other == gl_last_alphacombine_other))
		return;

	gl_last_alphacombine_local = local;
	gl_last_alphacombine_other = other;

	FlushBatchedPrims();

	gl_alpha_test = other != GR_COMBINE_OTHER_CONSTANT;

	UpdateAlphaTest();
}

// Every function other than GR_CMP_ALWAYS is D3DCMP_LESS, as on the D3D path
static void FX_CALL gl_AlphaTestFunction(GrCmpFnc_t function)
{
	if (!gl_active || function == gl_last_alphatest_function)
		return;

	gl_last_alphatest_function = function;

	FlushBatchedPrims();

	gl_alphacompare_less = function != GR_CMP_ALWAYS;
	gl_alpha_less = gl_alphacompare_less;

	UpdateAlphaTest();
}

static void FX_CALL gl_AlphaTestReferenceValue(GrAlpha_t value)
{
	if (gl_active)
	{
		gl_alpha_ref = value;

		UpdateAlphaTest();
	}
}

// Only "texture or not" is emulated, as on the D3D path
static void FX_CALL gl_ColorCombine(GrCombineFunction_t function, GrCombineFactor_t factor, GrCombineLocal_t local, GrCombineOther_t other, FxBool invert)
{
	if (gl_active && other != gl_last_colorcombine_mode)
	{
		gl_last_colorcombine_mode = other;

		FlushBatchedPrims();

		if (other == GR_COMBINE_OTHER_TEXTURE)
		{
			gl_modulate = 1;
			gl_tset = gl_last_tset;
		}
		else
		{
			gl_modulate = 0;

			if (gl_tset != MAX_TEXTURES)
			{
				gl_last_tset = gl_tset;
				gl_tset = MAX_TEXTURES;
			}
		}
	}
}

// Screen y runs the other way in Direct3D, but the frame buffer is upside down: the two cancel, and
// what it calls clockwise is counter clockwise here. As there this also reaches what is in the batches.
static void FX_CALL gl_CullMode(GrCullMode_t mode)
{
	if (!gl_active)
		return;

	if (mode == GR_CULL_DISABLE)
	{
		glDisable(GL_CULL_FACE);
	}
	else if (mode == GR_CULL_NEGATIVE)
	{
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
	}
	else if (mode == GR_CULL_POSITIVE)
	{
		glCullFace(GL_FRONT);
		glEnable(GL_CULL_FACE);
	}
}

// Glide units to Direct3D z; negative biases (towards the viewer) are scaled down
static void FX_CALL gl_DepthBiasLevel(FxI16 level)
{
	gl_depth_bias = (float)level;

	if (gl_depth_bias < 0.0f)
		gl_depth_bias *= 0.03846f;
}

static void FX_CALL gl_DepthBufferFunction(GrCmpFnc_t function)
{
	if (!gl_active || function == gl_last_depthbuffer_function)
		return;

	gl_last_depthbuffer_function = function;

	FlushBatchedPrims();

	// GR_CMP_NEVER to GR_CMP_ALWAYS are in the order of GL_NEVER to GL_ALWAYS
	glDepthFunc(GL_NEVER + function);
}

static void FX_CALL gl_DepthBufferMode(GrDepthBufferMode_t mode)
{
	if (!gl_active || mode == gl_last_depthbuffer_mode)
		return;

	gl_last_depthbuffer_mode = mode;

	FlushBatchedPrims();

	if (mode != GR_DEPTHBUFFER_DISABLE)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void gl_SetScreenScale(float scale)
{
	if (gl_active && scale != gl_screen_scale)
		FlushBatchedPrims();

	gl_screen_scale = scale;
	gl_screen_scale64 = scale * 64.0f;

	if (gl_active)
		UpdateFarFade();
}

// Polygons go out through a pattern on the last stretch before far_dist, instead of being cut
// there. 0 turns it off.
void gl_SetFarFade(float far_dist)
{
	if (gl_active && far_dist != gl_far_fade)
	{
		FlushBatchedPrims();

		gl_far_fade = far_dist;

		UpdateFarFade();
	}
}

// [A]
// Blends polygons by the alpha of their vertices, which the Glide calls can't ask for. Turning it
// off leaves the blend unset: the caller has to call grAlphaBlendFunction before it draws again.
void gl_SetVertexAlpha(int on)
{
	if (!gl_active || on == gl_vertex_alpha)
		return;

	FlushBatchedPrims();

	gl_vertex_alpha = on;

	if (on)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glUniform1f(gl_uniform_alpha_ref, ALPHA_REF_OFF);
	}
	else
	{
		gl_last_alphablend_sf = -1;
		gl_last_alphablend_df = -1;

		UpdateAlphaTest();
	}
}

//------------------------------------------------------------------------------------------
// Drawing

// [A]
// OPENDRIVER_PIXEL_LIGHTS: the point in camera space and whether the lamps light it (the mark of
// AddLampLight). x and y come back from the screen position as the projection made them
// (VertexRotatePers_D3D, ProjectClipPoint): x = tx * oow * DIST + d3d_screen_xmid. z is the
// camera z the D3D path keeps in the vertex (SetPosition, SetClipVertex): oow does not give it,
// ooz_table is made with screen_scale times display_width / 640 (ResetViewport). Every vertex
// gets the point, so a triangle lit at only some of its vertices fades the light across it
// instead of moving it.
static void FillLampVertex(const GrVertex *gv, GLVERTEX *dv)
{
#ifdef OPENDRIVER_PIXEL_LIGHTS
	float scale;

	if (gv->oow > 0.0f)
	{
		scale = 1.0f / (gv->oow * DIST);

		dv->lx = (gv->x - d3d_screen_xmid) * scale;
		dv->ly = (gv->y - d3d_screen_ymid) * scale;
		dv->lz = gv->z;
	}
	else
	{
		dv->lx = 0.0f;
		dv->ly = 0.0f;
		dv->lz = 0.0f;
	}

	dv->lit = gv->tmuvtx[1].oow == PIXEL_LAMP_MARK ? 1.0f : 0.0f;
#else
	dv->lx = 0.0f;
	dv->ly = 0.0f;
	dv->lz = 0.0f;
	dv->lit = 0.0f;
#endif
}

// All but z. The colour goes through one int as in MAKE_VERTEX, the alpha is usually 0.
static void FillVertex(const GrVertex *gv, GLVERTEX *dv)
{
	float tscale;
	unsigned int colour;

	tscale = 1.0f / 256.0f / gv->oow;
	colour = ((int)gv->g << 8) | (int)gv->b | ((int)gv->r << 16);

	dv->x = gv->x;
	dv->y = gv->y;
	dv->rhw = gv->oow;
	dv->r = (unsigned char)(colour >> 16);
	dv->g = (unsigned char)(colour >> 8);
	dv->b = (unsigned char)colour;
	dv->a = gl_vertex_alpha ? (unsigned char)gv->a : (unsigned char)(colour >> 24);
	dv->u = gv->tmuvtx[0].sow * tscale;
	dv->v = gv->tmuvtx[0].tow * tscale;

	FillLampVertex(gv, dv);
}

// MAKE_VERTEX. z comes from oow (w buffer emulation).
static void MakeVertex(const GrVertex *gv, GLVERTEX *dv)
{
	FillVertex(gv, dv);

	if (gl_depth_bias != 0.0f)
		dv->z = 1.0f - gl_screen_scale64 / (gl_screen_scale / gv->oow + gl_depth_bias);
	else
		dv->z = 1.0f - gv->oow * 64.0f;
}

// MAKE_VERTEX_REAL3D, for the game's own polygons: the bias goes on oow itself
static void MakeVertexReal3D(const GrVertex *gv, GLVERTEX *dv)
{
	FillVertex(gv, dv);

	if (gl_depth_bias != 0.0f)
		dv->z = 1.0f - gl_screen_scale64 / (gv->oow + gl_depth_bias);
	else
		dv->z = 1.0f - gv->oow * 64.0f;
}

// Room in the batch of the current texture set
// NOTE: glide2x.c doesn't check, a batch that fills up runs over into the next one
static GLVERTEX *BatchVertices(int count)
{
	GLVERTEX *v;

	if (gl_vbcount[gl_tset] + count > MAX_VERTICES)
		FlushBatchedPrims();

	v = &gl_vb[gl_tset][gl_vbcount[gl_tset]];

	gl_vbcount[gl_tset] += count;

	return v;
}

static void SetShaderMode(int mode)
{
	if (mode != gl_shader_mode)
	{
		gl_shader_mode = mode;
		glUniform1i(gl_uniform_mode, mode);
	}
}

static int TextureStageMode(void)
{
	if (!gl_modulate)
		return MODE_DIFFUSE;

	return gl_colorarg_diffuse ? MODE_DIFFUSE2 : MODE_TEXTURE;
}

static void DrawVertices(GLenum primitive, const GLVERTEX *v, int count)
{
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(GLVERTEX), v, GL_STREAM_DRAW);
	glDrawArrays(primitive, 0, count);
}

// [A]
// OPENDRIVER_PIXEL_LIGHTS: before a triangle the lamps light (the mark of AddLampLight) is batched,
// gives the shader the lamps as they are now (AddLamp in object.c, in camera space). When they
// changed since the last time, the batches go first with the old ones: the rear view mirror
// inverts the lamps (InvertLamps) while the main view's polygons still wait.
static void SyncLamps(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
#ifdef OPENDRIVER_PIXEL_LIGHTS
	float position[MAX_LAMPS][4];
	float colour[MAX_LAMPS][3];
	int count;
	int i;

	if (a->tmuvtx[1].oow != PIXEL_LAMP_MARK && b->tmuvtx[1].oow != PIXEL_LAMP_MARK && c->tmuvtx[1].oow != PIXEL_LAMP_MARK)
		return;

	count = num_lamps < MAX_LAMPS ? num_lamps : MAX_LAMPS;

	memset(position, 0, sizeof(position));
	memset(colour, 0, sizeof(colour));

	for (i = 0; i < count; i++)
	{
		position[i][0] = lamps[i].position.vx;
		position[i][1] = lamps[i].position.vy;
		position[i][2] = lamps[i].position.vz;
		position[i][3] = lamps[i].scale;

		colour[i][0] = lamps[i].color.vx / 255.0f;
		colour[i][1] = lamps[i].color.vy / 255.0f;
		colour[i][2] = lamps[i].color.vz / 255.0f;
	}

	if (count == gl_lamp_count && memcmp(position, gl_lamp_position, sizeof(position)) == 0 &&
		memcmp(colour, gl_lamp_colour, sizeof(colour)) == 0)
		return;

	FlushBatchedPrims();

	gl_lamp_count = count;
	memcpy(gl_lamp_position, position, sizeof(position));
	memcpy(gl_lamp_colour, colour, sizeof(colour));

	glUniform1i(gl_uniform_lamp_count, count);

	if (count)
	{
		glUniform4fv(gl_uniform_lamp_position, count, &position[0][0]);
		glUniform3fv(gl_uniform_lamp_colour, count, &colour[0][0]);
	}
#endif
}

// Draws the triangles batched per texture set (the last one holds untextured ones)
static void FlushBatchedPrims(void)
{
	int tset;

	if (!gl_active)
		return;

	for (tset = 0; tset < MAX_TEXTURES + 1; tset++)
	{
		if (gl_vbcount[tset])
		{
			if (tset == MAX_TEXTURES)
			{
				SetShaderMode(MODE_DIFFUSE);
			}
			else
			{
				SetShaderMode(TextureStageMode());

				if (gl_textures[tset].emulated)
					SetEmulatedTexture(tset);
				else
					SelectImage(&gl_textures[tset].image);
			}

			DrawVertices(GL_TRIANGLES, gl_vb[tset], gl_vbcount[tset]);

			gl_vbcount[tset] = 0;
		}
	}
}

// A line doesn't wait for the batches and uses the texture state as it is
static void DrawLineVertices(const GLVERTEX v[2])
{
	SetShaderMode(TextureStageMode());
	DrawVertices(GL_LINES, v, 2);
}

static void FX_CALL gl_DrawLine(const GrVertex *v1, const GrVertex *v2)
{
	GLVERTEX v[2];

	if (gl_active)
	{
		MakeVertex(v1, &v[0]);
		MakeVertex(v2, &v[1]);

		DrawLineVertices(v);
	}
}

static void FX_CALL gl_DrawLine_Real3D(const GrVertex *v1, const GrVertex *v2)
{
	GLVERTEX v[2];

	if (gl_active)
	{
		MakeVertexReal3D(v1, &v[0]);
		MakeVertexReal3D(v2, &v[1]);

		DrawLineVertices(v);
	}
}

// A quad as two triangles; nverts is ignored, as on the D3D path
// NOTE: [A] SyncLamps for OPENDRIVER_PIXEL_LIGHTS (the lamps mark all 4 vertices or none)
static void FX_CALL gl_DrawPolygonVertexList(int nverts, const GrVertex vlist[])
{
	GLVERTEX *v;

	if (gl_active)
	{
		SyncLamps(&vlist[0], &vlist[1], &vlist[2]);

		v = BatchVertices(6);

		MakeVertex(&vlist[0], &v[0]);
		MakeVertex(&vlist[1], &v[1]);
		MakeVertex(&vlist[2], &v[2]);

		v[3] = v[0];
		v[4] = v[2];

		MakeVertex(&vlist[3], &v[5]);
	}
}

// NOTE: [A] SyncLamps for OPENDRIVER_PIXEL_LIGHTS
static void FX_CALL gl_DrawPolygonVertexList_Real3D(int nverts, const GrVertex vlist[])
{
	GLVERTEX *v;

	if (gl_active)
	{
		SyncLamps(&vlist[0], &vlist[1], &vlist[2]);

		v = BatchVertices(6);

		MakeVertexReal3D(&vlist[0], &v[0]);
		MakeVertexReal3D(&vlist[1], &v[1]);
		MakeVertexReal3D(&vlist[2], &v[2]);

		v[3] = v[0];
		v[4] = v[2];

		MakeVertexReal3D(&vlist[3], &v[5]);
	}
}

// Also guDrawTriangleWithClip: the clipping is OpenGL's
// NOTE: [A] SyncLamps for OPENDRIVER_PIXEL_LIGHTS
static void FX_CALL gl_DrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	GLVERTEX *v;

	if (gl_active)
	{
		SyncLamps(a, b, c);

		v = BatchVertices(3);

		MakeVertex(a, &v[0]);
		MakeVertex(b, &v[1]);
		MakeVertex(c, &v[2]);
	}
}

// gDrawTriangle and gDrawTriangleWithClip
// NOTE: [A] SyncLamps for OPENDRIVER_PIXEL_LIGHTS
static void FX_CALL gl_DrawTriangle_Real3D(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
	GLVERTEX *v;

	if (gl_active)
	{
		SyncLamps(a, b, c);

		v = BatchVertices(3);

		MakeVertexReal3D(a, &v[0]);
		MakeVertexReal3D(b, &v[1]);
		MakeVertexReal3D(c, &v[2]);
	}
}

//------------------------------------------------------------------------------------------
// What the D3D path leaves empty

static void FX_CALL gl_AADrawLine(const GrVertex *v1, const GrVertex *v2)
{
}

static void FX_CALL gl_AADrawPoint(const GrVertex *pt)
{
}

static void FX_CALL gl_AADrawPolygon(const int nverts, const int ilist[], const GrVertex vlist[])
{
}

static void FX_CALL gl_AADrawPolygonVertexList(const int nverts, const GrVertex vlist[])
{
}

static void FX_CALL gl_AADrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c, FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias)
{
}

static void FX_CALL gl_AlphaControlsITRGBLighting(FxBool enable)
{
}

static int FX_CALL gl_BufferNumPending(void)
{
	return 0;
}

static void FX_CALL gl_CheckForRoom(FxI32 n)
{
}

static void FX_CALL gl_ChromakeyMode(GrChromakeyMode_t mode)
{
}

static void FX_CALL gl_ChromakeyValue(GrColor_t value)
{
}

static void FX_CALL gl_ConstantColorValue4(float a, float r, float g, float b)
{
}

static void FX_CALL gl_ConstantColorValue(GrColor_t value)
{
}

static void FX_CALL gl_DisableAllEffects(void)
{
}

static void FX_CALL gl_DitherMode(GrDitherMode_t mode)
{
}

static void FX_CALL gl_DrawPlanarPolygon(int nverts, const int ilist[], const GrVertex vlist[])
{
}

static void FX_CALL gl_DrawPlanarPolygonVertexList(int nverts, const GrVertex vlist[])
{
}

static void FX_CALL gl_DrawPoint(const GrVertex *pt)
{
}

static void FX_CALL gl_DrawPolygon(int nverts, const int ilist[], const GrVertex vlist[])
{
}

// ARGB, the colour format of the device
static void FX_CALL gl_FogColorValue(GrColor_t fogcolor)
{
	if (gl_active)
	{
		FlushBatchedPrims();

		glUniform3f(gl_uniform_fog_colour, ((fogcolor >> 16) & 0xFF) / 255.0f, ((fogcolor >> 8) & 0xFF) / 255.0f, (fogcolor & 0xFF) / 255.0f);
	}
}

static void FX_CALL gl_FogMode(GrFogMode_t mode)
{
	if (gl_active)
	{
		FlushBatchedPrims();

		gl_fog_on = mode != GR_FOG_DISABLE;

		UpdateFog();
	}
}

// What is drawn next is all fog colour, whatever its w, for as long as fog is on
void gl_SetFullFog(int full)
{
	if (gl_active)
	{
		FlushBatchedPrims();

		gl_fog_full = full;

		UpdateFog();
	}
}

static void FX_CALL gl_FogTable(const GrFog_t ft[64])
{
	float table[GR_FOG_TABLE_SIZE];
	int i;

	if (gl_active)
	{
		FlushBatchedPrims();

		for (i = 0; i < GR_FOG_TABLE_SIZE; i++)
			table[i] = ft[i] / 255.0f;

		glUniform1fv(gl_uniform_fog_table, GR_FOG_TABLE_SIZE, table);
	}
}

static void FX_CALL gl_GammaCorrectionValue(float value)
{
}

static void FX_CALL gl_GlideGetState(GrState *state)
{
}

static void FX_CALL gl_GlideSetState(const GrState *state)
{
}

static void FX_CALL gl_GlideShamelessPlug(const FxBool on)
{
}

static void FX_CALL gl_Hints(GrHint_t hintType, FxU32 hintMask)
{
}

static void FX_CALL gl_LfbConstantAlpha(GrAlpha_t alpha)
{
}

static void FX_CALL gl_LfbConstantDepth(FxU16 depth)
{
}

static FxBool FX_CALL gl_LfbReadRegion(GrBuffer_t src_buffer, FxU32 src_x, FxU32 src_y, FxU32 src_width, FxU32 src_height, FxU32 dst_stride, void *dst_data)
{
	return 0;
}

static void FX_CALL gl_LfbWriteColorFormat(GrColorFormat_t colorFormat)
{
}

static void FX_CALL gl_LfbWriteColorSwizzle(FxBool swizzleBytes, FxBool swapWords)
{
}

static FxBool FX_CALL gl_LfbWriteRegion(GrBuffer_t dst_buffer, FxU32 dst_x, FxU32 dst_y, GrLfbSrcFmt_t src_format, FxU32 src_width, FxU32 src_height, FxI32 src_stride, void *src_data)
{
	return 0;
}

static void FX_CALL gl_RenderBuffer(GrBuffer_t buffer)
{
}

static void FX_CALL gl_ResetTriStats()
{
}

static void FX_CALL gl_Splash(float x, float y, float width, float height, FxU32 frame)
{
}

static void FX_CALL gl_SstIdle(void)
{
}

static FxBool FX_CALL gl_SstIsBusy(void)
{
	return 0;
}

static void FX_CALL gl_SstOrigin(GrOriginLocation_t origin)
{
}

// Only pixelsOut, for the sun in DrawLensFlare: the pixels drawn since gl_SstResetPerfStats. The game
// waits for it as it does on the board.
static void FX_CALL gl_SstPerfStats(GrSstPerfStats_t *pStats)
{
	GLuint pixels;

	memset(pStats, 0, sizeof(GrSstPerfStats_t));

	if (!gl_query_active)
		return;

	FlushBatchedPrims();

	glEndQuery(GL_SAMPLES_PASSED);
	glGetQueryObjectuiv(gl_query, GL_QUERY_RESULT, &pixels);

	glDepthMask(gl_depth_masked ? GL_FALSE : GL_TRUE);

	gl_query_active = 0;

	pStats->pixelsOut = pixels;
}

// NOTE: what is drawn for the count leaves no depth behind. The square of the sun is a little closer
// than the flare drawn after it, which would lose those pixels.
static void FX_CALL gl_SstResetPerfStats(void)
{
	if (!gl_active || gl_query_active)
		return;

	FlushBatchedPrims();

	glDepthMask(GL_FALSE);
	glBeginQuery(GL_SAMPLES_PASSED, gl_query);

	gl_query_active = 1;
}

static void FX_CALL gl_SstSelect(int which_sst)
{
}

static FxU32 FX_CALL gl_SstStatus(void)
{
	return 0;
}

static FxBool FX_CALL gl_SstVRetraceOn(void)
{
	return 0;
}

static FxU32 FX_CALL gl_SstVideoLine(void)
{
	return 0;
}

static FxU32 FX_CALL gl_TexCalcMemRequired(GrLOD_t lodmin, GrLOD_t lodmax, GrAspectRatio_t aspect, GrTextureFormat_t fmt)
{
	return 0;
}

static void FX_CALL gl_TexClampMode(GrChipID_t tmu, GrTextureClampMode_t s_clampmode, GrTextureClampMode_t t_clampmode)
{
}

static void FX_CALL gl_TexCombine(GrChipID_t tmu, GrCombineFunction_t rgb_function, GrCombineFactor_t rgb_factor, GrCombineFunction_t alpha_function, GrCombineFactor_t alpha_factor, FxBool rgb_invert, FxBool alpha_invert)
{
}

static void FX_CALL gl_TexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc)
{
}

static void FX_CALL gl_TexDetailControl(GrChipID_t tmu, int lod_bias, FxU8 detail_scale, float detail_max)
{
}

static void FX_CALL gl_TexDownloadMipMapLevel(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data)
{
}

static void FX_CALL gl_TexDownloadMipMapLevelPartial(GrChipID_t tmu, FxU32 startAddress, GrLOD_t thisLod, GrLOD_t largeLod, GrAspectRatio_t aspectRatio, GrTextureFormat_t format, FxU32 evenOdd, void *data, int start, int end)
{
}

static void FX_CALL gl_TexDownloadTablePartial(GrChipID_t tmu, GrTexTable_t type, void *data, int start, int end)
{
}

static void FX_CALL gl_TexLodBiasValue(GrChipID_t tmu, float bias)
{
}

static FxU32 FX_CALL gl_TexMinAddress(GrChipID_t tmu)
{
	return 0;
}

static void FX_CALL gl_TexMipMapMode(GrChipID_t tmu, GrMipMapMode_t mode, FxBool lodBlend)
{
}

static void FX_CALL gl_TexMultibase(GrChipID_t tmu, FxBool enable)
{
}

static void FX_CALL gl_TexMultibaseAddress(GrChipID_t tmu, GrTexBaseRange_t range, FxU32 startAddress, FxU32 evenOdd, GrTexInfo *info)
{
}

static void FX_CALL gl_TexNCCTable(GrChipID_t tmu, GrNCCTable_t table)
{
}

static FxU32 FX_CALL gl_TexTextureMemRequired(FxU32 evenOdd, GrTexInfo *info)
{
	return 0;
}

static void FX_CALL gl_TriStats(FxU32 *trisProcessed, FxU32 *trisDrawn)
{
}

static FxBool FX_CALL glgu_3dfGetInfo(const char *filename, Gu3dfInfo *info)
{
	return 0;
}

static FxBool FX_CALL glgu_3dfLoad(const char *filename, Gu3dfInfo *data)
{
	return 0;
}

static void FX_CALL glgu_AADrawTriangleWithClip(const GrVertex *a, const GrVertex *b, const GrVertex *c)
{
}

static void FX_CALL glgu_AlphaSource(GrAlphaSource_t mode)
{
}

static void FX_CALL glgu_ColorCombineFunction(GrColorCombineFnc_t fnc)
{
}

static void FX_CALL glgu_DrawPolygonVertexListWithClip(int nverts, const GrVertex vlist[])
{
}

static int FX_CALL glgu_EncodeRLE16(void *dst, void *src, FxU32 width, FxU32 height)
{
	return 0;
}

static FxU16 FX_CALL glgu_EndianSwapBytes(FxU16 value)
{
	return 0;
}

static FxU32 FX_CALL glgu_EndianSwapWords(FxU32 value)
{
	return 0;
}

static float FX_CALL glgu_FogTableIndexToW(int i)
{
	return (float)(pow(2.0, 3.0 + (double)(i >> 2)) / (double)(8 - (i & 3)));
}

static void FX_CALL glgu_FogGenerateExp2(GrFog_t fogtable[64], float density)
{
}

// As in the Glide utilities: scaled so that the last entry is all fog
static void FX_CALL glgu_FogGenerateExp(GrFog_t fogtable[64], float density)
{
	float scale;
	float fog;
	int i;

	scale = 255.0f / (1.0f - (float)exp(-density * glgu_FogTableIndexToW(GR_FOG_TABLE_SIZE - 1)));

	for (i = 0; i < GR_FOG_TABLE_SIZE; i++)
	{
		fog = (1.0f - (float)exp(-density * glgu_FogTableIndexToW(i))) * scale;

		if (fog > 255.0f)
			fog = 255.0f;

		fogtable[i] = (GrFog_t)fog;
	}
}

static void FX_CALL glgu_FogGenerateLinear(GrFog_t fogtable[64], float nearZ, float farZ)
{
}

static GrMipMapId_t FX_CALL glgu_TexAllocateMemory(GrChipID_t tmu, FxU8 odd_even_mask, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minfilter_mode, GrTextureFilterMode_t magfilter_mode, float lod_bias, FxBool trilinear)
{
	return 0;
}

static FxBool FX_CALL glgu_TexChangeAttributes(GrMipMapId_t mmid, int width, int height, GrTextureFormat_t fmt, GrMipMapMode_t mm_mode, GrLOD_t smallest_lod, GrLOD_t largest_lod, GrAspectRatio_t aspect, GrTextureClampMode_t s_clamp_mode, GrTextureClampMode_t t_clamp_mode, GrTextureFilterMode_t minFilterMode, GrTextureFilterMode_t magFilterMode)
{
	return 0;
}

static void FX_CALL glgu_TexCombineFunction(GrChipID_t tmu, GrTextureCombineFnc_t fnc)
{
}

static FxU16 * FX_CALL glgu_TexCreateColorMipMap(void)
{
	return 0;
}

static void FX_CALL glgu_TexDownloadMipMap(GrMipMapId_t mmid, const void *src, const GuNccTable *table)
{
}

static void FX_CALL glgu_TexDownloadMipMapLevel(GrMipMapId_t mmid, GrLOD_t lod, const void **src)
{
}

static GrMipMapId_t FX_CALL glgu_TexGetCurrentMipMap(GrChipID_t tmu)
{
	return 0;
}

static GrMipMapInfo * FX_CALL glgu_TexGetMipMapInfo(GrMipMapId_t mmid)
{
	return 0;
}

static FxU32 FX_CALL glgu_TexMemQueryAvail(GrChipID_t tmu)
{
	return 0;
}

static void FX_CALL glgu_TexMemReset(void)
{
}

static void FX_CALL glgu_TexSource(GrMipMapId_t id)
{
}

static void FX_CALL gl_GlideInit(void)
{
}

//------------------------------------------------------------------------------------------
// The device

// Fills the pointer table
int UseGLDevice(void)
{
	int i;

	grAADrawLine = gl_AADrawLine;
	grAADrawPoint = gl_AADrawPoint;
	grAADrawPolygon = gl_AADrawPolygon;
	grAADrawPolygonVertexList = gl_AADrawPolygonVertexList;
	grAADrawTriangle = gl_AADrawTriangle;
	grAlphaBlendFunction = gl_AlphaBlendFunction;
	grAlphaCombine = gl_AlphaCombine;
	grAlphaControlsITRGBLighting = gl_AlphaControlsITRGBLighting;
	grAlphaTestFunction = gl_AlphaTestFunction;
	grAlphaTestReferenceValue = gl_AlphaTestReferenceValue;
	grBufferClear = gl_BufferClear;
	grBufferNumPending = gl_BufferNumPending;
	grBufferSwap = gl_BufferSwap;
	grCheckForRoom = gl_CheckForRoom;
	grChromakeyMode = gl_ChromakeyMode;
	grChromakeyValue = gl_ChromakeyValue;
	grClipWindow = gl_ClipWindow;
	grColorCombine = gl_ColorCombine;
	grColorMask = gl_ColorMask;
	grConstantColorValue4 = gl_ConstantColorValue4;
	grConstantColorValue = gl_ConstantColorValue;
	grCullMode = gl_CullMode;
	grDepthBiasLevel = gl_DepthBiasLevel;
	grDepthBufferFunction = gl_DepthBufferFunction;
	grDepthBufferMode = gl_DepthBufferMode;
	grDepthMask = gl_DepthMask;
	grDisableAllEffects = gl_DisableAllEffects;
	grDitherMode = gl_DitherMode;
	grDrawLine = gl_DrawLine;
	grDrawPlanarPolygon = gl_DrawPlanarPolygon;
	grDrawPlanarPolygonVertexList = gl_DrawPlanarPolygonVertexList;
	grDrawPoint = gl_DrawPoint;
	grDrawPolygon = gl_DrawPolygon;
	grDrawPolygonVertexList = gl_DrawPolygonVertexList;
	grDrawTriangle = gl_DrawTriangle;
	grFogColorValue = gl_FogColorValue;
	grFogMode = gl_FogMode;
	grFogTable = gl_FogTable;
	grGammaCorrectionValue = gl_GammaCorrectionValue;
	grGlideGetState = gl_GlideGetState;
	grGlideGetVersion = gl_GlideGetVersion;
	grGlideSetState = gl_GlideSetState;
	grGlideShamelessPlug = gl_GlideShamelessPlug;
	grHints = gl_Hints;
	grLfbConstantAlpha = gl_LfbConstantAlpha;
	grLfbConstantDepth = gl_LfbConstantDepth;
	gr_LfbLock = gl_LfbLock;
	grLfbReadRegion = gl_LfbReadRegion;
	gr_LfbUnlock = gl_LfbUnlock;
	grLfbWriteColorFormat = gl_LfbWriteColorFormat;
	grLfbWriteColorSwizzle = gl_LfbWriteColorSwizzle;
	grLfbWriteRegion = gl_LfbWriteRegion;
	grRenderBuffer = gl_RenderBuffer;
	grResetTriStats = gl_ResetTriStats;
	grSplash = gl_Splash;
	grSstIdle = gl_SstIdle;
	grSstIsBusy = gl_SstIsBusy;
	grSstOrigin = gl_SstOrigin;
	grSstPerfStats = gl_SstPerfStats;
	grSstQueryBoards = gl_SstQueryBoards;
	grSstResetPerfStats = gl_SstResetPerfStats;
	grSstScreenHeight = gl_SstScreenHeight;
	grSstScreenWidth = gl_SstScreenWidth;
	grSstSelect = gl_SstSelect;
	grSstStatus = gl_SstStatus;
	grSstVRetraceOn = gl_SstVRetraceOn;
	grSstVideoLine = gl_SstVideoLine;
	grTexCalcMemRequired = gl_TexCalcMemRequired;
	grTexClampMode = gl_TexClampMode;
	grTexCombine = gl_TexCombine;
	grTexCombineFunction = gl_TexCombineFunction;
	grTexDetailControl = gl_TexDetailControl;
	grTexDownloadMipMap = gl_TexDownloadMipMap;
	grTexDownloadMipMapLevel = gl_TexDownloadMipMapLevel;
	grTexDownloadMipMapLevelPartial = gl_TexDownloadMipMapLevelPartial;
	grTexDownloadTable = gl_TexDownloadTable;
	grTexDownloadTablePartial = gl_TexDownloadTablePartial;
	grTexFilterMode = gl_TexFilterMode;
	grTexLodBiasValue = gl_TexLodBiasValue;
	grTexMaxAddress = gl_TexMaxAddress;
	grTexMinAddress = gl_TexMinAddress;
	grTexMipMapMode = gl_TexMipMapMode;
	grTexMultibase = gl_TexMultibase;
	grTexMultibaseAddress = gl_TexMultibaseAddress;
	grTexNCCTable = gl_TexNCCTable;
	grTexSource = gl_TexSource;
	grTexTextureMemRequired = gl_TexTextureMemRequired;
	grTriStats = gl_TriStats;
	gu3dfGetInfo = glgu_3dfGetInfo;
	gu3dfLoad = glgu_3dfLoad;
	guAADrawTriangleWithClip = glgu_AADrawTriangleWithClip;
	guAlphaSource = glgu_AlphaSource;
	guColorCombineFunction = glgu_ColorCombineFunction;
	guDrawPolygonVertexListWithClip = glgu_DrawPolygonVertexListWithClip;
	guDrawTriangleWithClip = gl_DrawTriangle;
	guEncodeRLE16 = glgu_EncodeRLE16;
	guEndianSwapBytes = glgu_EndianSwapBytes;
	guEndianSwapWords = glgu_EndianSwapWords;
	guFogGenerateExp2 = glgu_FogGenerateExp2;
	guFogGenerateExp = glgu_FogGenerateExp;
	guFogGenerateLinear = glgu_FogGenerateLinear;
	guFogTableIndexToW = glgu_FogTableIndexToW;
	guTexAllocateMemory = glgu_TexAllocateMemory;
	guTexChangeAttributes = glgu_TexChangeAttributes;
	guTexCombineFunction = glgu_TexCombineFunction;
	guTexCreateColorMipMap = glgu_TexCreateColorMipMap;
	gu_TexDownloadMipMap = glgu_TexDownloadMipMap;
	guTexDownloadMipMapLevel = glgu_TexDownloadMipMapLevel;
	guTexGetCurrentMipMap = glgu_TexGetCurrentMipMap;
	guTexGetMipMapInfo = glgu_TexGetMipMapInfo;
	guTexMemQueryAvail = glgu_TexMemQueryAvail;
	guTexMemReset = glgu_TexMemReset;
	guTexSource = glgu_TexSource;
	gr_GlideInit = gl_GlideInit;
	gr_GlideShutdown = gl_GlideShutdown;
	gr_SstQueryHardware = gl_SstQueryHardware;
	gr_SstWinOpen = gl_SstWinOpen;
	gr_SstWinClose = gl_SstWinClose;
	gr_SstControl = gl_SstControl;

	gDrawLine = gl_DrawLine_Real3D;
	gDrawPolygonVertexList = gl_DrawPolygonVertexList_Real3D;
	gDrawTriangle = gl_DrawTriangle_Real3D;
	gDrawTriangleWithClip = gl_DrawTriangle_Real3D;

	for (i = 0; i < 65536; i++)
		gl_texlookup16[i] = (unsigned short)i;

	return 1;
}

// ARGB1555
void gl_GetHWSettings(HARDWARE_SETTINGS *settings)
{
	settings->rbits = 3;
	settings->rshift = 10;
	settings->gbits = 3;
	settings->gshift = 5;
	settings->bbits = 3;
	settings->bshift = 0;
	settings->abits = 7;
	settings->ashift = 15;
	settings->emulate_8bit = 1;
	settings->device_flags = -1;
	settings->texlookup16 = gl_texlookup16;
}
