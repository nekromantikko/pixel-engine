# pixel-engine
Personal project

I always thought it was silly how old school style pixel art games are rendered with modern hardware using polygons, so I decided to make a compute shader based "software renderer" that emulates old school game graphics with scanline emulation for the kinds of cool scanline effects you could get with old school consoles.
The NES was my main inspiration, but my renderer has more colors per tile and a higher resolution, so it's not fully 8-bit but it is based on how the NES renders its graphics. 

I'm working on a simple side scrolling platformer/shooter to showcase the renderer (Currently in a very early stage of development) along with an editor using Dear IMGUI.

Original CRT shader by Timothy Lottes (https://www.shadertoy.com/view/XsjSzR)

Video demonstrating nametable memory streaming:


https://github.com/nekromantikko/pixel-engine/assets/43074593/818813dd-a967-4786-ac1f-b2aa5c14bd72

Metasprite editing:


https://github.com/nekromantikko/pixel-engine/assets/43074593/e54be9e9-3f0e-4a45-9298-462aea4dcfaa

Metasprite collision editing:


https://github.com/nekromantikko/pixel-engine/assets/43074593/da4bfe57-9498-4c7d-b7ca-6dcfe853f44a


# TODO:
- Switch to cmake, ditch vckpg?
- Add assets to repo
