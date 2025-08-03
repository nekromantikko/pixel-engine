# GitHub Workflow Setup

This repository includes a GitHub Actions workflow for build validation that ensures all code changes compile successfully before being merged.

## Workflow Details

- **File**: `.github/workflows/build.yml`
- **Triggers**: Push to master branch and all pull requests to master
- **Environment**: Ubuntu latest with required dependencies
- **Build**: CMake configuration and compilation of both main executable and asset packer

## Setting up Branch Protection

To require this workflow to pass before merging pull requests:

1. Go to your repository settings
2. Navigate to "Branches" 
3. Add a protection rule for the `master` branch
4. Enable "Require status checks to pass before merging"
5. Select "Build Validation" workflow as a required check
6. Enable "Require branches to be up to date before merging"

## Dependencies Installed

The workflow automatically installs:
- libvulkan-dev (Vulkan development libraries)
- libsdl2-dev (SDL2 development libraries) 
- glslang-tools (GLSL shader validator)
- build-essential (GCC compiler and build tools)

## Build Configuration

- Build Type: Release
- Editor: Enabled (`-DENABLE_EDITOR=ON`)
- Parallel compilation using all available CPU cores