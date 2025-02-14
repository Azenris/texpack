
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
#include <climits>

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

enum ToIntResult
{
	Success,
	Failed,
	Overflow,
	Underflow,
};

ToIntResult to_int( i32 *value, char const *str, char **endOut = nullptr, i32 base = 0 );
ToIntResult to_int( u32 *value, char const *str, char **endOut = nullptr, i32 base = 0 );
ToIntResult to_int( i64 *value, char const *str, char **endOut = nullptr, i32 base = 0 );
ToIntResult to_int( u64 *value, char const *str, char **endOut = nullptr, i32 base = 0 );

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
	std::cerr << "\nERROR: " << code << "\n\n";

	std::cerr << "texpack usage:\n"
				 "texpack <folder> -o <output-folder> -w 4096 -h 4096 -pad 2\n"
				 "\n"
				 "-o <output-folder>  output folder \n"
				 "-w 4096             width of output textures \n"
				 "-h 4096             height of output textures \n"
				 "-margin 1           extra space around and not included in the sprite\n"
				 "-pad 2              extra space around and included in the sprite" << std::endl;

	return code;
}

static bool render_image( std::vector<u8> &output, i32 offX, i32 offY, i32 frameW, i32 frameH, u8 *input, i32 frame, i32 inputW, i32 channels, Data *data )
{
	bool isTranslucent = false;

	for ( i32 y = 0; y < frameH; ++y )
	{
		for ( i32 x = 0; x < frameW; ++x )
		{
			i32 to = ( ( offX + x ) + ( offY + y ) * data->textureWidth ) * data->outputChannels;
			i32 from = ( x + frame * frameW + y * inputW ) * channels;

			output[ to + 0 ] = input[ from + 0 ];
			output[ to + 1 ] = input[ from + 1 ];
			output[ to + 2 ] = input[ from + 2 ];
			output[ to + 3 ] = input[ from + 3 ];

			isTranslucent = isTranslucent || ( input[ to + 3 ] != 0 && input[ to + 3 ] != 255 );
		}
	}

	return isTranslucent;
}

struct Group
{
	std::vector<Image> diffuse;
	std::vector<Image> normal;
	std::vector<Image> emissive;
};

struct ImageFilesData
{
	std::unordered_map<std::string, u64> &map;
	Group &group;
	std::vector<stbrp_rect> &rects;
	std::vector<TexpackSpriteNamed> &texpackSprite;
	Data *data;
};

RETURN_CODE image_files( const char *path, ImageFilesData &data )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	fs::path entrypath;
	std::string filepath;
	std::string filename;
	std::ifstream datafile;
	std::string datafileField;
	i32 datafileValue;

	i32 frameCount = 1;
	i32 margin = 1;
	i32 padding = 1;

	for ( const fs::directory_entry &entry : fs::recursive_directory_iterator( path ) )
	{
		if ( entry.is_directory() )
			continue;

		entrypath = entry.path();

		if ( entrypath.extension() == ".txt" )
			continue;

		filepath = entrypath.string();
		filename = entrypath.stem().string();
		datafile.open( entrypath.replace_extension( "txt" ), std::ios::binary );

		frameCount = -1;
		margin = data.data->margin;
		padding = data.data->padding;

		if ( datafile.is_open() )
		{
			while ( !datafile.eof() && datafile.good() )
			{
				datafile >> datafileField;
				datafile >> datafileValue;

				if ( datafileField == "FC" )
					frameCount = datafileValue;
				else if ( datafileField == "MG" )
					margin = datafileValue;
				else if ( datafileField == "PD" )
					padding = datafileValue;
			}
		}

		datafile.close();

		Image *image = nullptr;

		if ( filename.length() > 1 && filename.back() == 'n' && filename[ filename.length() - 2 ] == '_' )
		{
			data.map[ filename ] = data.group.normal.size();
			data.group.normal.emplace_back();
			image = &data.group.normal.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
		}
		else if ( filename.length() > 1 && filename.back() == 'e' && filename[ filename.length() - 2 ] == '_' )
		{
			data.map[ filename ] = data.group.emissive.size();
			data.group.emissive.emplace_back();
			image = &data.group.emissive.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
		}
		else
		{
			data.map[ filename ] = data.group.diffuse.size();

			data.rects.emplace_back();
			stbrp_rect *rect = &data.rects.back();

			data.group.diffuse.emplace_back();
			image = &data.group.diffuse.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );

			data.texpackSprite.emplace_back();
			TexpackSpriteNamed *spr = &data.texpackSprite.back();

			if ( frameCount == -1 )
			{
				frameCount = 1;

				size_t found = image->filename.rfind( "_" );

				if ( found != std::string::npos )
				{
					if ( to_int( &frameCount, &image->filename[ found + 1 ], nullptr, 10 ) == ToIntResult::Success )
					{
						image->filename.erase( found );
					}
				}

				if ( frameCount == 0 )
					frameCount = 1;
			}

			spr->sprite.frameCount = frameCount;

			image->margin = margin;
			image->padding = padding;
			image->width += margin * 2 + padding * 2 * frameCount;
			image->height += ( margin + padding ) * 2;

			rect->w = image->width;
			rect->h = image->height;
		}

		if ( !image->img )
		{
			std::cerr << "Failed to open image: " << filepath << std::endl;
			return RETURN_CODE_FAILED_TO_OPEN_IMAGE;
		}
	}

	return ret;
}

RETURN_CODE process_texturegroup( const char *path, Data *data )
{
	std::cout << "Processing: " << path << std::endl;

	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	std::unordered_map<std::string, u64> map;
	Group group;
	std::vector<stbrp_rect> rects;
	std::vector<TexpackSpriteNamed> texpackSprite;

	ImageFilesData imgData =
	{
		.map = map,
		.group = group,
		.rects = rects,
		.texpackSprite = texpackSprite,
		.data = data,
	};

	constexpr i32 reserveAmount = 1024;
	group.diffuse.reserve( reserveAmount );
	group.normal.reserve( reserveAmount );
	group.emissive.reserve( reserveAmount );
	rects.reserve( reserveAmount );

	ret = image_files( path, imgData );
	if ( ret != RETURN_CODE_SUCCESS )
		return ret;

	stbrp_context context;

	std::vector<stbrp_node> nodes;
	nodes.resize( data->textureWidth );

	stbrp_init_target( &context, data->textureWidth, data->textureHeight, nodes.data(), (i32)nodes.size() );

	if ( stbrp_pack_rects( &context, rects.data(), (i32)rects.size() ) == 0 )
	{
		// TODO : in future could possible make another texture for the overflowed ones
		std::cerr << "Failed to pack all images. (" << path << ")" << std::endl;
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
			*img++ = 128;
			*img++ = 255;
			*img++ = 255;
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
		std::cerr << "Failed to create data file: " << outputName + ".dat" << std::endl;
		return RETURN_CODE_FAILED_TO_CREATE_DATA_FILE;
	}

	std::string diffuseName = outputName + ".png";
	std::string normalName = outputName + "_n.png";
	std::string emissiveName = outputName + "_e.png";

	TexpackTexture texpackTexture;
	texpackTexture.size = { data->textureWidth, data->textureHeight };
	texpackTexture.numSprites = (u32)rects.size();

	f32 tw = (f32)data->textureWidth;
	f32 th = (f32)data->textureHeight;

	for ( u64 i = 0, count = rects.size(); i < count; ++i )
	{
		Image diffuse = group.diffuse[ i ];
		Image normal = {};
		Image emissive = {};

		TexpackSpriteNamed *spr = &texpackSprite[ i ];

		if ( auto iter = map.find( diffuse.filename + "_n" ); iter != map.end() )
		{
			normal = group.normal[ iter->second ];
		}

		if ( auto iter = map.find( diffuse.filename + "_e" ); iter != map.end() )
		{
			emissive = group.emissive[ iter->second ];
		}

		stbrp_rect rect = rects[ i ];

		i32 margin = diffuse.margin;
		i32 padding = diffuse.padding;

		i32 offX = rect.x + margin + padding;
		i32 offY = rect.y + margin + padding;
		i32 inputTextureW = ( rect.w - ( margin * 2 + padding * 2 * spr->sprite.frameCount ) );
		i32 frameW = inputTextureW / spr->sprite.frameCount;
		i32 frameH = rect.h - ( margin + padding ) * 2;
		bool isTranslucent = false;

		for ( i32 frame = 0; frame < spr->sprite.frameCount; ++frame )
		{
			i32 frameOffX = offX + frame * ( frameW + padding * 2 );
			i32 frameOffY = offY;

			isTranslucent = render_image( diffuseImage, frameOffX, frameOffY, frameW, frameH, diffuse.img, frame, inputTextureW, diffuse.channels, data ) || isTranslucent;

			if ( normal.img )
			{
				if ( ( normal.width / spr->sprite.frameCount ) != frameW || normal.height != frameH )
				{
					std::cerr << "Normal texture should be same size as diffuse texture." << std::endl;
					return RETURN_CODE_NORMAL_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;
				}

				isTranslucent = render_image( normalImage, frameOffX, frameOffY, frameW, frameH, normal.img, frame, inputTextureW, normal.channels, data ) || isTranslucent;
			}

			if ( emissive.img )
			{
				if ( ( emissive.width / spr->sprite.frameCount ) != frameW || emissive.height != frameH )
				{
					std::cerr << "Emissive texture should be same size as diffuse texture." << std::endl;
					return RETURN_CODE_EMISSIVE_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;
				}

				isTranslucent = render_image( emissiveImage, frameOffX, frameOffY, frameW, frameH, emissive.img, frame, inputTextureW, emissive.channels, data ) || isTranslucent;
			}
		}

		spr->name = diffuse.filename;

		offX = rect.x + margin;
		offY = rect.y + margin;
		frameW = frameW + padding * 2;
		frameH = frameH + padding * 2;

		spr->sprite.uvs = { offX / tw, offY / th, ( offX + frameW ) / tw, ( offY + frameH ) / th };
		spr->sprite.size = { frameW, frameH };
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
		std::cerr << "Invalid arguments" << std::endl;
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
		else if ( strcmp( argv[ i ], "-margin" ) == 0 )
		{
			if ( i == argc - 1 )
				return usage( RETURN_CODE_INVALID_ARGUMENTS );
			data.margin = atoi( argv[ i + 1 ] );

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
	{
		std::cerr << "Width and height should be > 0" << std::endl;
		return usage( RETURN_CODE_INVALID_ARGUMENTS );
	}

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

ToIntResult to_int( i32 *value, char const *str, char **endOut, i32 base )
{
	i64 v;
	ToIntResult result = to_int( &v, str, endOut, base );
	if ( result != ToIntResult::Success )
		return result;
	if ( v > INT32_MAX )
		return ToIntResult::Overflow;
	if ( v < INT32_MIN )
		return ToIntResult::Underflow;
	*value = static_cast<i32>( v );
	return ToIntResult::Success;
}

ToIntResult to_int( u32 *value, char const *str, char **endOut, i32 base )
{
	u64 v;
	ToIntResult result = to_int( &v, str, endOut, base );
	if ( result != ToIntResult::Success )
		return result;
	if ( v > UINT32_MAX )
		return ToIntResult::Overflow;
	if ( v < 0 )
		return ToIntResult::Underflow;
	*value = static_cast<u32>( v );
	return ToIntResult::Success;
}

ToIntResult to_int( i64 *value, char const *str, char **endOut, i32 base )
{
	char *end;
	errno = 0;
	i64 l = strtol( str, &end, base );
	if ( endOut )
		*endOut = end;
	if ( ( errno == ERANGE && l == LONG_MAX ) || l > INT_MAX )
		return ToIntResult::Overflow;
	if ( ( errno == ERANGE && l == LONG_MIN ) || l < INT_MIN )
		return ToIntResult::Underflow;
	if ( *str == '\0' )
		return ToIntResult::Failed;
	*value = l;
	return ToIntResult::Success;
}

ToIntResult to_int( u64 *value, char const *str, char **endOut, i32 base )
{
	char *end;
	errno = 0;
	u64 l = strtoul( str, &end, base );
	if ( endOut )
		*endOut = end;
	if ( ( errno == ERANGE && l == LONG_MAX ) || l > INT_MAX )
		return ToIntResult::Overflow;
	if ( ( errno == ERANGE && l == LONG_MIN ) || l < INT_MIN )
		return ToIntResult::Underflow;
	if ( *str == '\0' )
		return ToIntResult::Failed;
	*value = l;
	return ToIntResult::Success;
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