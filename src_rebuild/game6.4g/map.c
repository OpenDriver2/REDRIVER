/*
 * map.c - level map, models and textures
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "map.c"); partially translated.
 */

#include <string.h>

typedef unsigned char uint8_t;

typedef struct TEX_INFO
{
	char name[8];
	uint8_t tset;
	uint8_t u;
	uint8_t v;
	uint8_t w;
	uint8_t h;
	uint8_t pad1;
	uint8_t pad2;
	uint8_t pad3;
} TEX_INFO;

#define TEX_NAME_LEN	8

// NOTE: bss globals in the linker's common area; defined here provisionally (main users are in map.c)
TEX_INFO *tinfo_block;
int TSetsToLoad;
int num_textures;

// [0x4eea3e] [D]
TEX_INFO *GetTextureDetails(char *tex_name)
{
	TEX_INFO *info;
	int idx;
	int name_len;
	int valid;
	int loop;

	if (!num_textures)
		return NULL;

	name_len = strlen(tex_name);

	if (!name_len)
		return NULL;

	if (name_len > TEX_NAME_LEN)
		name_len = TEX_NAME_LEN;

	loop = num_textures;
	info = tinfo_block;

	if (!info)
		return NULL;

	while (loop--)
	{
		valid = 1;

		for (idx = 0; idx < name_len; idx++)
		{
			if (tex_name[idx] != info->name[idx])
			{
				valid = 0;
				break;
			}
		}

		// names are 8 chars, zero padded
		if (valid && (idx == TEX_NAME_LEN || info->name[idx] == 0))
			return info;

		info++;
	}

	return NULL;
}
