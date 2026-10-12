# Modern Linux build

This is a compatibility and performance extension of the existing Info-ZIP
source tree, not a declaration of an upstream UnZip 6.1 release.

Build from the repository root:

```sh
make -f modern/Makefile -j4
make -f modern/Makefile test
```

Use `CPU=znver2` for a Ryzen 4800U scheduling tune. Other compiler-supported
scheduling tunes include `znver3`, `znver4`, and `znver5`. CPU changes require
`make -f modern/Makefile clean` first. No `-march=native` is injected: scheduling
tuning preserves the compiler's default ISA, so the resulting binary does not
require that Ryzen generation. Optional explicit CFLAGS/LDFLAGS are respected.
There is no PGO, and no runtime ISA dispatch in this implementation.

Modern targets explicitly select GNU C17 for the legacy K&R source. This allows
modern GCC versions to compile it without pretending every function has been
rewritten in C23. Redundant gmtime/localtime declarations are removed.

The legacy `linux` target selects i386 assembly based on the compiler target,
not the build host. The modern target uses portable C, including an immutable
8 KiB slicing-by-eight IEEE CRC-32 table and endian/alignment-safe byte loads.
The SSE4.2 CRC32 instruction is not used: it implements CRC-32C, not ZIP's IEEE
polynomial. This does not introduce a new x86_64 assembly routine.

## Concurrent archive operations

```sh
./unzip-parallel -j 4 -t one.zip two.zip three.zip
mkdir extracted
./unzip-parallel -j 4 -d extracted one.zip two.zip three.zip
```

A bounded pthread queue runs independent UnZip child processes using
`posix_spawn`, without a shell. Each extraction gets a newly created numbered
directory (`archive-000001`, etc.) under the output root, corresponding to input
order. Existing job directories are rejected instead of overwritten. A failed
job returns nonzero; other jobs continue, and partial output is retained.

The launcher expects the `unzip` executable beside itself. It supports Linux's
/proc/self/exe. Archive names beginning with `-` should be supplied as absolute
paths or with a `./` prefix, since they are passed to UnZip as arguments.

Parallelism is across archives, not members within a single archive. It does not
share the legacy global UnZip state across threads. One compressed stream cannot
be divided into independent deflate jobs by this launcher. No arbitrary extra
UnZip options, interactive password handling or merging into one output directory
are supported by this interface. The ordinary unzip command remains available.

## Validation

`tests/crc-test.c` compares CRC against zlib, including known vectors, nonzero
seeds, every short length, sixteen alignments, large inputs and streaming updates.
`tests/modern-test.py` checks stored/deflated entries, funzip, SFX, four-worker
processing, empty files, spaced archive paths, corrupted archives, exit codes,
invalid job counts and existing output-directory collisions.

Local validation used GCC 13.3. Both generic and znver2 builds passed. Address
and undefined-behavior sanitizer checks of the CRC passed with LeakSanitizer
turned off because this container cannot inspect /proc task metadata.
GitHub CI requests GCC 13 and GCC 16; a configured CI job is not a completed
GCC 16.2 test. No performance speedup has been measured or asserted.

This work does not import the separate Fedora security patch series discussed
for the LPM recipe. Existing upstream behavior and security limitations remain;
apply and validate that patch series separately before a distribution release.
