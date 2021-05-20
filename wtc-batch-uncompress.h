#ifndef _ZCD_BATCH_UNCOMPRESS_H_
#define _ZCD_BATCH_UNCOMPRESS_H_

#include "wtc-bits-mngt.h"

// TAG BitField --------------------------------------------------
typedef union { // (Serialized with 8 bits)
	unsigned char byte;
	struct {

		unsigned char sz:3;
		unsigned char lbl:5;
	} bits;
} tag_t;

typedef union {
	unsigned char	byte;
	struct {
		unsigned char		abs:1;
		bm_sample_type_t	type:4;
	} bits;
} br_tv_type_t;

typedef union{
	unsigned char byte;
	struct {
		unsigned char	null:1;
		unsigned char	cts:1;
		unsigned char	no_sample:1;
		unsigned char	batch_req:1;
		unsigned char	nb_of_type_measure:4;
	} bits;
}flag;

// sample contents -----------------------------------------
typedef struct { // (Fully Serialized with about
	bm_sample_t v;			// V
	bm_sample_t t; 		 // T or dT according to tt
}br_uncompress_sample_t;

#define NUMBER_OF_SERIES	16

typedef struct { // (Fully Serialized with about
	tag_t tag;				// Tag of uncompress
	unsigned char  coding_type;
	unsigned char  coding_table;
	br_uncompress_sample_t  uncompress_samples[200];
	unsigned char uncompress_samples_nb;
	unsigned char compress_samples_nb;
	br_tv_type_t tv;    	// Value representation type
	bm_sample_t resolution;
}br_uncompress_serie_t;

typedef struct{
	br_uncompress_serie_t br_uncompress_serie[NUMBER_OF_SERIES];
	unsigned long timestamp;
	unsigned char counter;
	unsigned char batch_req;
}br_uncompress_series_t;

void br_uncompress_serie_dump(br_uncompress_series_t *br_uncompress_series);

#endif // _ZCD_BATCH_UNCOMPRESS_H_
