# randsim
Random number generation SFMT algorithm with multi-threads

SIMD-oriented Fast Mersenne Twister (SFMT). twice faster than Mersenne Twister. Please refer to the official link of SFMT for the detail.
The official SFMT is here: http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/index.html

What is SFMT?

SFMT is a new variant of Mersenne Twister (MT) introduced by Mutsuo Saito and Makoto Matsumoto in 2006. The algorithm was reported at MCQMC 2006. The article published in the proceedings of MCQMC2006. (see Prof. Matsumoto's Papers on random number generation.) SFMT is a Linear Feedbacked Shift Register (LFSR) generator that generates a 128-bit pseudorandom integer at one step. SFMT is designed with recent parallelism of modern CPUs, such as multi-stage pipelining and SIMD (e.g. 128-bit integer) instructions. It supports 32-bit and 64-bit integers, as well as double precision floating point as output.

SFMT is much faster than MT, in most platforms. Not only the speed, but also the dimensions of equidistributions at v-bit precision are improved. In addition, recovery from 0-excess initial state is much faster. See Master's Thesis of Mutsuo Saito for detail.

The implementation SFMT19937 can be compiled in three possible platforms:

Standard C without SIMD instructions
CPUs with Intel's SSE2 instructions + C compiler which supports these feature
CPUs with PowerPC's AltiVec instructions + C compiler which supports these feature

Here (this github repository) is just using it and tested at MacOS, but should also be OK in all Linux platform.
On my testing Mac computer, when I build with sse=1 and run it with 3 threads, the best performance is 1.9G/s generation speed. The random number generation performance detail is as follows:

Algorithm           Speed
SFMT SSE2 BLOCK     1.922G/s
SFMT SSE2 SEQUENCE  1.202G/s
C++ std::mt19937    0.142G/s

All the tests are run with 3 threads.

----------------------------------------

How to build:

$ make

How to run:

$ ./randsim-sse

License:

SFMT, as well as MT, can be used freely for any purpose, including commercial use.
