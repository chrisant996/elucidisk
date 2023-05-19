# Elucidisk

This uses a sunburst chart to visualize disk space usage on Windows.

It is modelled after [Scanner](http://www.steffengerlach.de/freeware/), 
[Diskitude](https://madebyevan.com/diskitude/), and other similar disk space 
viewers.

## What can it do?

## Why was it created?

I want the free space on a disk to show up in the sunburst chart.  The only 
sunburst disk space visualizer I could find that includes the free space is
Scanner.

I also wanted a few improvements to the user interface, such as highlighting 
the arc under the mouse pointer and showing names of directories/files when
the name fits in the arc.

I wanted to use the MIT license.  Most disk space visualizers are either
proprietary or use a "viral" version of GPL license.

So, I wrote my own.

It is written in C++ and uses DirectX for rendering.
