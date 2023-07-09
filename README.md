# pixel-engine
Personal project

I always thought it was silly how old school style pixel art games are rendered with modern hardware using polygons, so I decided to make a compute shader based "software renderer" that emulates old school game graphics with scanline emulation for the kinds of cool scanline effects you could get with old school consoles.
The NES was my main inspiration, but my renderer has more colors per tile and a higher resolution, so it's not fully 8-bit but it is based on how the NES renders its graphics. 

I'm working on a simple side scrolling platformer/shooter to showcase the renderer (Currently in a very early stage of development) along with an editor using Dear IMGUI.

Original CRT shader by Timothy Lottes (https://www.shadertoy.com/view/XsjSzR)
