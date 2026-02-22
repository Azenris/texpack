# texpack

A simple texture packer using stb_rect_pack.

> [!CAUTION]
> This is mostly being used for my own project and structures may change in newer versions.

### Status
![Build Windows](https://github.com/Azenris/texpack/actions/workflows/build-windows.yml/badge.svg)
![Build Ubuntu](https://github.com/Azenris/texpack/actions/workflows/build-ubuntu.yml/badge.svg)

### License
For license see LICENSE.md
For third party see third_party/THIRD_PARTY_LICENSES.txt
With the executable you can also use the '-l' or '--license' flag.

### Building
Use build scripts `build.sh` or `build.bat` or manually call the cmake (check the build scripts for examples).
The scripts can be called with an argument `debug` or `release` or `ALL` if nothing is passed in `ALL` is automatically used.

### Naming
Filenames should end with how many frames they have. eg. water_4.png
If the texture is for a normal, instead of a frame count (which is assumed to be the same as its base) end with _n. eg. water_n.png
If the texture is for a emissive, instead of a frame count (which is assumed to be the same as its base) end with _e. eg. water_e.png

## Usage
```texpack <input> -o <output-folder> -w 4096 -h 4096 -pad 2

-o / --output     <output-folder>    output folder
-w / --width      4096               width of output textures
-h / --height     4096               height of output textures
-m / --margin     1                  extra space around and not included in the sprite
-p / --pad        2                  extra space around and included in the sprite
-c / --collision                     generate collision box
-t / --time                          output 'Time: <time>' in standard out after completion
-V / --version                       version
-v / --verbose                       verbose logging
-l / --license                       license
```
### Datafile
Datafiles should have the same name as the image file but with a txt extension.
They can override some global options with image specific ones. For example changing the padding.
Example datafile. Case-Sensitive.
```
FC 2
MG 1
PD 0
OR 16 8
COL RECT A
```
```
FC <num>        = FrameCount
MG <num>        = Margin
PD <num>        = Padding
OR <num> <num>  = Origin
NS <num>        = Nineslice Pixel Corner Count
```
```
COL <type> <char> ...  = Collision
  <type> can be either RECT or CIRCLE
  <char> can be A to generate automatic values
  <char> can be F to generate as full image
EG.
COL RECT F             = a rect as the full image size
COL RECT M 1 1 5 5     = a rect with 1, 1, 5, 5 values
COL RECT A             = auto generate around the sprite
COL CIRCLE M 1 1 5     = a circle at position 1, 1 with a radius of 5
COL CIRCLE A           = auto generate a circle, position in centre, radius = max(w, h)
```

## Parse .dat File

A .dat file is produced containing the sprite data.
```
#pragma pack(push, 1)

struct TexpackHeader
{
	u32 magicNumber;
	u16 majorVersion;
	u16 minorVersion;
	u16 revisionVersion;
	u16 reserved;
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
	ivec2 origin;
	i32 frameCount;
	bool isTranslucent;
	u16 nineslice;
	u8 colliderCount;
};

#pragma pack(pop)
```
> [!NOTE]
> vec4 is f32 * 4 ( 16 bytes )
> ivec2 is i32 * 2 ( 8 bytes )

### Read datafile pseudo
- starting at start of file
	- Header:       read struct `TexpackHeader`
	- Texture Name: read text until null terminator
	- Texture:      read struct `TexpackTexture`
	- repeat texture.numSprites times
		- Sprite Name: read text until null terminator
		- Sprite:      read struct `TexpackSprite`
