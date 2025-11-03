#!/bin/bash

# ================================================================================
# HGT2PNG VEGETATION MASK TEST SCRIPT
# Demonstrates the new Alpine biome vegetation mask generation
# ================================================================================

echo "🌲 HGT2PNG Alpine Vegetation Mask Generator Test"
echo "================================================"

# Check if hgt2png is compiled
if [ ! -f "./hgt2png" ]; then
    echo "❌ ERROR: hgt2png not found. Please compile first:"
    echo "   gcc -std=gnu99 -Wall -Wextra -O2 hgt2png.c -o hgt2png -lm -lpng -pthread -fopenmp"
    exit 1
fi

# Check for HGT files in current directory
HGT_FILES=$(find . -name "*.hgt" -o -name "*.HGT" | head -3)

if [ -z "$HGT_FILES" ]; then
    echo "❌ No HGT files found in current directory."
    echo "💡 Please download SRTM HGT files from:"
    echo "   https://dds.cr.usgs.gov/srtm/version2_1/SRTM3/"
    echo "   https://dds.cr.usgs.gov/srtm/version2_1/SRTM1/"
    echo ""
    echo "🔍 Example files for Alpine regions:"
    echo "   N46E007.hgt  (Switzerland - Matterhorn region)"
    echo "   N47E008.hgt  (Switzerland - Bernese Alps)"
    echo "   N45E006.hgt  (France/Italy - Mont Blanc region)"
    exit 1
fi

echo "📁 Found HGT files:"
echo "$HGT_FILES"
echo ""

# Test each HGT file
for hgt_file in $HGT_FILES; do
    echo "🏔️ Testing Alpine vegetation mask for: $hgt_file"
    
    # Generate standard heightmap
    echo "   1️⃣ Generating standard heightmap..."
    ./hgt2png -q -s 4"$hgt_file"
    
    # Generate Alpine vegetation mask
    echo "   2️⃣ Generating Alpine vegetation mask..."
    ./hgt2png -q -s 4 --vegetation-mask --biome alpine "$hgt_file"
    
    # Check results
    base_name=$(basename "$hgt_file" .hgt)
    base_name=$(basename "$base_name" .HGT)
    
    heightmap_png="${base_name}.png"
    vegetation_png="${base_name}_vegetation_alpine.png"
    
    if [ -f "$heightmap_png" ] && [ -f "$vegetation_png" ]; then
        echo "   ✅ SUCCESS: Generated $heightmap_png and $vegetation_png"
        
        # Show file sizes
        heightmap_size=$(stat -c%s "$heightmap_png" 2>/dev/null || echo "unknown")
        vegetation_size=$(stat -c%s "$vegetation_png" 2>/dev/null || echo "unknown")
        
        echo "   📊 File sizes:"
        echo "      Heightmap: $heightmap_size bytes"
        echo "      Vegetation: $vegetation_size bytes"
    else
        echo "   ❌ ERROR: Failed to generate PNG files"
    fi
    echo ""
done

echo "🎯 TEST SUMMARY:"
echo "==============="
echo "✅ Alpine vegetation mask generation tested"
echo ""
echo "📋 HOW TO USE THE RESULTS:"
echo "-------------------------"
echo "1. Import heightmap PNG into Blender as Displacement"
echo "2. Import vegetation mask PNG as Factor for mixing:"
echo "   - White areas (255) = Maximum vegetation density"
echo "   - Black areas (0) = No vegetation (rock/snow/steep slopes)"
echo "   - Gray areas = Gradual vegetation density"
echo ""
echo "🏔️ ALPINE BIOME PARAMETERS:"
echo "   • Vegetation range: 700m - 2000m elevation"
echo "   • Tree line: 1800m (spruce/larch limit)"
echo "   • Bush line: 2200m (dwarf pine/rhododendron)"
echo "   • Grass line: 2500m (alpine meadows)"
echo "   • Maximum slope: 60° for vegetation"
echo "   • South faces drier, North faces moister"
echo "   • Valleys have more vegetation than ridges"
echo ""
echo "🎨 For best results in Blender, use with blender_dyer.py:"
echo "   create_pbr_terrain_material(biome='alpine', use_advanced_mixing=True)"
