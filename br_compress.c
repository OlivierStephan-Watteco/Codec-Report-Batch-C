// gcc -Wno-char-subscripts -fshort-enums -o br_compress br_compress.c wtc-batch-report.c wtc-huff.c ../../core/lib/math-def.c ../../core/lib/list.c -I ../../core
//
// Usage : see function usage()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wtc-batch-report.h"
#include <fcntl.h>

#define MAX_NB_SERIES  6
extern unsigned char current_buf_counter;
void usage() {
  FPRINTF(stderr,"USAGE:\n");
  FPRINTF(stderr,"  br_compress [-h | -v] tagsz \"taglbl,resol,delta,sampletype\" \"...\" ... < buf > result \n");
	FPRINTF(stderr,"  Input buf (on stdin) must be text file with one sample per line: \n");
    FPRINTF(stderr,"  '<count>\\n '\n");
    FPRINTF(stderr,"  '<timestamp>\\n '\n");
	FPRINTF(stderr,"  '<tstamp1>\\t<lbl1>\\t<value1>\\n '\n");
	FPRINTF(stderr,"  '<tstamp2>\\t<lbl3>\\t<value2>\\n '\n");
	FPRINTF(stderr,"  '<tstamp3>\\t<lbl3>\\t<value3>\\n '\n");
	FPRINTF(stderr,"  '.... '\n");
	FPRINTF(stderr,"  where lblx can be any of command line defined labels.\n");
	FPRINTF(stderr,"  Example: \n");
	FPRINTF(stderr,"    ./br_compress 3 1,10,1,7 2,1.0,1,12\n");
	FPRINTF(stderr,"      \nWhere SampleType can be:\n");
	FPRINTF(stderr,"       1: Boolean\n");
	FPRINTF(stderr,"       2: Unsigned int U4    3: Signed int I4 \n");
	FPRINTF(stderr,"       4: Unsigned int U8    5: Signed int I8 \n");
	FPRINTF(stderr,"       6: Unsigned int U16   7: Signed int I16\n");
	FPRINTF(stderr,"       8: Unsigned int U24   9: Signed int I24\n");
	FPRINTF(stderr,"      10: Unsigned int U32  11: Signed int I32\n");
	FPRINTF(stderr,"      12: Float\n");
}

bm_sample_t getSampleFromFloat(float floatVal, bm_sample_type_t st) {
	bm_sample_t sample;
	if (st == ST_BL)   { sample.u8  = ((unsigned char) floatVal > 0); }
	else if (st == ST_U8)   { sample.u8  = ( unsigned char) floatVal; }
	else if (st == ST_I8)   { sample.i8  = (   signed char) floatVal; }
	else if (st == ST_U16)  { sample.u16 = (unsigned short) floatVal; }
	else if (st == ST_I16)  { sample.i16 = (  signed short) floatVal; }
	else if (st == ST_U24)  { sample.u32 = (unsigned long)  floatVal; }
	else if (st == ST_I24)  { sample.i32 = (  signed long)  floatVal; }
	else if (st == ST_U32)  { sample.u32 = (unsigned long)  floatVal; }
	else if (st == ST_I32)  { sample.i32 = (  signed long)  floatVal; }
	else if (st == ST_FL)   { sample.fl  =                  floatVal; }
	else sample.u32 = 0;

	return sample;
}

unsigned char glbVerbose = 0;

int main(int argc, char *argv[]) {

	bm_sample_t tmpTimeStamp, tmpSample;
	bm_sample_type_t st;
	int stInt;
	uint32_t i=0;

	tag_t tag;

	br_serie_t series[MAX_NB_SERIES];
	br_serie_param_t params[MAX_NB_SERIES];
	br_serie_t *sPtr;
	unsigned char ret=BR_NO_ERR;

	if (argc < 2) {
		usage();  exit(-1);
	}

	int tag_lbl;
	float tmpFloat, resol_float;
	float delta_float;
	int noArg=1;

	if (strcmp(argv[noArg], "-h") ==  0) {
		glbVerbose = 1;
		usage();  exit(-1);
	}

	if (strcmp(argv[noArg], "-v") ==  0) {
		glbVerbose = 1;
		argc--; noArg++;
	}

	if (argc < 2) {
		FPRINTF(stderr,"ERR: Please define at least 2 parameters !\n");
		usage();  exit(-1);
	}


	unsigned char tag_size = atoi(argv[noArg]);
	argc--; noArg++;
	if ((tag_size == 0) || (tag_size > 5)) {
		FPRINTF(stderr,"ERR: Bad tag size. Must be from 1 to 5 !\n");
		usage();
		exit(-2);
	}
	tag.bits.sz = tag_size;

	// Initialise batch_report processor
	br_init(); // Empty all buffers

	// Init data structures according to parameters
	unsigned char nbSeries = 0;

	while (argc > 1) {
		if (nbSeries > MAX_NB_SERIES) {
			FPRINTF(stderr,"ERR: To many series (max %d) !\n",MAX_NB_SERIES);
			usage();  exit(-1);
		}
		FPRINTF(stderr, "Series (  SXX: lbl,resol,delta,type): \n ");
		sscanf(argv[noArg], "%d,%f,%f,%d", &tag_lbl, &resol_float,&delta_float, &stInt);

		// Sample_type
		st = (bm_sample_type_t)stInt;
		// Data tag
		tag.bits.lbl = tag_lbl;

		FPRINTF(stderr, "  S%02d: %d,%f,%f,%s\n", nbSeries, tag_lbl, resol_float,delta_float,BM_ST2STR(st));

		br_serie_init(&series[nbSeries], &params[nbSeries]);
		br_serie_set_and_add(&series[nbSeries], st,
			getSampleFromFloat(resol_float, st),
			getSampleFromFloat(delta_float, st),
			tag,0,0);

		argc--; nbSeries++;  noArg++;
	}

	FPRINTF(stderr, "\nPlease, input one data per line \"timestamp\\tlbl\\tvalue \\n\" \n ");
	FPRINTF(stderr, "and terminate with <ctrl>D :\n");

	// Deltas calculation, size evaluation and serialization for "time,sample" inputs
	char str[255];
	unsigned int no_line=0;
	signed char batch_counter; unsigned long timestamp;
	while ( (fgets( str, 255, stdin)!=NULL) && (ret!=BR_NEW_CBUF_AVAILABLE)) {
	    if (no_line == 0){
	        sscanf(str, "%ld",&batch_counter);

	    } else if (no_line == 1){
	        sscanf(str, "%ld",&timestamp);
	    } else if (sscanf(str, "%ld\t%d\t%f",&tmpTimeStamp.u32,&tag_lbl,&tmpFloat) == EOF) {
			break;
		} else {
			tag.bits.lbl = tag_lbl;
			if ((sPtr = get_serie_from_tag(tag)) == NULL) {
			//if ((sPtr = NULL) == NULL) {
				FPRINTF(stderr,"ERR: Sample nb %d has not a valid label (%d) !\nAborting !\n\n",i, tag_lbl);
				exit(-1);
			};
			tmpSample = getSampleFromFloat(tmpFloat, sPtr->par->type);
			FPRINTF(stderr,"\nDBG: Input: %ld\t%d\t%f\n", tmpTimeStamp.u32,sPtr->par->type,BM_S2FL(tmpSample,sPtr->par->type) );
			ret=br_serie_evaluate_sample(sPtr,tmpSample,tmpTimeStamp,BR_A_CHECK_CHANGE);
		}
	    no_line++;
	}

	if(ret!=BR_NEW_CBUF_AVAILABLE){
		bm_sample_t dummy;
		dummy.u32 = 0;
		FPRINTF(stderr,"BR_A_FORCE_REPORT\n");
	    current_buf_counter = batch_counter; // pour gérer le batch counter
		// Not NULL is set to ignore sample but only fore the report
		br_serie_evaluate_sample(NULL,dummy,dummy,BR_A_FORCE_REPORT);
	}
	else
	{
		FPRINTF(stderr, "\nBR_NEW_CBUF_AVAILABLE\n");
	}



	//uncompress
	unsigned char buftosend[250];
	unsigned char lnb_bytes;

	br_cpy_compressed_buf(NEWER_COMPRESSED_BUF, buftosend,&lnb_bytes,timestamp);

	FPRINTF(stdout,"\nRESULT: ");
	for (i=0; i<lnb_bytes;i++) {
		fprintf(stdout,"%02x", buftosend[i]);
	}
	fprintf(stdout,"\n");

/* 	int filedesc = open("CompressedFile", O_TRUNC|O_WRONLY );
	//write(filedesc,buftosend, lnb_bytes);
	write(filedesc,buftosend, lnb_bytes);
	close(filedesc);

	// Show binary view of compressed frame
	br_processingbuf_bin_dump(0,0,lnb_bytes*8);
 */
//	br_uncompress_bin(buftosend,lnb_bytes*8,tag.bits.sz,&br_uncompress_series);
////
//	br_uncompress_serie_dump(&br_uncompress_series);

	exit(0);
}
