# texpack

### Status
![Build Windows](https://github.com/Azenris/texpack/actions/workflows/build-windows.yml/badge.svg)
![Build Ubuntu](https://github.com/Azenris/texpack/actions/workflows/build-ubuntu.yml/badge.svg)

### Simple Texture Packer
A simple texture packer currently using stb_rect_pack.

### License
For license see LICENSE.md

For third party see third_party/THIRD_PARTY_LICENSES.txt

### Naming
Filenames cannot currently include underscores except for including emossive and normals which end in _e and _n respectively.

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
```
```
COL <type> <char> ...  = Collision
  COL <type> can be either RECT or CIRCLE
  COL <char> can be A to generate automatic values
EG.
COL RECT M 1 1 5 5     = a rect with 1, 1, 5, 5 values
COL RECT A             = auto generate around the sprite
COL CIRCLE M 1 1 5     = a circle at position 1, 1 with a radius of 5
COL CIRCLE A           = auto generate a circle, position in centre, radius = max(w, h)
```