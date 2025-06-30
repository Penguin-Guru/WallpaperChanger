# WallpaperChanger

This is a fairly simple command-line utility to manage wallpapers.
- It does not provide a GUI, TUI, or dialogue-style interface.
- It has no network functionality.
- It does not run a continuous process.
- It has relatively few [dependencies](#libraries).

Simple multi-monitor support has been implemented (with the Randr extension). See [limitations](#limitations) below.

## Primary functions:

- Set a specific wallpaper by file path.
- Search a directory (recursively) for a wallpaper that has not been previously used.
- Mark certain wallpapers as favourites.
- Select a random wallpaper that has been previously marked as a favourite.
- Possibly more, if this list is out of date...

## How it works:

See output of `--help` for usage.
Note that the level of verbosity is configurable.

Currently, the only database format supported is a flat text file. This is my personal preference, because it makes the data easily accessible. By default, the program currently uses `$XDG_DATA_HOME/wallpapers.log`.

When looking for images on the file system, the default path is: `$XDG_DATA_HOME/wallpapers/`. The program will sometimes complain if any entries in the database point outside of that path-- this is a safety mechanism. Symlinks to files are always followed and symlinks to directories can be followed if requested.

A config file is supported but not required. By default, the file should be `$XDG_CONFIG_HOME/wallpaper-changer.conf`. It is read line by line from top to bottom and left to right. Each line should contain the following elements in order:
1. A long-form parameter key (without the hyphens).
2. A key/value delimiter, which may be either a colon, an equals symbol, or whitespace.
3. A list of values, read from left to right, each separated by a comma, a semicolon, or whitespace.

### Integrations:

To automatically re-load the most recent wallpapers for connected monitors when you start Xorg, add a line like `wallpaperchanger -r` to your `~/.xinitrc` (or such).

If you would like to periodically change your wallpaper, there are various job schedulers like [Cron](https://en.wikipedia.org/wiki/Cron) that can be used.


### Limitations:

While I have chosen not to run a continuous process, there is one drawback: disconnecting certain monitors causes wallpapers on some still connected monitors to be be mangled. I don't know of a window-manager-agnostic way to handle these sort of events. In such cases, you will simply need to run this program again (with the `-r` flag). An external process that can hook or detect those events could be used but I do not know of one to recommend.

Currently, wallpaper file paths must not contain semicolons. This is generally discouraged anyway. The program *should* warn you if it encounters any.

## Requirements:

This will only work on (more or less) POSIX systems and I have only tested it on Linux. I have also only tested it with Xorg, but it *might* work with Wayland.

### Libraries:
- xcb
- xcb-image
- xcb-randr
- xcb-util
- xcb-errors
- FreeImage (for reading in image files)

### More libraries (you probably already have):
- unistd.h
- magic.h
- glibc >= 2.38 (or something similar that provides strchrnul)

### Tool chain:
- I have only tried compiling this with G.C.C. That is what the build script uses.
- You *may* need Bash to run the build script.

### Compilation:

I have included the lazy Bash script I used for development, "[redo.sh](redo.sh)". If you have all the dependencies installed, you *should* be able to compile the program simply by running the script. If that fails, you should also be able to see the commands it would run by providing the word `print` as a parameter. The script also supports a few other convenience features for development. By default, the program will be built for debugging. To optimise for production, provide the build script with the word `production` as a parameter.

Depending on how your system libraries are packaged, you may need to change the library names in the build script. They are defined with other would-be constants near the top of the script.

## Installation:

"Installation" here just means moving the (single) compiled program file into a directory referenced by your system's "path" (`$PATH`). This allows the program to be run by name, without specifying the full path to the file. The [conventional](https://en.wikipedia.org/wiki/Filesystem_Hierarchy_Standard) directory would be `/usr/local/bin/`.

## Development:

I do use this regularly, but I mostly wrote it for practice and the code is still a bit messy. I know there are a few areas in which the code can be improved and I'm sure there are some bugs I haven't caught yet. Please file any issues and I'll fix what I can.

Contribution is most welcome. I tried to make it relatively easy to add support for alternative system libraries and database formats. I don't really expect to add that support myself but who knows.
