Copyright (C) 2015 Spotify AB

*****************
** Spotiamb UI **
*****************

Description:
------------
This is the source code to the Spotiamb UI. The actual Spotify protocol code
is proprietary and is bundled as a pre-built .obj file that can be used on
Windows systems.

Here is the web site of the project: http://spotiamb.com

License:
--------
The Spotify pre-built library is provided for non-commercial use only and
by using it, you accept the Spotify Developer Terms of Use.

For the current terms and conditions, please read:

https://developer.spotify.com/developer-terms-of-use/

The Spotiamb UI source code is licensed under the Apache License, version 2.0. 
See LICENSE

Requirements:
-------------
A recent version of Visual Studio
CMake 2.6 or later.
A Spotify Premium account

How to build:
-------------
First you need to acquire an appkey from https://devaccount.spotify.com/my-account/
Put the appkey in the appkey.h file.

Then, do the following to create the Visual Studio project files:
mkdir build
cd build
cmake ..

Then open up the .sln file in Visual Studio and build the 'spotiamb' target.
