# Asset Packer Build Script

This build script automatically generates `assets.npak` at build time from source assets in the `assets/` directory.

## Overview

The asset packer is a standalone C++ tool that:
- Scans the assets directory for supported asset types
- Reads asset metadata from `.meta` files to preserve UUIDs
- Processes and packages assets into the NPAK format
- Integrates seamlessly with the CMake build system

## Supported Asset Types

Currently supports the following asset types:
- **Sprites** (`.sprite` files) - JSON-based metasprite definitions
- **Palettes** (`.dat` files) - Binary palette data
- **CHR Banks** (`.bmp` files) - Bitmap character sheet data

Additional asset types can be easily added by extending the `TryGetAssetTypeFromPath()` and processing functions.

## Build Integration

The asset packer is automatically built and run during the main project build:

1. CMake creates the `simple_asset_packer` executable
2. A custom target `generate_assets` runs the packer during build
3. The packer reads from `${CMAKE_SOURCE_DIR}/assets/`
4. Outputs `assets.npak` to `${CMAKE_BINARY_DIR}/assets.npak`

## Usage

### Automatic (via CMake)
The asset packer runs automatically during project builds. No manual intervention required.

### Manual
```bash
# Build the asset packer
mkdir build && cd build
cmake ..
make simple_asset_packer

# Run manually
./simple_asset_packer ../assets assets.npak
```

## Asset File Format

Assets must have corresponding `.meta` files containing:
```json
{
    "file_format_version": 1,
    "guid": 12345678901234567890
}
```

The GUID is used as the asset ID in the NPAK archive.

## NPAK Format

The generated `.npak` file uses the NPAK format:
- Header with signature "NPAK", asset count, and directory offset
- Consecutive asset data
- Directory with asset entries (ID, name, path, offset, size, flags)

## Extending Asset Support

To add support for new asset types:

1. Add the asset type to the enum in `simple_asset_packer.cpp`
2. Add file extension mapping in `TryGetAssetTypeFromPath()`
3. Implement a `ProcessXAsset()` function for the new type
4. Add the case to the switch statement in `CreateNPAK()`

## Dependencies

The asset packer has minimal dependencies:
- C++20 standard library
- No external dependencies (Vulkan, SDL2, etc.)
- No complex editor functionality dependencies

This ensures the build script can run in any environment where the main project can be built.