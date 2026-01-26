@echo OFF
cls

cmake -S . -B build/static-mt ^
	-DBUILD_CRT_STATIC=ON

cmake --build build/static-mt --config=Debug