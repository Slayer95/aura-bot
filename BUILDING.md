Building
--------

Get the source code from the project [repository][1] at Gitlab.

### Windows

Windows users must use VS2019 or later. Visual Studio 2019 Community edition works.
Neccessary .sln and .vcxproj files are provided. Before building, choose the Release configuration and Win32 or x64 as the platform.
The binary shall be generated in the `..\aura-bot\aura\Release` folder.

Note: When installing Visual Studio select in the `Desktop development with C++` category the `Windows 8.1 SDK` or `Windows 10 SDK` 
(depending on your OS version), and, if running with VS2019 or newer, also the `MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.29)`.

### Linux

Linux users will probably need some packages for it to build:

* Debian/Ubuntu -- `apt-get install git build-essential m4 libgmp3-dev cmake libbz2-dev zlib1g-dev`
* Arch Linux -- `pacman -S base-devel cmake`

#### Steps

For building StormLib execute the following commands (line by line):

	cd aura-bot/StormLib
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DYNAMIC_MODULE=1 ..
	make
	sudo make install

Continue building bncsutil:

	cd ../..
	cd bncsutil/src/bncsutil
	make
	sudo make install

Build miniupnpc

	cd ../..
	cd miniupnpc
	make
	sudo make install

Build cpr

  TODO: Fill me in

Then proceed to build Aura:

	cd ../../..
	make

Now you can run Aura by executing `./aura` or install it to your path using `sudo make install`.

**Note**: gcc version needs to be 8 or higher along with a compatible libc.

**Note**: clang needs to be 5 or higher along with ld gold linker (ie. package binutils-gold for ubuntu),
clang lower than 16 requires -std=c++17

**Note**: StormLib installs itself in `/usr/local/lib` which isn't in PATH by default
on some distros such as Arch or CentOS.

### OS X

#### Requirements

* OSX ≥10.15 (Catalina) or higher.
* Latest available Xcode for your platform and/or the Xcode Command Line Tools.
One of these might suffice, if not just get both.
* A recent `libgmp`.

You can use [Homebrew](http://brew.sh/) to get `libgmp`. When you are at it, you can also use it to install StormLib instead of compiling it on your own:

	brew install gmp
	brew install stormlib   # optional

Now proceed by following the [steps for Linux users](#steps) and omit StormLib in case you installed it using `brew`.

[1]: https://gitlab.com/ivojulca/aura-bot
