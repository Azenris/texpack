
// System Includes
#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <chrono>

// Third Party Includes
#pragma warning( push )
#pragma warning( disable : 4505 )
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_rect_pack.h"
#pragma warning( pop )

// Includes
#include "types.h"

namespace fs = std::filesystem;

#ifdef WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

enum RETURN_CODE
{
	RETURN_CODE_SUCCESS,
	RETURN_CODE_INVALID_ARGUMENTS,
	RETURN_CODE_FAILED_TO_PACK_ALL,
	RETURN_CODE_FAILED_TO_OPEN_DIRECTORY,
	RETURN_CODE_FAILED_TO_OPEN_IMAGE,
	RETURN_CODE_FAILED_TO_CREATE_DATA_FILE,
	RETURN_CODE_FAILED_TEXTURE_NAME_TOO_BIG,
	RETURN_CODE_FAILED_SPRITE_NAME_TOO_BIG,
};

RETURN_CODE usage( RETURN_CODE code )
{
	std::cerr << "texpack usage:\n"
				 "texpack <folder> -o <output-folder> -w 4096 -h 4096 -pad 2\n"
				 "\n"
				 "-o <output-folder>  output folder \n"
				 "-w 4096             width of output textures \n"
				 "-h 4096             height of output textures \n"
				 "-pad 2              padding applied around the images" << std::endl;
	return code;
}

RETURN_CODE image_files( const char *path, std::vector<Image> &images, std::vector<stbrp_rect> &rects, Data *data )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	fs::path entrypath;
	std::string filepath;
	std::string filename;

	for ( const fs::directory_entry &entry : fs::recursive_directory_iterator( path ) )
	{
		entrypath = entry.path();
		filepath = entrypath.string();
		filename = entrypath.filename().string();

		if ( !entry.is_directory() )
		{
			rects.emplace_back();
			images.emplace_back();
			stbrp_rect *rect = &rects.back();
			Image *image = &images.back();

			if ( filename.size() > MAX_SPRITE_NAME )
				return RETURN_CODE_FAILED_SPRITE_NAME_TOO_BIG;

			image->filename = filename;

			image->img = stbi_load( filepath.c_str(), &rect->w, &rect->h, &image->channels, 4 );

			rect->w += data->padding * 2;
			rect->h += data->padding * 2;

			if ( !image->img )
				return RETURN_CODE_FAILED_TO_OPEN_IMAGE;
		}
	}

	return ret;
}

RETURN_CODE process_texturegroup( const char *path, Data *data )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	std::vector<Image> images;
	std::vector<stbrp_rect> rects;

	images.reserve( 1024 );
	rects.reserve( 1024 );

	ret = image_files( path, images, rects, data );
	if ( ret != RETURN_CODE_SUCCESS )
		return ret;

	stbrp_context context;

	std::vector<stbrp_node> nodes;
	nodes.resize( data->textureWidth );

	stbrp_init_target( &context, data->textureWidth, data->textureHeight, nodes.data(), (i32)nodes.size() );

	if ( stbrp_pack_rects( &context, rects.data(), (i32)rects.size() ) == 0 )
	{
		// TODO : in future could possible make another texture for the overflowed ones
		return RETURN_CODE_FAILED_TO_PACK_ALL;
	}

	u64 totalBytes = data->textureWidth * data->textureHeight * data->outputChannels;
	std::vector<u8> outputImage;
	outputImage.resize( totalBytes );

	{
		u8 *img = outputImage.data();
		for ( u64 i = 0; i < totalBytes; i += 4 )
		{
			*img++ = 255;
			*img++ = 0;
			*img++ = 255;
			*img++ = 0;
		}
	}

	std::string outputName = data->outputName;
	std::string textureName = fs::path( path ).filename().string();

	if ( textureName.size() > MAX_TEXTURE_NAME )
	{
		return RETURN_CODE_FAILED_TEXTURE_NAME_TOO_BIG;
	}

	outputName += PATH_SEP + textureName;

	std::ofstream dataFile( outputName + ".dat", std::ios::binary );
	if ( !dataFile.good() )
	{
		return RETURN_CODE_FAILED_TO_CREATE_DATA_FILE;
	}

	outputName += ".png";

	TexpackTexture texpackTexture;
	sprintf( texpackTexture.texture, "%s", textureName.c_str() );
	texpackTexture.size = { data->textureWidth, data->textureHeight };
	texpackTexture.numSprites = (u32)rects.size();

	std::vector<TexpackSprite> texpackSprite;

	f32 tw = (f32)data->textureWidth;
	f32 th = (f32)data->textureHeight;

	for ( u64 i = 0, count = rects.size(); i < count; ++i )
	{
		Image image = images[ i ];
		stbrp_rect rect = rects[ i ];

		i32 offX = rect.x + data->padding;
		i32 offY = rect.y + data->padding;
		i32 xCount = rect.w - data->padding * 2;
		i32 yCount = rect.h - data->padding * 2;
		bool isTranslucent = false;

		for ( i32 y = 0; y < yCount; ++y )
		{
			for ( i32 x = 0; x < xCount; ++x )
			{
				i32 from = ( ( offX + x ) + ( offY + y ) * data->textureWidth ) * data->outputChannels;
				i32 to = ( x + y * xCount ) * image.channels;

				outputImage[ from + 0 ] = image.img[ to + 0 ];
				outputImage[ from + 1 ] = image.img[ to + 1 ];
				outputImage[ from + 2 ] = image.img[ to + 2 ];
				outputImage[ from + 3 ] = image.img[ to + 3 ];

				isTranslucent = isTranslucent || ( image.img[ to + 3 ] != 0 && image.img[ to + 3 ] != 255 ) ;
			}
		}

		texpackSprite.emplace_back();
		TexpackSprite *spr = &texpackSprite.back();
		sprintf( spr->name, "%s", image.filename.c_str() );
		spr->uvs = { offX / tw, offY / th, ( offX + xCount ) / tw, ( offY + yCount ) / th };
		spr->size = { xCount, yCount };
		spr->isTranslucent = isTranslucent;

		stbi_image_free( image.img );
		image.img = nullptr;
	}

	std::cout << "Saving texture: " << outputName << std::endl;

	stbi_write_png( outputName.c_str(), data->textureWidth, data->textureHeight, data->outputChannels, outputImage.data(), data->textureWidth * data->outputChannels);

	dataFile.write( (char*)&texpackTexture, sizeof( texpackTexture ) );
	dataFile.write( (char*)&texpackSprite[ 0 ], texpackSprite.size() * sizeof( TexpackSprite ) );

	return ret;
}

int main( int argc, char *argv[] )
{
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

	if ( argc < 2 )
	{
		return usage( RETURN_CODE_INVALID_ARGUMENTS );
	}

	Data data;

	for ( u64 i = 2; i < argc; ++i )
	{
		if ( strcmp( argv[ i ], "-o" ) == 0 )
		{
			if ( i == argc - 1 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );

			data.outputName = argv[ i + 1 ];

			i += 1;
		}
		else if ( strcmp( argv[ i ], "-w" ) == 0 )
		{
			if ( i == argc - 1 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );
			data.textureWidth = atoi( argv[ i + 1 ] );
			if ( data.textureWidth == 0 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );

			i += 1;
		}
		else if ( strcmp( argv[ i ], "-h" ) == 0 )
		{
			if ( i == argc - 1 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );
			data.textureHeight = atoi( argv[ i + 1 ] );
			if ( data.textureHeight == 0 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );

			i += 1;
		}
		else if ( strcmp( argv[ i ], "-pad" ) == 0 )
		{
			if ( i == argc - 1 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );
			data.padding = atoi( argv[ i + 1 ] );

			i += 1;
		}
	}

	if ( data.textureWidth == 0 || data.textureHeight == 0 )
		return usage( RETURN_CODE_INVALID_ARGUMENTS );

	const char *inputPath = argv[ 1 ];

	std::cout << "Input: " << inputPath << std::endl;

	fs::create_directories( data.outputName );

	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	i32 numRects = 0;
	std::string filename;

	// Cycle the top layer of folders (These are the texturegroups)
	for ( const auto &entry : fs::directory_iterator( inputPath ) )
	{
		filename = entry.path().filename().string();

		if ( entry.is_directory() )
		{
			if ( filename != "." && filename != ".." )
			{
				ret = process_texturegroup( entry.path().string().c_str(), &data);

				if ( ret != RETURN_CODE_SUCCESS )
					break;
			}
		}
		else
		{
			std::cerr << "File ignored.Top layer expects just folder representing texturegroups but found a file : " << filename << std::endl;
		}
	}

	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now() - now );

	std::cout << "Time: " << milliseconds.count() << "ms" << std::endl;

	return ret != RETURN_CODE_SUCCESS ? usage( ret ) : ret;
}

// ----------------------------------------
// Unity Build

#pragma warning( push )
#pragma warning( disable : 4505 )

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#pragma warning( pop )