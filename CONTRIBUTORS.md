# Contributors

## Project Creator & Maintainer

**Peter Ebel** <peter.ebel@outlook.de>
- Original concept and implementation (2016)
- v1.1.0 OpenMP optimization and professional features (2025)
- Architecture design, performance optimization, documentation

## How to Contribute

### 🐛 Bug Reports
- Use [GitHub Issues](https://github.com/PeterEbel/hgt2png/issues)
- Include system info, compiler version, and HGT file details
- Provide minimal reproduction steps

### ✨ Feature Requests  
- Open a [GitHub Issue](https://github.com/PeterEbel/hgt2png/issues) with "Feature Request" label
- Describe use case and expected behavior
- Consider performance and memory impact

### 🔧 Code Contributions
1. Fork the repository
2. Create feature branch: `git checkout -b feature/amazing-feature`
3. Follow code style: GNU99 C standard, const-correctness
4. Test with multiple HGT files and configurations
5. Update documentation if needed
6. Submit Pull Request

### 📖 Documentation
- Improve README.md, Options.md, INSTALL.md
- Add usage examples and workflow guides
- Translate documentation to other languages

## Code Style Guidelines

- **C Standard**: GNU99 with OpenMP extensions
- **Memory Safety**: All allocations checked, overflow protection
- **Const-Correctness**: Mark read-only parameters as const
- **Error Handling**: Clean cleanup on all error paths
- **Performance**: Profile before optimizing, measure impact
- **Threading**: Use OpenMP for parallelization, avoid raw pthreads

## Testing

### Required Tests
- Compile on Ubuntu/Debian/Fedora/Arch
- Test with various HGT file sizes (SRTM-1, SRTM-3)
- Verify all command-line options work
- Memory leak testing with Valgrind
- Performance benchmarking

### Test Commands
```bash
# Basic functionality
make clean && make check-deps && make
./hgt2png --help
./hgt2png test.hgt

# Memory leak check
make debug
valgrind --leak-check=full ./hgt2png test.hgt

# Performance test
time ./hgt2png -s 3 large_terrain.hgt
```

## Release Process

1. Update version in hgt2png.c header
2. Update CHANGELOG.md with new features
3. Test on multiple platforms
4. Create GitHub release with binaries
5. Update documentation

---

**Thank you for contributing to HGT2PNG!** 🏔️