# Codec-Report-Batch-C

To compile the source:

gcc -Wno-char-subscripts -fshort-enums -o br_compress br_compress.c wtc-batch-report.c wtc-huff.c ../../core/lib/math-def.c ../../core/lib/list.c -I ../../core
