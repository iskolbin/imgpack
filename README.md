[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/V7V0VP6A)

Imgpack
=======

Simple texture packer generator. Takes folder with images and outputs texture atlas + data file. Written in pure C without external dependencies.

Quick start
-----------

To build you will need `C99` compiler and optionally `Make`. All dependencies are packed in the repo, so just clone or download zip the repo and do:

```
make all
```

This will create `imgpack` command-line tool which can be used like:

```
imgpack <OPTIONS> <folder with images>
```

where options are:

| Key        | Value  | Description
|------------|--------|------------
| --data     | string | output file path, if ommited `stdout` is used
| --image    | string | (required) output image path
| --trim     |        | if specified trims input images with transparent borders
| --padding  | int    | adds transparent padding
| --exturde  | int    | adds copied pixels on image borders, which helps whit texture bleeding
| --format   | string | output atlas data format
| --verbose  |        | print debug messages during the packing process


Output atlas formats
--------------------

### C

Outputs plain C enums and static int arrays. Ids of the images is generated as uppercased folder name + basename of the images without extension. For instance if you have `images` folder with `images/testing.png` image you will get:

```
enum IMAGES_Ids {
	IMAGES_TESTING = 0,
};

static const char *IMAGES_Image = "images.png"; // --image option

static const int IMAGES_Scale[2] = {1, 1}; // NIY

static const int IMAGES_Frame[1][4] = {
	{2, 2, 42, 42}, // which part of the atlas to draw
};

static const int IMAGES_Size[1][2] = {
	{80, 80}, // source size of the image
};
```

### RAYLIB

Same as `C` except but using `raylib` structs instead of plain int arrays:

```
static const Vector2 IMAGES_Scale[2] = {1, 1}; // NIY

static const Vector2 IMAGES_Frame[1][4] = {
	{2, 2, 42, 42}, // which part of the atlas to draw
};

static const Vector2 IMAGES_Size[1][2] = {
	{80, 80}, // source size of the image
};
```

### JSON\_ARRAY

Outputs JSON formatted like TexturePacker does

### JSON\_HASH

Outputs JSON formatted like TexturePacker does


Todos
-----

* Choosing threshold for trim
* Multipacking
* Limits for max width and max height for output texture
* Not POT output texture
* Not square output texture
* Scaling
* Compressed output texture (DXT5?)
* Detecting same images
* Memory allocation checks
* Input error checking

Donation
--------

ImgPack is free and open source project and if you like the project consider a [donation](https://ko-fi.com/V7V0VP6A).
