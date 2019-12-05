[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/V7V0VP6A)

Imgpack
=======

Simple texture atlas generator. Takes folder with images and outputs texture atlas + data file. Written in pure C without external dependencies. For packing uses `stb_rect_pack` which implements `Skyline` bin packing algorithm. For crossplatform filesystem manipulation uses `cute_files`.

Quick start
-----------

To build you will need `C99` compiler and optionally `Make`. All dependencies are packed in the repo, so just clone the repo and do:

```
make all
```

This will create `imgpack` command-line tool which can be used like:

```
./imgpack <OPTIONS> <folder with images>
```

where options are:

| Key          |    | Value   | Description
|--------------|----|---------|----------------------------------------------
| --data       | -d | string  | output file path, if ommited `stdout` is used
| --image      | -i | string  | output image path (NEEDED)
| --name       | -n | string  | name
| --format     | -f | string  | output atlas data format
| --trim       | -t | int     | alpha threshold for trimming image with transparent border, should be 0-255
| --padding    | -p | int     | adds transparent padding
| --exturde    | -e | int     | adds copied pixels on image borders, which helps with texture bleeding
| --max-width  | -w | int     | maximum atlas width
| --max-height | -h | int     | maximum atlas height
| --scale      | -s | int/int | scaling ratio int form "A/B" or just "K"
| --unique     | -u |         | remove identical images (after trimming)
| --force-pot  | -2 |         | force power of two texture output
| --sort       | -s |         | sorting by path name (ascending)
| --verbose    | -v |         | print debug messages during the packing process
| --help       | -? |         | prints this memo

Output atlas formats
--------------------

### RAYLIB

Outputs plain C enums and static arrays of `raylib` structs. Ids of the images is generated as uppercased folder name + basename of the images without extension. For instance if you have `images` folder with `images/testing.png` image you will get:
```
enum IMAGES_Ids {
	IMAGES_TESTING = 0,
};

#define IMAGES_PATH "output/images.png"

static const Vector2 IMAGES_Scale[2] = {1, 1};

static const Vector2 IMAGES_Frame[1][4] = {
	{2, 2, 42, 42}, // which part of the atlas to draw
};

static const Vector2 IMAGES_Size[1][2] = {
i	{80, 80}, // source size of the image
};
...
```

### JSON\_ARRAY

Outputs JSON formatted like TexturePacker does

### JSON\_HASH

Outputs JSON formatted like TexturePacker does


Todos
-----

* Multipacking
* Not square output texture
* Compressed output texture (DXT5?)
* Memory allocation checks
* Input error checking

Donation
--------

ImgPack is free and open source project and if you like the project consider a [donation](https://ko-fi.com/V7V0VP6A).
