#!/usr/bin/env python3
"""
Quantization Analysis Tool for HGT2PNG
Analyzes height value distribution to detect terrassierung artifacts.
"""

import numpy as np
import matplotlib.pyplot as plt
from PIL import Image
import sys

def analyze_image_quantization(image_path):
    """Analyze quantization patterns in a PNG heightmap."""
    print(f"Analyzing: {image_path}")
    
    # Load image
    img = Image.open(image_path)
    
    if img.mode == 'RGB':
        # 8-bit RGB heightmap
        data = np.array(img.convert('L'))
        bit_depth = 8
        max_val = 255
    elif img.mode == 'L':
        # Grayscale (8-bit or 16-bit)
        data = np.array(img)
        if data.dtype == np.uint8:
            bit_depth = 8
            max_val = 255
        else:
            bit_depth = 16
            max_val = 65535
    elif img.mode == 'I;16':
        # 16-bit grayscale
        data = np.array(img)
        bit_depth = 16
        max_val = 65535
    else:
        print(f"Unsupported image mode: {img.mode}")
        return
    
    print(f"Image: {data.shape[1]}×{data.shape[0]} pixels, {bit_depth}-bit")
    print(f"Data range: {data.min()} - {data.max()}")
    
    # Analyze height value distribution
    unique_values, counts = np.unique(data, return_counts=True)
    print(f"Unique height values: {len(unique_values)}")
    print(f"Theoretical maximum: {max_val + 1}")
    print(f"Utilization: {len(unique_values)/(max_val + 1)*100:.1f}%")
    
    # Detect quantization patterns
    value_gaps = np.diff(unique_values)
    gap_counts = np.unique(value_gaps, return_counts=True)
    
    print(f"\nQuantization Analysis:")
    print(f"Average gap between values: {np.mean(value_gaps):.2f}")
    print(f"Standard deviation: {np.std(value_gaps):.2f}")
    
    # Look for regular patterns (terrassierung indicators)
    if len(gap_counts[0]) < 10:
        print("Regular gaps detected (potential terrassierung):")
        for gap, count in zip(gap_counts[0], gap_counts[1]):
            if count > len(unique_values) * 0.01:  # More than 1% of gaps
                print(f"  Gap {gap}: {count} occurrences ({count/len(value_gaps)*100:.1f}%)")
    
    # Sample a small region for detailed analysis
    center_y, center_x = data.shape[0] // 2, data.shape[1] // 2
    sample_size = min(100, data.shape[0] // 4, data.shape[1] // 4)
    sample = data[center_y:center_y+sample_size, center_x:center_x+sample_size]
    
    # Calculate gradient magnitude to assess smoothness
    grad_y, grad_x = np.gradient(sample.astype(float))
    gradient_magnitude = np.sqrt(grad_x**2 + grad_y**2)
    
    print(f"\nLocal Gradient Analysis (center {sample_size}×{sample_size} region):")
    print(f"Mean gradient magnitude: {np.mean(gradient_magnitude):.2f}")
    print(f"Max gradient magnitude: {np.max(gradient_magnitude):.2f}")
    print(f"Sharp transitions (>10): {np.sum(gradient_magnitude > 10)} pixels")
    
    # Generate histogram
    plt.figure(figsize=(12, 8))
    
    # Height value histogram
    plt.subplot(2, 2, 1)
    plt.hist(data.flatten(), bins=min(256, len(unique_values)), alpha=0.7, color='skyblue')
    plt.title('Height Value Distribution')
    plt.xlabel('Height Value')
    plt.ylabel('Pixel Count')
    plt.yscale('log')
    
    # Gap size histogram
    plt.subplot(2, 2, 2)
    plt.hist(value_gaps, bins=min(50, len(np.unique(value_gaps))), alpha=0.7, color='lightcoral')
    plt.title('Gap Sizes Between Consecutive Height Values')
    plt.xlabel('Gap Size')
    plt.ylabel('Count')
    
    # Gradient magnitude map
    plt.subplot(2, 2, 3)
    plt.imshow(gradient_magnitude, cmap='hot', interpolation='nearest')
    plt.title('Gradient Magnitude (Center Region)')
    plt.colorbar()
    
    # Sample height profile
    plt.subplot(2, 2, 4)
    profile_line = sample[sample_size//2, :]
    plt.plot(profile_line, 'b-', linewidth=1)
    plt.title('Height Profile (Horizontal Line)')
    plt.xlabel('Pixel Position')
    plt.ylabel('Height Value')
    
    plt.tight_layout()
    
    # Save analysis plot
    output_path = image_path.replace('.png', '_quantization_analysis.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"\nAnalysis plot saved to: {output_path}")
    
    # Quality assessment
    smoothness_score = 1.0 / (1.0 + np.std(value_gaps))
    detail_score = len(unique_values) / (max_val + 1)
    
    print(f"\nQuality Metrics:")
    print(f"Smoothness Score: {smoothness_score:.3f} (higher = more smooth transitions)")
    print(f"Detail Score: {detail_score:.3f} (higher = more detail levels)")
    print(f"Overall Quality: {(smoothness_score * detail_score):.3f}")
    
    return {
        'unique_values': len(unique_values),
        'smoothness_score': smoothness_score,
        'detail_score': detail_score,
        'mean_gradient': np.mean(gradient_magnitude),
        'max_gradient': np.max(gradient_magnitude)
    }

def compare_images(old_path, new_path):
    """Compare two heightmaps for quantization improvements."""
    print("=" * 60)
    print("QUANTIZATION COMPARISON ANALYSIS")
    print("=" * 60)
    
    print("\n--- OLD IMAGE (Before Fix) ---")
    old_metrics = analyze_image_quantization(old_path)
    
    print("\n--- NEW IMAGE (After Fix) ---")
    new_metrics = analyze_image_quantization(new_path)
    
    print("\n--- IMPROVEMENT SUMMARY ---")
    improvements = {}
    for key in old_metrics:
        old_val = old_metrics[key]
        new_val = new_metrics[key]
        change = ((new_val - old_val) / old_val) * 100 if old_val != 0 else 0
        improvements[key] = change
        
        print(f"{key}: {old_val:.3f} → {new_val:.3f} ({change:+.1f}%)")
    
    # Overall assessment
    key_improvements = [
        improvements['smoothness_score'],
        improvements['detail_score'],
        -improvements['max_gradient']  # Lower max gradient is better
    ]
    
    overall_improvement = np.mean(key_improvements)
    print(f"\nOverall Improvement: {overall_improvement:+.1f}%")
    
    if overall_improvement > 5:
        print("✅ Significant improvement in quantization quality!")
    elif overall_improvement > 0:
        print("✅ Moderate improvement detected.")
    else:
        print("⚠️  No clear improvement detected.")

if __name__ == "__main__":
    if len(sys.argv) == 2:
        # Single image analysis
        analyze_image_quantization(sys.argv[1])
    elif len(sys.argv) == 3:
        # Comparison mode
        compare_images(sys.argv[1], sys.argv[2])
    else:
        print("Usage:")
        print("  python3 analyze_quantization.py <image.png>           # Analyze single image")
        print("  python3 analyze_quantization.py <old.png> <new.png>  # Compare two images")