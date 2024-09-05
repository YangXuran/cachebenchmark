## Overview

This project is a memory and cache performance testing tool for ARMv8 systems. It is used to estimate the cache size and test the cache or memory performance of CPU. The tool supports multi-threaded parallel testing to evaluate the cache performance when multiple clusters are working together, making it suitable for cross-compilation and execution on ARMv8 systems.

### Features

- **Cache Size Estimation and Performance Testing**: Measures cache size and performance on ARMv8.
- **`memcpy` Performance Test**: Uses SIMD instructions from the GNU C Library to test `memcpy` performance.
- **Read/Write Bandwidth Testing**: Uses SIMD code from the "Bandwidth: A Memory Bandwidth Benchmark" tool to test read/write rates with varying data sizes.
- **Multi-threaded Support**: Allows for parallel testing across multiple clusters to analyze cache performance in multi-core environments by using OpenMP.
- **Cross-Compilation Ready**: Designed for easy cross-compilation and execution on embedded ARMv8 systems.
- **Graphical Output**: Generates line charts of performance results using `gnuplot`.

### Dependencies

This tool requires `gnuplot` to generate performance line charts. Before using the tool, ensure that `gnuplot` is installed and properly configured on your ARMv8 embedded device.

Download 'gnuplot' from [](https://sourceforge.net/projects/gnuplot/files/gnuplot/)
Build it for arm64

```
./configure --host=arm64 CC=aarch64-linux-gnu-gcc --prefix=$(pwd)/install
make
make install
```

## Open Source Software Used

This project uses and modifies code from the following open source software projects and is released in accordance with their respective licenses:

1. **GNU C Library** - Licensed under the GNU Lesser General Public License v2.1 or later.
   - Original source: [GNU C Library](https://www.gnu.org/software/libc/)

2. **Bandwidth: A Memory Bandwidth Benchmark** - Licensed under the GNU General Public License v2 or later.
   - Original source: [Bandwidth: A Memory Bandwidth Benchmark](https://zs3.me/bandwidth.php)

All modifications made to these projects are released under the terms of their respective licenses.
