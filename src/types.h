
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

#pragma pack(push, 1)

struct vec2
{
	f32 x;
	f32 y;
};

struct vec3
{
	f32 x;
	f32 y;
	f32 z;
};

struct vec4
{
	f32 x;
	f32 y;
	f32 z;
	f32 w;
};

struct ivec2
{
	i32 x;
	i32 y;
};

struct ivec3
{
	i32 x;
	i32 y;
	i32 z;
};

struct ivec4
{
	i32 x;
	i32 y;
	i32 z;
	i32 w;
};

struct TexpackTexture
{
	ivec2 size;
	u32 numSprites;
};

struct TexpackSprite
{
	vec4 uvs;
	ivec2 size;
	i32 frameCount;
	bool isTranslucent;
};

#pragma pack(pop)

struct TexpackSpriteNamed
{
	std::string name;
	TexpackSprite sprite;
};

struct Image
{
	std::string filename;
	stbi_uc *img;
	i32 channels;
	i32 width;
	i32 height;
};

struct Data
{
	std::string outputName;
	i32 outputChannels = 4;
	i32 textureWidth = 0;
	i32 textureHeight = 0;
	i32 padding = 0;
};

static_assert( sizeof( i8 ) == 1 );
static_assert( sizeof( i16 ) == 2 );
static_assert( sizeof( i32 ) == 4 );
static_assert( sizeof( i64 ) == 8 );
static_assert( sizeof( u8 ) == 1 );
static_assert( sizeof( u16 ) == 2 );
static_assert( sizeof( u32 ) == 4 );
static_assert( sizeof( u64 ) == 8 );
static_assert( sizeof( f32 ) == 4 );
static_assert( sizeof( f64 ) == 8 );

static_assert( sizeof( vec2 ) == 8 );
static_assert( sizeof( vec3 ) == 12 );
static_assert( sizeof( vec4 ) == 16 );
static_assert( sizeof( ivec2 ) == 8 );
static_assert( sizeof( ivec3 ) == 12 );
static_assert( sizeof( ivec4 ) == 16 );