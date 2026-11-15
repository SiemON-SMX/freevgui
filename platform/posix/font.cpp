// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2024-2026 Alibek Omarov

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "platform/common/font.h"
#include "vgui_internal.h"

using namespace vgui;

namespace
{
struct FontFile
{
	unsigned char *data;
	stbtt_fontinfo info;
	bool tried;
	bool ok;
};

FontFile g_regularFile = { nullptr, {}, false, false };
FontFile g_boldFile = { nullptr, {}, false, false };

const char *const g_regularPaths[] =
{
	"/system/fonts/Roboto-Regular.ttf",
	"/system/fonts/NotoSans-Regular.ttf",
	"/system/fonts/DroidSans.ttf",
	"/system/fonts/Roboto[wdth,wght].ttf",
	"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	"/usr/share/fonts/TTF/DejaVuSans.ttf",
	"/usr/share/fonts/dejavu/DejaVuSans.ttf",
	nullptr
};

const char *const g_boldPaths[] =
{
	"/system/fonts/Roboto-Bold.ttf",
	"/system/fonts/NotoSans-Bold.ttf",
	"/system/fonts/DroidSans-Bold.ttf",
	"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
	"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
	"/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
	nullptr
};

bool loadFontFile( FontFile &file, const char *const *paths )
{
	if( file.tried )
		return file.ok;

	file.tried = true;

	for( int i = 0; paths[i]; i++ )
	{
		FILE *fp = fopen( paths[i], "rb" );

		if( !fp )
			continue;

		fseek( fp, 0, SEEK_END );
		long size = ftell( fp );
		fseek( fp, 0, SEEK_SET );

		if( size <= 0 )
		{
			fclose( fp );
			continue;
		}

		unsigned char *data = new unsigned char[size];

		if( fread( data, 1, size, fp ) != (size_t)size )
		{
			delete[] data;
			fclose( fp );
			continue;
		}

		fclose( fp );

		int offset = stbtt_GetFontOffsetForIndex( data, 0 );

		if( offset < 0 || !stbtt_InitFont( &file.info, data, offset ))
		{
			delete[] data;
			continue;
		}

		file.data = data;
		file.ok = true;
		vgui_dprintf( "vgui: using system font %s\n", paths[i] );
		return true;
	}

	return false;
}

class FontPlat_Stb : public BaseFontPlat
{
public:
	static FontPlat_Stb *Create( const char *name, int tall, int wide, int weight, bool italic )
	{
		FontFile *file = nullptr;

		if( weight >= 600 && loadFontFile( g_boldFile, g_boldPaths ))
			file = &g_boldFile;
		else if( loadFontFile( g_regularFile, g_regularPaths ))
			file = &g_regularFile;

		if( !file )
			return nullptr;

		FontPlat_Stb *font = new FontPlat_Stb();
		font->file = file;
		font->name = vgui_strdup( name ? name : "" );
		font->requestTall = tall;
		font->requestWide = wide;
		font->weight = weight;
		font->italic = italic;

		int ascent, descent, lineGap;
		stbtt_GetFontVMetrics( &file->info, &ascent, &descent, &lineGap );

		if( tall >= 0 )
			font->scale = stbtt_ScaleForPixelHeight( &file->info, (float)( tall > 0 ? tall : 12 ));
		else
			font->scale = stbtt_ScaleForMappingEmToPixels( &file->info, (float)( -tall ));

		font->baseline = (int)floorf( ascent * font->scale + 0.5f );
		font->cellTall = (int)ceilf(( ascent - descent ) * font->scale );

		if( font->cellTall < 1 )
			font->cellTall = 1;

		int x0, y0, x1, y1;
		stbtt_GetFontBoundingBox( &file->info, &x0, &y0, &x1, &y1 );
		font->maxWide = (int)ceilf(( x1 - x0 ) * font->scale );

		if( font->maxWide < 1 )
			font->maxWide = font->cellTall;

		return font;
	}

	virtual ~FontPlat_Stb() override
	{
		delete[] name;
	}

	virtual bool equals( const char *newName, int newTall, int newWide, float, int newWeight, bool newItalic, bool, bool, bool ) override
	{
		return newName && !strcmp( name, newName ) && requestTall == newTall && requestWide == newWide
			&& weight == newWeight && italic == newItalic;
	}

	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba ) override
	{
		if( !rgba || isBlank( ch ))
			return;

		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox( &file->info, ch, scale, scale, &x0, &y0, &x1, &y1 );

		int glyphWide = x1 - x0, glyphTall = y1 - y0;

		if( glyphWide <= 0 || glyphTall <= 0 )
			return;

		unsigned char *coverage = new unsigned char[glyphWide * glyphTall];
		memset( coverage, 0, glyphWide * glyphTall );
		stbtt_MakeCodepointBitmap( &file->info, coverage, glyphWide, glyphTall, glyphWide, scale, scale, ch );

		for( int j = 0; j < glyphTall; j++ )
		{
			int cellY = baseline + y0 + j;

			if( cellY < 0 || cellY >= cellTall )
				continue;

			for( int i = 0; i < glyphWide; i++ )
			{
				int outX = rgbaX + i, outY = rgbaY + cellY;

				if( outX < 0 || outY < 0 || outX >= rgbaWide || outY >= rgbaTall )
					continue;

				unsigned char *dst = &rgba[( outY * rgbaWide + outX ) * 4];
				dst[0] = dst[1] = dst[2] = 255;
				dst[3] = coverage[j * glyphWide + i];
			}
		}

		delete[] coverage;
	}

	virtual void getCharABCwide( int ch, int &a, int &b, int &c ) override
	{
		int advance, lsb;
		stbtt_GetCodepointHMetrics( &file->info, ch, &advance, &lsb );
		int advancePx = (int)floorf( advance * scale + 0.5f );

		if( isBlank( ch ))
		{
			a = b = 0;
			c = advancePx;
			return;
		}

		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox( &file->info, ch, scale, scale, &x0, &y0, &x1, &y1 );

		a = x0;
		b = x1 - x0;
		c = advancePx - a - b;
	}

	virtual int getTall() override
	{
		return cellTall;
	}

	virtual int getWide() override
	{
		return maxWide;
	}

	virtual void drawSetTextFont( SurfacePlat * ) override
	{
	}

private:
	FontPlat_Stb() : file( nullptr ), name( nullptr ), requestTall( 0 ), requestWide( 0 ), weight( 0 ),
		italic( false ), scale( 1.0f ), baseline( 0 ), cellTall( 0 ), maxWide( 0 )
	{
	}

	static bool isBlank( int ch )
	{
		return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == 0xA0;
	}

	FontFile *file;
	char *name;
	int requestTall, requestWide, weight;
	bool italic;
	float scale;
	int baseline, cellTall, maxWide;
};
}

BaseFontPlat *vgui::FontPlat_CreateSystem( const char *name, int tall, int wide, float, int weight, bool italic, bool, bool, bool )
{
	return FontPlat_Stb::Create( name, tall, wide, weight, italic );
}
