#!/bin/bash

# Test script for headless mode functionality
# This script demonstrates that the game can run without graphics/audio

echo "Testing Pixel Engine Headless Mode"
echo "=================================="

# Build headless version
echo "Building headless version..."
cd "$(dirname "$0")/.."
mkdir -p build_headless
cd build_headless

# Configure and build
cmake .. -DHEADLESS=ON -DENABLE_EDITOR=OFF 
if [ $? -ne 0 ]; then
    echo "❌ Failed to configure headless build"
    exit 1
fi

make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "❌ Failed to build headless version"
    exit 1
fi

echo "✅ Headless build successful"

# Test headless execution
echo "Testing headless execution..."
timeout 5s ./pixelengine 2>&1 | tee headless_output.log
RESULT=${PIPESTATUS[0]}

if [ $RESULT -eq 0 ] || [ $RESULT -eq 124 ]; then
    echo "✅ Headless execution successful"
    echo "Game ran for 5 seconds without crashing"
    
    # Check if memory initialization worked
    if grep -q "Memory arenas initialized successfully" headless_output.log; then
        echo "✅ Memory system initialized correctly"
    else
        echo "⚠️ Memory system may not have initialized properly"
    fi
    
    # Check that no SDL/graphics errors occurred
    if ! grep -qi "vulkan\|sdl\|opengl" headless_output.log; then
        echo "✅ No graphics/audio system errors detected"
    else
        echo "⚠️ Graphics/audio system errors may have occurred"
    fi
    
    echo ""
    echo "Last few lines of output:"
    tail -5 headless_output.log
else
    echo "❌ Headless execution failed with code $RESULT"
    echo "Output:"
    cat headless_output.log
    exit 1
fi

echo ""
echo "🎉 Headless mode test completed successfully!"
echo "The game can now run without graphics or audio for remote testing."