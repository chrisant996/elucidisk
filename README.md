# Elucidisk

This uses a sunburst chart to visualize disk space usage on Windows.

It is modelled after [Scanner](http://www.steffengerlach.de/freeware/), 
[Diskitude](https://madebyevan.com/diskitude/), and other similar disk space 
viewers.

The name "Elucidisk" is a portmanteau of "elucidate" and "disk".

![image](https://raw.githubusercontent.com/chrisant996/elucidisk/master/demo.png)

## What can it do?

- Scans drives or folders to find the size used.
- Shows the results in a sunburst chart.
- The chart has several configurable options:
    - Use plain colors.
    - Use rainbow colors (based on angle in the sunburst).
    - Use heatmap colors (based on size).
    - Show/hide names of files and directories in the sunburst (if the name fits).
    - Show/hide free space for drives.
    - Use the actual compressed size for compressed files, instead of the uncompressed size.
    - Show arcs with proportional area (e.g. two arcs for 50 GB directories will have the same area, even if they are at different distances from the center).
    - Show size comparison bar when hovering over an arc (the comparison bars are always in the center ring, so their sizes are comparable even when Proportional Area is turned off).
- Show combined summary chart for all local drives.
- Right click on an arc for a context menu of available actions.
- Right click elsewhere for a context menu of configurable options (or press <kbd>Shift</kbd>-<kbd>F10</kbd> or <kbd>Apps</kbd> key).

Please feel free to [open 
issues](https://github.com/chrisant996/elucidisk/issues) for suggestions, 
problem reports, or other feedback.

If you want to contribute, fork the repo and create a topic branch, and send a 
pull request for your topic branch.  Also, consider opening an issue first and 
discussing the contribution you want to make.

## Why was it created?

When viewing a sunburst chart of disk space usage, I want the free space on a 
disk to show up in the chart.  The only sunburst disk space visualizer I could 
find that includes the free space is Scanner.

I also wanted a few improvements to the user interface, such as highlighting 
the arc under the mouse pointer and showing names of directories/files when
the name fits in the arc.

I wanted to use the MIT license.  Most disk space visualizers are either
proprietary or use a "viral" version of GPL license.

So, I wrote my own.

It is written in C++ and uses DirectX for rendering.

## Command-line arguments

Elucidisk can be started from command-line, to scan a certain **drive** or **folder** by passing the
relevant path as an (optional) argument:

    elucidisk [path-to-drive-or-folder]

## File Explorer integration

Included in the repo (and in recent release archives) is a folder `RegistryFiles`,
containing a couple of *Windows Registry* (`.reg`) files:

* [Add_Elucidisk_to_File_Explorer_context_menu.reg](RegistryFiles/Add_Elucidisk_to_File_Explorer_context_menu.reg)
* [Remove_Elucidisk_from_File_Explorer_context_menu.reg](RegistryFiles/Remove_Elucidisk_from_File_Explorer_context_menu.reg)

These files can be used to add / remove the following two *context-menu* entries
in **Windows File Explorer**:

* **Scan drive with Elucidisk**
* **Scan folder with Elucidisk**

To add these two *context-menu* entries, perform the following steps:
1. Right-click the file `Add_Elucidisk_to_File_Explorer_context_menu.reg` and choose **Edit**.
1. Inspect the file contents and make sure the 4 paths specifying `elucidisk.exe` are matching the location of where you installed Elucidisk on your PC (the preset path is `C:\Program Files\Elucidisk\elucidisk.exe`).
1. (Optionally, rename the titles of these two *context-menu* entries to your liking.)
1. Save the changes you made (if any).
1. Finally, "merge" this `.reg` file into the registry, by double-clicking it (or by right-clicking it and choosing `Merge`).

To remove the same context menu entries, simply merge the file `Remove_Elucidisk_from_File_Explorer_context_menu.reg` into the registry (there's no need to edit paths this file).

## Building Elucidisk

Elucidisk uses [Premake](http://premake.github.io) to generate Visual Studio solutions. Note that Premake >= 5.0.0-beta8 is required.

1. Cd to your clone of elucidisk.
2. Run <code>premake5.exe <em>toolchain</em></code> (where <em>toolchain</em> is one of Premake's actions - see `premake5.exe --help`).
3. Build scripts will be generated in <code>.build\\<em>toolchain</em></code>. For example `.build\vs2019\elucidisk.sln`.
4. Call your toolchain of choice (Visual Studio, msbuild.exe, etc).

