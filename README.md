# texpack

### Status
![Build Windows](https://github.com/Azenris/texpack/actions/workflows/build-windows.yml/badge.svg)
![Build Ubuntu](https://github.com/Azenris/texpack/actions/workflows/build-ubuntu.yml/badge.svg)

### Simple Texture Packer
A simple texture packer currently using stb_rect_pack.

### License
For license see LICENSE.md

For third party see third_party/THIRD_PARTY_LICENSES.txt

### Datafile
Datafiles should have the same name as the image file but with a txt extension.
They can override some global options with image specific ones. For example changing the padding.
Example datafile. Case-Sensitive.
```
FC 2
MG 1
PD 0
```
```
FC = FrameCount
MG = Margin
PD = Padding
```