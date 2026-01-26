
// System Includes
#define __STDC_LIMIT_MACROS
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
#include <charconv>

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

struct Options
{
	bool verbose;
	GenCollisionData generateCollisionData;
};

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
};

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
				 "-pad 2              extra space around and included in the sprite\n"
				 "-collision          generate collision box\n"
				 "-verbose            verbose logging" << std::endl;

	return code;
}

bool to_int( const std::string &str, i32 *result )
{
	auto [ ptr, ec ] = std::from_chars( str.data(), str.data() + str.size(), *result );
	return ec == std::errc{} && ptr == str.data() + str.size();
}

#define min_value( l, r )	( ( l ) < ( r ) ? ( l ) : ( r ) )
#define max_value( l, r )	( ( l ) > ( r ) ? ( l ) : ( r ) )

static i32 image_rect_area_left( Image *image, i32 left, i32 imgWidth, i32 frameCount )
{
	i32 frameW = image->frameW;
	i32 frameH = image->frameH;
	i32 channels = image->channels;
	u8 *input = image->img;

	for ( i32 x = 0; x < frameW; ++x )
	{
		for ( i32 y = 0; y < frameH; ++y )
		{
			for ( i32 frame = 0; frame < frameCount; ++frame )
			{
				i32 from = ( x + frame * frameW + y * imgWidth ) * channels;
				if ( input[ from + 3 ] != 0 )
					return max_value( left, x );
			}
		}
	}
	return left;
}

static i32 image_rect_area_top( Image *image, i32 top, i32 imgWidth, i32 frameCount )
{
	i32 frameW = image->frameW;
	i32 frameH = image->frameH;
	i32 channels = image->channels;
	u8 *input = image->img;

	for ( i32 y = 0; y < frameH; ++y )
	{
		for ( i32 x = 0; x < frameW; ++x )
		{
			for ( i32 frame = 0; frame < frameCount; ++frame )
			{
				i32 from = ( x + frame * frameW + y * imgWidth ) * channels;
				if ( input[ from + 3 ] != 0 )
					return max_value( top, y );
			}
		}
	}
	return top;
}

static i32 image_rect_area_right( Image *image, i32 right, i32 imgWidth, i32 frameCount )
{
	i32 frameW = image->frameW;
	i32 frameH = image->frameH;
	i32 channels = image->channels;
	u8 *input = image->img;

	for ( i32 x = frameW - 1; x >= 0; --x )
	{
		for ( i32 y = 0; y < frameH; ++y )
		{
			for ( i32 frame = 0; frame < frameCount; ++frame )
			{
				i32 from = ( x + frame * frameW + y * imgWidth ) * channels;
				if ( input[ from + 3 ] != 0 )
					return min_value( right, x );
			}
		}
	}
	return right;
}

static i32 image_rect_area_bottom( Image *image, i32 bot, i32 imgWidth, i32 frameCount )
{
	i32 frameW = image->frameW;
	i32 frameH = image->frameH;
	i32 channels = image->channels;
	u8 *input = image->img;

	for ( i32 y = frameH - 1; y >= 0; --y )
	{
		for ( i32 x = 0; x < frameW; ++x )
		{
			for ( i32 frame = 0; frame < frameCount; ++frame )
			{
				i32 from = ( x + frame * frameW + y * imgWidth ) * channels;
				if ( input[ from + 3 ] != 0 )
					return min_value( bot, y );
			}
		}
	}
	return bot;
}

static ivec4 image_rect_area( Image *image, TexpackSpriteNamed *sprite, i32 imgWidth, Data *data )
{
	ivec4 area = { INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX };
	i32 frameCount = sprite->sprite.frameCount;

	area.x = image_rect_area_left( image, area.x, imgWidth , frameCount);
	area.y = image_rect_area_top( image, area.y, imgWidth, frameCount );
	area.z = image_rect_area_right( image, area.z, imgWidth, frameCount );
	area.w = image_rect_area_bottom( image, area.w, imgWidth, frameCount );

	area.x += image->padding;
	area.y += image->padding;
	area.z += image->padding;
	area.w += image->padding;

	return area;
}

static bool render_image( std::vector<u8> &output, i32 offX, i32 offY, i32 frameW, i32 frameH, u8 *input, i32 imgSize, i32 frame, i32 inputW, i32 channels, Data *data )
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

			isTranslucent = isTranslucent || ( output[ to + 3 ] != 0 && output[ to + 3 ] != 255 );
		}
	}

	return isTranslucent;
}

RETURN_CODE image_files( const char *path, Options *options, Data *data, ImageFilesData *fileData )
{
	RETURN_CODE ret = RETURN_CODE_SUCCESS;

	fs::path entrypath;
	std::string filepath;
	std::string filename;
	std::string datafilename;
	std::ifstream datafile;
	std::string datafileField;
	i32 datafileValue;
	i32 frameCount;
	i32 margin;
	i32 padding;
	i32 originX;
	i32 originY;
	i32 imgWidth;
	i32 imgHeight;
	u16 nineslice;
	bool manualCol;
	u32 collisionCount;
	GenCollisionData genColData[ MAX_SPRITE_COLLIDERS ];

	for ( const fs::directory_entry &entry : fs::recursive_directory_iterator( path ) )
	{
		if ( entry.is_directory() )
			continue;

		entrypath = entry.path();

		if ( entrypath.extension() == ".txt" )
			continue;

		filepath = entrypath.string();
		filename = entrypath.stem().string();

		if ( options->verbose )
			std::cout << "Processing file: " << filepath << std::endl;

		frameCount = -1;
		margin = data->margin;
		padding = data->padding;
		originX = INT32_MAX;
		originY = INT32_MAX;
		nineslice = 0;
		collisionCount = options->generateCollisionData.enable ? 1 : 0;
		genColData[ 0 ] = options->generateCollisionData;
		manualCol = false;

		Image *image = nullptr;

		if ( filename.length() > 1 && filename.back() == 'n' && filename[ filename.length() - 2 ] == '_' )
		{
			fileData->map[ filename ] = fileData->group.normal.size();
			fileData->group.normal.emplace_back();
			image = &fileData->group.normal.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
			image->imgSize = image->width * image->height * image->channels;
		}
		else if ( filename.length() > 1 && filename.back() == 'e' && filename[ filename.length() - 2 ] == '_' )
		{
			fileData->map[ filename ] = fileData->group.emissive.size();
			fileData->group.emissive.emplace_back();
			image = &fileData->group.emissive.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
			image->imgSize = image->width * image->height * image->channels;
		}
		else
		{
			if ( frameCount == -1 )
			{
				frameCount = 1;

				size_t found = filename.rfind( "_" );

				if ( found != std::string::npos )
				{
					if ( to_int( &filename[ found + 1 ], &frameCount ) )
					{
						filename.erase( found );
					}
				}
			}

			datafilename = ( entrypath.parent_path() / filename ).replace_extension( "txt" ).string();
			datafile.open( datafilename, std::ios::binary );

			if ( datafile.is_open() )
			{
				if ( options->verbose )
					std::cout << "Reading datafile: " << datafilename << std::endl;

				while ( !datafile.eof() && datafile.good() )
				{
					datafile >> datafileField;

					if ( datafileField == "FC" )
					{
						datafile >> datafileValue;
						frameCount = datafileValue;
					}
					else if ( datafileField == "MG" )
					{
						datafile >> datafileValue;
						margin = datafileValue;
					}
					else if ( datafileField == "PD" )
					{
						datafile >> datafileValue;
						padding = datafileValue;
					}
					else if ( datafileField == "OR" )
					{
						datafile >> datafileValue;
						originX = datafileValue;
						datafile >> datafileValue;
						originY = datafileValue;
					}
					else if ( datafileField == "NS" )
					{
						datafile >> datafileValue;
						if ( datafileValue < 0 || datafileValue > 65535 )
						{
							std::cerr << "Nineslice value out of bounds: " << datafileValue << std::endl;
							datafileValue = 0;
						}
						nineslice = (u16)datafileValue;
					}
					else if ( datafileField == "COL" )
					{
						// first collision is overwritten if their was a global one
						if ( !manualCol && options->generateCollisionData.enable && collisionCount == 1 )
						{
							manualCol = true;
							collisionCount -= 1;
						}
						GenCollisionData *colData = &genColData[ collisionCount++ ];
						colData->enable = true;
						datafile >> datafileField;
						if ( datafileField == "RECT" )
						{
							datafile >> datafileField;
							if ( datafileField == "A" ) // Auto
							{
								colData->type = GEN_COLLISION_DATA_TYPE_RECT_AUTO;
							}
							else if ( datafileField == "F" ) // Full
							{
								colData->type = GEN_COLLISION_DATA_TYPE_RECT_FULL;
							}
							else if ( datafileField == "M" ) // Manual
							{
								colData->type = GEN_COLLISION_DATA_TYPE_RECT_MANUAL;
								datafile >> datafileValue;
								colData->area.x = datafileValue;
								datafile >> datafileValue;
								colData->area.y = datafileValue;
								datafile >> datafileValue;
								colData->area.z = datafileValue;
								datafile >> datafileValue;
								colData->area.w = datafileValue;
							}
							else
							{
								if ( options->verbose )
									std::cerr << "Unknown data file field COL RECT: " << datafileField << std::endl;
							}
						}
						else if ( datafileField == "CIRCLE" )
						{
							datafile >> datafileField;
							if ( datafileField == "A" ) // Auto
							{
								colData->type = GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO;
							}
							else if ( datafileField == "AE" ) // Auto-Emcompass
							{
								colData->type = GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO_ENCOMPASS;
							}
							else if ( datafileField == "M" ) // Manual
							{
								colData->type = GEN_COLLISION_DATA_TYPE_CIRCLE_MANUAL;
								datafile >> datafileValue;
								colData->position.x = datafileValue;
								datafile >> datafileValue;
								colData->position.y = datafileValue;
								datafile >> datafileValue;
								colData->radius = datafileValue;
							}
							else
							{
								if ( options->verbose )
									std::cerr << "Unknown data file field COL CIRCLE: " << datafileField << std::endl;
							}
						}
						else
						{
							if ( options->verbose )
								std::cerr << "Unknown data file field for COL: " << datafileField << std::endl;
						}
					}
					else
					{
						if ( options->verbose )
							std::cerr << "Unknown data file field: " << datafileField << std::endl;
					}
				}
			}

			datafile.close();

			fileData->map[ filename ] = fileData->group.diffuse.size();

			fileData->rects.emplace_back();
			stbrp_rect *rect = &fileData->rects.back();

			fileData->group.diffuse.emplace_back();
			image = &fileData->group.diffuse.back();
			image->filename = filename;
			image->img = stbi_load( filepath.c_str(), &image->width, &image->height, &image->channels, 4 );
			image->imgSize = image->width * image->height * image->channels;

			fileData->texpackSprite.emplace_back();
			TexpackSpriteNamed *spr = &fileData->texpackSprite.back();

			if ( frameCount <= 0 )
				frameCount = 1;

			if ( originX == INT32_MAX )
				originX = ( image->width / frameCount ) / 2;

			if ( originY == INT32_MAX )
				originY = image->height / 2;

			spr->sprite.frameCount = frameCount;
			spr->sprite.origin = { originX + padding, originY + padding };
			spr->sprite.nineslice = nineslice;
			spr->sprite.colliderCount = (u8)collisionCount;

			imgWidth = image->width;
			imgHeight = image->height;

			image->margin = margin;
			image->padding = padding;
			image->width += margin * 2 + padding * 2 * frameCount;
			image->height += ( margin + padding ) * 2;
			image->frameW = imgWidth / spr->sprite.frameCount;
			image->frameH = imgHeight;
			image->colliderCount = collisionCount;

			rect->w = image->width;
			rect->h = image->height;

			// Collision
			for ( u32 colIdx = 0; colIdx < collisionCount; ++colIdx )
			{
				GenCollisionData *colData = &genColData[ colIdx ];

				if ( colData->enable )
				{
					switch ( colData->type )
					{
					case GEN_COLLISION_DATA_TYPE_RECT_AUTO:
						colData->area = image_rect_area( image, spr, imgWidth, data );
						break;

					case GEN_COLLISION_DATA_TYPE_RECT_FULL:
						colData->area = { padding, padding, image->frameW, image->frameH };
						break;

					case GEN_COLLISION_DATA_TYPE_RECT_MANUAL:
						break;

					case GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO:
						colData->area = image_rect_area( image, spr, imgWidth, data );
						colData->position = { ( image->frameW + padding * 2 ) / 2, ( image->frameH + padding * 2 ) / 2 };
						colData->radius = max_value( ( colData->area.z - colData->area.x ), ( colData->area.w - colData->area.y ) );
						break;

					case GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO_ENCOMPASS:
						colData->area = image_rect_area( image, spr, imgWidth, data );
						colData->position = { ( image->frameW + padding * 2 ) / 2, ( image->frameH + padding * 2 ) / 2 };
						// TODO : radius equal to diagonal from centre
						break;

					case GEN_COLLISION_DATA_TYPE_CIRCLE_MANUAL:
						break;
					}

					image->genColData[ colIdx ] = genColData[ colIdx ];
				}
			}
		}

		if ( !image->img )
		{
			std::cerr << "Failed to open image: " << filepath << std::endl;
			return RETURN_CODE_FAILED_TO_OPEN_IMAGE;
		}
	}

	return ret;
}

RETURN_CODE process_texturegroup( const char *path, Options *options, Data *data )
{
	if ( options->verbose )
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
	};

	constexpr i32 reserveAmount = 1024;
	group.diffuse.reserve( reserveAmount );
	group.normal.reserve( reserveAmount );
	group.emissive.reserve( reserveAmount );
	rects.reserve( reserveAmount );

	ret = image_files( path, options, data, &imgData );
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

	if ( options->verbose )
		std::cout << "Creating blank images." << path << std::endl;

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

	outputName += "/" + textureName;

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

			if ( options->verbose )
				std::cout << "Rendering diffuse image for " << diffuse.filename << "(frame: " << frame << ")" << std::endl;

			isTranslucent = render_image( diffuseImage, frameOffX, frameOffY, frameW, frameH, diffuse.img, diffuse.imgSize, frame, inputTextureW, diffuse.channels, data ) || isTranslucent;

			if ( normal.img )
			{
				if ( ( normal.width / spr->sprite.frameCount ) != frameW || normal.height != frameH )
				{
					std::cerr << "Normal texture should be same size as diffuse texture." << std::endl;
					return RETURN_CODE_NORMAL_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;
				}

				if ( options->verbose )
					std::cout << "Rendering normal image for " << diffuse.filename << "(frame: " << frame << ")" << std::endl;

				isTranslucent = render_image( normalImage, frameOffX, frameOffY, frameW, frameH, normal.img, normal.imgSize, frame, inputTextureW, normal.channels, data ) || isTranslucent;
			}

			if ( emissive.img )
			{
				if ( ( emissive.width / spr->sprite.frameCount ) != frameW || emissive.height != frameH )
				{
					std::cerr << "Emissive texture should be same size as diffuse texture." << std::endl;
					return RETURN_CODE_EMISSIVE_TEXTURE_NOT_SAME_SIZE_AS_DIFFUSE;
				}

				if ( options->verbose )
					std::cout << "Rendering emissive image for " << diffuse.filename << "(frame: " << frame << ")" << std::endl;

				isTranslucent = render_image( emissiveImage, frameOffX, frameOffY, frameW, frameH, emissive.img, emissive.imgSize, frame, inputTextureW, emissive.channels, data ) || isTranslucent;
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

		if ( texpackSprite[ i ].sprite.colliderCount > 0 )
		{
			const Image *diffuse = &group.diffuse[ i ];

			for ( i32 colIdx = 0, colCount = diffuse->colliderCount; colIdx < colCount; ++colIdx )
			{
				const GenCollisionData *col = &diffuse->genColData[ colIdx ];

				switch ( col->type )
				{
				case GEN_COLLISION_DATA_TYPE_RECT_AUTO:
				case GEN_COLLISION_DATA_TYPE_RECT_FULL:
				case GEN_COLLISION_DATA_TYPE_RECT_MANUAL:
					{
						u8 colliderType = COLLIDER_TYPE_RECT;
						dataFile.write( (char*)&colliderType, sizeof( colliderType ) );
						dataFile.write( (char*)&col->area, sizeof( col->area ) );
					}
					break;

				case GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO:
				case GEN_COLLISION_DATA_TYPE_CIRCLE_AUTO_ENCOMPASS:
				case GEN_COLLISION_DATA_TYPE_CIRCLE_MANUAL:
					{
						u8 colliderType = COLLIDER_TYPE_CIRCLE;
						dataFile.write( (char*)&colliderType, sizeof( colliderType ) );
						dataFile.write( (char*)&col->position, sizeof( col->position ) );
						dataFile.write( (char*)&col->radius, sizeof( col->radius ) );
					}
					break;
				}
			}
		}
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

	Options options =
	{
		.verbose = false,
		.generateCollisionData =
		{
			.enable = false,
			.type = GEN_COLLISION_DATA_TYPE_RECT_MANUAL
		},
	};

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
		else if ( strcmp( argv[ i ], "-collision" ) == 0 )
		{
			options.generateCollisionData.enable = true;
			i += 1;
		}
		else if ( strcmp( argv[ i ], "-verbose" ) == 0 )
		{
			options.verbose = true;
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
				ret = process_texturegroup( entry.path().string().c_str(), &options, &data );

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