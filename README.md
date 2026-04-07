# Media Tag System

## Usage
- Copy config_default.yaml to config.yaml, change anything if needed
- Bookmarks are stored in config.yaml, the program does not write and added bookmarks or config changes, you need to add it yourself for now
- Enter toggles between gallery view and media/fullscreen view when selected on an image or video
- Files or folders can be dragged in to enter them or look at the file

## Build Requirements

- CMake
- Python 3
- py7zr Python Module

### Windows
- Visual Studio 2022+ For Windows

### Linux
- libmpv
- SDL3
- freetype

## Building
- Enter the `thirdparty` folder, and run the `thirdparty.bat` script to download and compile thirdparty stuff
- On Linux, you may need to run `linux_prepare_python.sh`, which makes a python virtual environment, and installs py7zr to it, then run `thirdparty.sh`
- If all succeeded, return to the root folder
- Make a folder called `build` and enter it
- Enter that folder and in cmd or the terminal, run `cmake ..`
- On Windows, open media_tag_system.sln and build the project
- On Linux, run `cmake --build`, or whatever else you want to build it with, like `make -j8` or ninja
- The program will be in the `out` folder, ready to run
