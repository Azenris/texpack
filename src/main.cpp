
// System Includes
#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <chrono>
#include <unordered_map>

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
	RETURN_CODE_NORMAL_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE,
	RETURN_CODE_EMISSIVE_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE,
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

struct Group
{
	std::vector<Image> diffuse;
	std::vector<Image> normal;
	std::vector<Image> emissive;
};

RETURN_CODE image_files( const char *path, std::unordered_map<std::string, u64> &map, Group &group, std::vector<stbrp_rect> &rects, Data *data )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	fs::path entrypath;
	std::string filepath;
	std::string filename;

	for ( const fs::directory_entry &entry : fs::recursive_directory_iterator( path ) )
	{
		entrypath = entry.path();
		filepath = entrypath.string();
		filename = entrypath.stem().string();

		if ( !entry.is_directory() )
		{
			Image *image = nullptr;

			if ( filename.length() > 1 && filename.back() == 'n' && filename[ filename.length() - 2 ] == '_' )
			{
				map[ filename ] = group.normal.size();
				group.normal.emplace_back();
				image = &group.normal.back();
				image->filename = filename;
				image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
				image->width += data->padding * 2;
				image->height += data->padding * 2;
			}
			else if ( filename.length() > 1 && filename.back() == 'e' && filename[ filename.length() - 2 ] == '_' )
			{
				map[ filename ] = group.emissive.size();
				group.emissive.emplace_back();
				image = &group.emissive.back();
				image->filename = filename;
				image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
				image->width += data->padding * 2;
				image->height += data->padding * 2;
			}
			else
			{
				map[ filename ] = group.diffuse.size();

				rects.emplace_back();
				stbrp_rect *rect = &rects.back();

				group.diffuse.emplace_back();
				image = &group.diffuse.back();
				image->filename = filename;
				image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
				image->width += data->padding * 2;
				image->height += data->padding * 2;

				rect->w = image->width;
				rect->h = image->height;
			}

			if ( !image->img )
				return RETURN_CODE_FAILED_TO_OPEN_IMAGE;
		}
	}

	return ret;
}

RETURN_CODE process_texturegroup( const char *path, Data *data )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	std::unordered_map<std::string, u64> map;
	Group group;
	std::vector<stbrp_rect> rects;

	constexpr i32 reserveAmount = 1024;
	group.diffuse.reserve( reserveAmount );
	group.normal.reserve( reserveAmount );
	group.emissive.reserve( reserveAmount );
	rects.reserve( reserveAmount );

	ret = image_files( path, map, group, rects, data );
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

	std::vector<u8> diffuseImage;
	{
		diffuseImage.resize( totalBytes );
		u8 *img = diffuseImage.data();
		for ( u64 i = 0; i < totalBytes; i += 4 )
		{
			*img++ = 255;		// magenta - although if alpha is respected it wont be seen
			*img++ = 0;
			*img++ = 255;
			*img++ = 0;
		}
	}

	std::vector<u8> normalImage;
	{
		normalImage.resize( totalBytes );
		u8 *img = normalImage.data();
		for ( u64 i = 0; i < totalBytes; i += 4 )
		{
			*img++ = 128;
			*img++ = 0;
			*img++ = 255;
			*img++ = 0;
		}
	}

	std::vector<u8> emissiveImage;
	{
		emissiveImage.resize( totalBytes );
		u8 *img = emissiveImage.data();
		memset( img, 0, totalBytes );
	}

	std::string outputName = data->outputName;
	std::string textureName = fs::path( path ).filename().string();

	outputName += PATH_SEP + textureName;

	std::ofstream dataFile( outputName + ".dat", std::ios::binary );
	if ( !dataFile.good() )
	{
		return RETURN_CODE_FAILED_TO_CREATE_DATA_FILE;
	}

	std::string diffuseName = outputName + ".png";
	std::string normalName = outputName + "_n.png";
	std::string emissiveName = outputName + "_e.png";

	TexpackTexture texpackTexture;
	texpackTexture.size = { data->textureWidth, data->textureHeight };
	texpackTexture.numSprites = (u32)rects.size();

	std::vector<TexpackSpriteNamed> texpackSprite;

	f32 tw = (f32)data->textureWidth;
	f32 th = (f32)data->textureHeight;

	for ( u64 i = 0, count = rects.size(); i < count; ++i )
	{
		Image diffuse = group.diffuse[ i ];
		Image normal = {};
		Image emissive = {};

		texpackSprite.emplace_back();
		TexpackSpriteNamed *spr = &texpackSprite.back();

		size_t found = diffuse.filename.rfind( "_" );
		if ( found != std::string::npos )
		{
			spr->sprite.frameCount = std::stoi( &diffuse.filename[ found + 1 ] );
			if ( spr->sprite.frameCount == 0 )
				spr->sprite.frameCount = 1;
			diffuse.filename.erase( found );
		}
		else
		{
			spr->sprite.frameCount = 1;
		}

		if ( auto iter = map.find( diffuse.filename + "_n" ); iter != map.end() )
		{
			normal = group.normal[ iter->second ];
		}

		if ( auto iter = map.find( diffuse.filename + "_e" ); iter != map.end() )
		{
			emissive = group.emissive[ iter->second ];
		}

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
				i32 to = ( x + y * xCount ) * diffuse.channels;

				diffuseImage[ from + 0 ] = diffuse.img[ to + 0 ];
				diffuseImage[ from + 1 ] = diffuse.img[ to + 1 ];
				diffuseImage[ from + 2 ] = diffuse.img[ to + 2 ];
				diffuseImage[ from + 3 ] = diffuse.img[ to + 3 ];

				isTranslucent = isTranslucent || ( diffuse.img[ to + 3 ] != 0 && diffuse.img[ to + 3 ] != 255 );
			}
		}

		if ( normal.img )
		{
			if ( normal.width != diffuse.width || normal.height != diffuse.height )
				return RETURN_CODE_NORMAL_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;

			for ( i32 y = 0; y < yCount; ++y )
			{
				for ( i32 x = 0; x < xCount; ++x )
				{
					i32 from = ( ( offX + x ) + ( offY + y ) * data->textureWidth ) * data->outputChannels;
					i32 to = ( x + y * xCount ) * diffuse.channels;

					normalImage[ from + 0 ] = normal.img[ to + 0 ];
					normalImage[ from + 1 ] = normal.img[ to + 1 ];
					normalImage[ from + 2 ] = normal.img[ to + 2 ];
					normalImage[ from + 3 ] = normal.img[ to + 3 ];

					isTranslucent = isTranslucent || ( normal.img[ to + 3 ] != 0 && normal.img[ to + 3 ] != 255 );
				}
			}
		}

		if ( emissive.img )
		{
			if ( emissive.width != diffuse.width || emissive.height != diffuse.height )
				return RETURN_CODE_EMISSIVE_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;

			for ( i32 y = 0; y < yCount; ++y )
			{
				for ( i32 x = 0; x < xCount; ++x )
				{
					i32 from = ( ( offX + x ) + ( offY + y ) * data->textureWidth ) * data->outputChannels;
					i32 to = ( x + y * xCount ) * emissive.channels;

					emissiveImage[ from + 0 ] = emissive.img[ to + 0 ];
					emissiveImage[ from + 1 ] = emissive.img[ to + 1 ];
					emissiveImage[ from + 2 ] = emissive.img[ to + 2 ];
					emissiveImage[ from + 3 ] = emissive.img[ to + 3 ];

					isTranslucent = isTranslucent || ( emissive.img[ to + 3 ] != 0 && emissive.img[ to + 3 ] != 255 );
				}
			}
		}

		spr->name = diffuse.filename;
		spr->sprite.uvs = { offX / tw, offY / th, ( offX + xCount ) / tw, ( offY + yCount ) / th };
		spr->sprite.size = { xCount, yCount };
		spr->sprite.isTranslucent = isTranslucent;

		stbi_image_free( diffuse.img );
		diffuse.img = nullptr;

		if ( normal.img )
		{
			stbi_image_free( normal.img );
			normal.img = nullptr;
		}

		if ( emissive.img )
		{
			stbi_image_free( emissive.img );
			emissive.img = nullptr;
		}
	}

	std::cout << "Saving texture: " << diffuseName << std::endl;

	stbi_write_png( diffuseName.c_str(), data->textureWidth, data->textureHeight, data->outputChannels, diffuseImage.data(), data->textureWidth * data->outputChannels );
	stbi_write_png( normalName.c_str(), data->textureWidth, data->textureHeight, data->outputChannels, normalImage.data(), data->textureWidth * data->outputChannels );
	stbi_write_png( emissiveName.c_str(), data->textureWidth, data->textureHeight, data->outputChannels, emissiveImage.data(), data->textureWidth * data->outputChannels );

	dataFile.write( textureName.c_str(), textureName.length() );
	dataFile.write( ".png", 4 + 1 ); // +1 to write the null terminator
	dataFile.write( (char*)&texpackTexture, sizeof( texpackTexture ) );

	for ( u64 i = 0, count = texpackSprite.size(); i < count; ++i )
	{
		dataFile.write( texpackSprite[ i ].name.c_str(), texpackSprite[ i ].name.length() + 1 ); // +1 to write the null terminator
		dataFile.write( (char*)&texpackSprite[ i ].sprite, sizeof( TexpackSprite ) );
	}

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
			std::cerr << "File ignored. Top layer expects just folder representing texturegroups but found a file : " << filename << std::endl;
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