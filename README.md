# WallpaperChanger
This is a fairly simple wallpaper manager that will probably only work on Linux. The code is still messy and the overall design is questionable. I wanted to rewrite my Bash script in C++, then I decided to port that to C. It was mostly for practice but I do use this regularly. I know there are a few areas in which the code can be improved and I'm sure there are some bugs I haven't caught yet. Please file any issues and I'll fix what I can.

## How it works:

This program supports simple operations:
- Setting a specific wallpaper by file path.
- Finding a wallpaper in a directory (recursively) that has not been previously used.
- Marking certain wallpapers as favourites.
- Selecting a wallpaper that has been previously marked as a favourite.
- Possibly more, if this list is out of date...

Currently, the only database format supported is a flat text file. This is my personal preference, because it makes the data easily accessible. By default, the program currently uses "$XDG_DATA_HOME/wallpapers.log".

When looking for images on the file system, the default path is: "$XDG_DATA_HOME/wallpapers/". The program will complain if any entries in the database point outside of that path. Symlinks to files are always followed and symlinks to directories can be followed if requested.

Verbosity is configurable. See output of `--help` for usage.

A config file is supported if useful. By default, the file should be "$XDG_CONFIG_HOME/wallpaper-changer.conf". It is read line by line from top to bottom and left to right. Each line should contain the following elements in order:
1. A long-form parameter key (without the hyphens).
2. A key/value delimiter, which may be either a colon, an equals symbol, or whitespace.
3. A list of values, read from left to right, each separated by a comma, a semicolon, or whitespace.

## How to use it:

This will only work on (more or less) POSIX systems and will probably only work on linux. Those are because it currently uses unistd.h and _GNU_SOURCE. I have only tested it with Xorg but it might work with Wayland.

### Libraries required:
- xcb
- xcb-image
- xcb-util
- xcb-errors
- FreeImage (for reading in image files)
- magic (for parsing MIME types)

### Tool chain:
- I have only tried compiling this with G.C.C. That is what the build script uses.
- You may need Bash to run the build script.

### Compilation:

I have included the lazy Bash script I used for development, "[redo.sh](redo.sh)". If you have all the dependencies installed, you *should* be able to compile the program simply by running the script. If that fails, you should also be able to see the commands it would run by providing the word `print` as a parameter. The script also supports a few other convenience features for development. By default, the program will be built for debugging. To optimise for production, provide the build script with the word `production` as a parameter.

Depending on how your system libraries are packages, you may need to change the library names in the build script. They are defined with other would-be constants near the top of the script.

## Development:

Contribution is most welcome. I tried to make it relatively easy to add support for alternative system libraries and database formats. I don't really expect to add that support but who knows.
