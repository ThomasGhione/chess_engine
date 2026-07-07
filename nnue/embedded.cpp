// Embeds nnue/net/hydray.nnue into the binary via the GNU assembler's .incbin
// (works for both the Linux build and the mingw-w64 cross build), so ./chess
// is self-contained: `setoption name UseNNUE value true` needs no EvalFile.
//
// The blob is the verbatim quantised.bin (payload + "bullet" padding), 64-byte
// aligned so the Network reinterpret in nnue.cpp satisfies the AVX2 loads.
// The .incbin path is resolved from the compiler's working directory (the
// repo root — see the explicit makefile rule that also declares the data
// dependency, since -MMD cannot track .incbin includes).

__asm__(
    ".section .rodata\n"
    ".balign 64\n"
    ".global g_hydrayEmbeddedNetStart\n"
    "g_hydrayEmbeddedNetStart:\n"
    ".incbin \"nnue/net/hydray.nnue\"\n"
    ".global g_hydrayEmbeddedNetEnd\n"
    "g_hydrayEmbeddedNetEnd:\n"
    ".text\n"
);
