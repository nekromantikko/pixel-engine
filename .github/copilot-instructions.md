## Dependencies

Most dependencies should be handled by cmake using FetchContent, take a look at CmakeLists.txt to find out more. 
It also depends on SDL2-2.28.5 and Vulkan 1.2, which need to be aquired separately if you intend to test. SDL2.dll needs to be placed in the build directory to run it.

## Code standards

For engine core and game code, C-style C++ is preferred. Heap allocations should be minimized, and there should especially be no heap allocation in hot loops. Stl containers should only be used if absolutely necessary, or as placeholders.
For editor code (Any source files with the word 'editor' in them, as well as '#ifdef EDITOR' blocks, heap allocation, dynamic arrays and other stl containers can be freely used for convenience. Make sure these don't leak into other parts of the code.
