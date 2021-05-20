// Linux/Cygwin: gcc -Wno-char-subscripts -fshort-enums br_uncompress.c wtc-batch-uncompress.c  wtc-huff.c -o br_uncompress
// Pour windows: mingw32-gcc -Wno-char-subscripts -fshort-enums br_uncompress.c wtc-batch-uncompress.c  wtc-huff.c -o br_uncompress

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wtc-batch-uncompress.h"

#define ISHEX(c) ((c>='0' && c<='9') || (c>='a' && c<='f')  || (c>='A' && c<='F'))
#define HEX2BYTE(c) ((c>='0' && c<='9') ? c - '0' : 10 + ((c>='a' && c<='f') ? c - 'a' : c -'A'))


// USAGE:
// br_uncompress tagsz "taglbl,resol,sampletype" "..." ... < buf > result

#define MAX_NAME_SIZE 32

unsigned char br_uncompress_bin(unsigned char *buf, unsigned short buf_sz,unsigned char tag_sz,br_uncompress_series_t *br_uncompress_series);

void usage() {
  fprintf(stderr,"USAGE:\n");
  fprintf(stderr,"  br_uncompress [-h | -v] [-a] tagsz \"taglbl,resol,sampletype\" \"...\" ... < buf > result \n");
	fprintf(stderr,"  -a : Input buf must be considered as ascii hexa bytes either than usual raw bytes: 'hhhhhh...' or 'hh hh hh...' or '$HH$HH$HH...'\n");
	fprintf(stderr,"      Allow following usages: \n");
	fprintf(stderr,"      echo '$10$27$00$80$03$93$20$18$00$80$10$81$83$07$0d$45$85$10$05' | ./br_uncompress -a  3 2,1.0,12\n");
	fprintf(stderr,"      or \n");
	fprintf(stderr,"      echo \"404780800a5800000442ca8a4048fd395c817e21cb9a40028fd5379de3768b4f816e75a6e376006e2d800066\" | ./br_uncompress -a  3 2,10,9 1,10,7 4,30,10 3,10,4 5,10,6 6,1,4\n");
	fprintf(stderr,"      \nWhere SampleType can be:\n");
	fprintf(stderr,"       1: Boolean\n");
	fprintf(stderr,"       2: Unsigned int U4    3: Signed int I4 \n");
	fprintf(stderr,"       4: Unsigned int U8    5: Signed int I8 \n");
	fprintf(stderr,"       6: Unsigned int U16   7: Signed int I16\n");
	fprintf(stderr,"       8: Unsigned int U24   9: Signed int I24\n");
	fprintf(stderr,"      10: Unsigned int U32  11: Signed int I32\n");
	fprintf(stderr,"      12: Float\n");


}

unsigned char glbVerbose = 0;

int main(int argc, char *argv[]) {
	//bm_sample_t v,t,res, ss;
	bm_sample_t res;
	bm_sample_type_t st;
	int st_int;
	unsigned char withasciidata=0;
	unsigned char noArg = 1;
	unsigned char nbSeries = 0;

	br_uncompress_series_t br_uncompress_series;

	memset(&br_uncompress_series,0,sizeof(br_uncompress_series_t));

	int tag_lbl;
	float resol_float;

	if (argc < 2) {
		usage();  exit(0);
	}

	if (strcmp(argv[1], "-h") ==  0) {
		usage();  exit(0);
	}

	if (strcmp(argv[noArg], "-v") ==  0) {
		glbVerbose = 1;
		argc--;	noArg++;
	}

	if (argc < 3) {
		usage();  exit(-1);
	}

	if (strcmp(argv[noArg], "-a")==0) {
		withasciidata=1;
		argc--;	noArg++;
	}

	unsigned char tag_size = atoi(argv[noArg]);
	argc--;	noArg++;
	if ((tag_size == 0) || (tag_size > 5)) {
		usage();
		exit(-2);
	}

	// Init data structures according to desired types
	while (argc > 1) {
PRINTF("Scan args:\n ");
		sscanf(argv[noArg], "%d,%f,%d", &tag_lbl, &resol_float, &st_int);
PRINTF("S%02d: %d,%f,%d\n", nbSeries, tag_lbl, resol_float, st_int);
		st = (bm_sample_type_t)st_int;

		br_uncompress_series.br_uncompress_serie[nbSeries].tag.bits.sz = tag_size;
		br_uncompress_series.br_uncompress_serie[nbSeries].tag.bits.lbl= tag_lbl;
		// TODO: Manage any type for resolution, not only U16 ???
		if(st==ST_FL){
			br_uncompress_series.br_uncompress_serie[nbSeries].resolution.fl=resol_float;
		} else {
			res.u32 = (unsigned long)resol_float;
			br_uncompress_series.br_uncompress_serie[nbSeries].resolution.u32=res.u32;
		}

		br_uncompress_series.br_uncompress_serie[nbSeries].tv.bits.type=st;

		noArg++; argc--;nbSeries++;
	}

	// Read all stdin until EOF or error
	unsigned short buf_size = 0;
	unsigned char n,p,i, buff[1000];
	while ((n = read(0, buff, BUFSIZ)) > 0) buf_size += n;

	// convert hexadecimal text input "hhxxhhxx" to binary if required
	if (withasciidata==1) {
		unsigned char hd, ld;
		p=0;
		for (i = 0; i<buf_size;) {
			if (ISHEX(buff[i])) {
				hd = buff[i]; ld = buff[i+1];
				buff[p] = (HEX2BYTE(hd) << 4) + HEX2BYTE(ld);
				i+=2; p++;
			} else i+=1;
		}
		buf_size=p;
	}

	buf_size *= 8;
	br_uncompress_bin(buff,buf_size,tag_size,&br_uncompress_series);

	// Dump result on stdout
	br_uncompress_serie_dump(&br_uncompress_series);
	//sleep(3);

	return(0);
}
