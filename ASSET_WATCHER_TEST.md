# Asset Watcher Testing Guide

The Asset Watcher has been implemented and integrated into the pixel engine editor. Here's how to test its functionality:

## How it Works

The Asset Watcher monitors the source assets directory (`ASSETS_SRC_DIR`) for changes and automatically updates the in-memory asset manager when files are added, deleted, modified, or renamed.

### Key Features Implemented:

1. **File Addition**: When a new asset file is added to the directory, the watcher:
   - Detects it's a valid asset type
   - Creates a .meta file with a new GUID if one doesn't exist
   - Loads the asset data and adds it to the AssetManager

2. **File Deletion**: When an asset file is deleted:
   - Removes the corresponding asset from the AssetManager
   - Cleans up any orphaned .meta files

3. **File Modification**: When an asset file is changed:
   - Reloads the asset by treating it as delete + add

4. **File Renaming/Moving**: When files are renamed or moved:
   - Treats it as delete from old location + add to new location
   - Handles coordinated moves where both asset and .meta files move together

## Testing Instructions

### Manual Testing in Editor:

1. **Build and run the editor**:
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   make pixelengine
   ./pixelengine
   ```

2. **Test File Addition**:
   - While the editor is running, copy a .bmp file to the assets directory
   - Watch the console output for: "Asset watcher: File added: ..."
   - The asset should appear in the editor's asset browser

3. **Test File Deletion**:
   - Delete an asset file from the assets directory
   - Watch for: "Asset watcher: File deleted: ..."
   - The asset should disappear from the editor

4. **Test File Modification**:
   - Edit an existing asset file (e.g., with an image editor)
   - Watch for: "Asset watcher: File modified: ..."
   - The asset should be reloaded

### Expected Console Output:

When the editor starts:
```
Asset watcher initialized for directory: /path/to/assets (found X tracked files)
```

When files change:
```
Asset watcher: Processing N file changes
Asset watcher: File added: /path/to/new_asset.bmp
Asset watcher: Successfully added asset: /path/to/new_asset.bmp with GUID: 123456
```

### Testing Script:

Run the provided test script to simulate file operations:
```bash
/tmp/test_asset_watcher.sh
```

Then perform file operations in `/tmp/test_assets` while the editor is pointing to that directory.

## Implementation Details

- **Polling Interval**: 1 second (configurable via `SCAN_INTERVAL`)
- **File Types**: Monitors all files that `AssetSerialization::TryGetAssetTypeFromPath` recognizes
- **Error Handling**: Graceful handling of file access errors and partial writes
- **Performance**: Throttled to avoid excessive file system access

## Code Integration

The watcher is integrated into the editor lifecycle:

- **Initialization**: `Editor::Init()` calls `AssetWatcher::Init(ASSETS_SRC_DIR)`
- **Updates**: `Editor::Update()` calls `AssetWatcher::Update(currentTime)`
- **Cleanup**: `Editor::Free()` calls `AssetWatcher::Free()`

## Troubleshooting

If the watcher isn't working:

1. Check that `EDITOR` is defined during compilation
2. Verify the assets directory exists and is accessible
3. Look for error messages in the console output
4. Ensure the file operations are on valid asset types (.bmp, .dat, etc.)

The watcher provides detailed logging to help diagnose issues.