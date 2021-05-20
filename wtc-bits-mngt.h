#ifndef _WTC_BITS_MNGT_H_
#define _WTC_BITS_MNGT_H_
/*
#include <sys/types.h>
*/
#include <stdio.h>      /* PRINTF */
#include <math.h>

#ifndef CONTIKI
#include <stdint.h>
#endif

#ifdef CONTIKI
#define DEBUG 0
#include "wtc-debug-print-macros.h"
#else
//#define PRINTF(...)
//#define DEBUG 0
#define PRINTF(...) do { if (glbVerbose) printf(__VA_ARGS__); } while (0)
#define FPRINTF(...) do { if (glbVerbose) fprintf(__VA_ARGS__); } while (0)
#endif

#ifndef truncf
#define truncf(a) (ceil(a))
#endif

extern unsigned char glbVerbose;

// SAMPLE TYPES --------------------------------------------------
typedef enum {
	ST_UNDEF = 0,
	ST_BL, // boolean
	ST_U4,
	ST_I4,
	ST_U8,
	ST_I8,
	ST_U16,
	ST_I16,
	ST_U24,
	ST_I24,
	ST_U32,
	ST_I32,
	ST_FL
} bm_sample_type_t;
#define BM_ST_SZ(st) (st > ST_I24 ? 32 : (st > ST_I16 ? 24 : (st > ST_I8 ? 16 : (st > ST_I4 ? 8 : (st > ST_BL ? 4 :(st > ST_UNDEF ? 1 : 0))))))
#define BM_ST2STR(st) (\
	(st == ST_BL  ? "BL " : \
	(st == ST_U4  ? "U4 " : st == ST_I4  ? "I4 "  : \
	(st == ST_U8  ? "U8 " : st == ST_I8  ? "I8 "  : \
	(st == ST_U16 ? "U16" : st == ST_I16 ? "I16" : \
	(st == ST_U24 ? "U24" : st == ST_I24 ? "I24" : \
	(st == ST_U32 ? "U32" : st == ST_I32 ? "I32" : \
	(st == ST_FL  ? "FL " : "???"))))))))


typedef union {
		uint8_t u8;
		int8_t i8;
		uint16_t u16;
		int16_t i16;
		uint32_t u32;
		int32_t i32;
		float fl;
} bm_sample_t;

#define BM_PTS_FROM_ST(s,st) (uint8_t *)\
	(st == ST_BL  ? &(s.u8) : \
	(st == ST_U4  ? &(s.u8)  : 					st == ST_I4  ? (unsigned char *)&(s.i8)  : \
	(st == ST_U8  ? &(s.u8)  : 					st == ST_I8  ? (unsigned char *)&(s.i8)  : \
	(st == ST_U16 ? (unsigned char *)&(s.u16) : st == ST_I16 ? (unsigned char *)&(s.i16) : \
	(st == ST_U24 ? (unsigned char *)&(s.u32) : st == ST_I24 ? (unsigned char *)&(s.i32) : \
	(st == ST_U32 ? (unsigned char *)&(s.u32) : st == ST_I32 ? (unsigned char *)&(s.i32) : \
	(st == ST_FL  ? (unsigned char *)&(s.fl)  : NULL)))))))

#define BM_S2FL(s,st) ((float)\
	(st == ST_BL  ? s.u8 : \
	(st == ST_U8  ? s.u8  : st == ST_I8  ? s.i8  : \
	(st == ST_U4  ? s.u8  : st == ST_I4  ? s.i8  : \
	(st == ST_U16 ? s.u16 : st == ST_I16 ? s.i16 : \
	(st == ST_U24 ? s.u32 : st == ST_I24 ? s.i32 : \
	(st == ST_U32 ? s.u32 : st == ST_I32 ? s.i32 : \
	(st == ST_FL  ? s.fl  : 0))))))))
#define BM_T2U32(t,tt) ((uint32_t)\
	(tt == ST_BL  ? t.u8  : \
	(tt == ST_U4  ? t.u8  : \
	(tt == ST_U8  ? t.u8  : \
	(tt == ST_U16 ? t.u16 : \
	(tt == ST_U24 ? t.u32 : \
	(tt == ST_U32 ? t.u32 : 0xFFFFFFFF)))))))

inline static float bitsMngtRound(float number)
{
	// NOTE: Using double either than float SHOULD be beter not to loose precision after 24 bits
	// ie:
	// - Integer bigger than 2^24 + 1 (16,777,217) can't be precisely represented with FLOAT
	// - Integer bigger than 2^53 -1  (9,007,199,254,740,993) can't be precisely represented with DOUBLE
	//
	//BUT this choice of double SHOULD be used even outside of this fonction every wher in compression !!!!
	//SO it's not done for now !
	//

	// Big float are always integers
	if (number < -2147483648.0) return number;
	if (number >  2147483647.0) return number;

	// Else find the best rounding to int
    return (float)(number >= 0 ? (int32_t)(number + 0.5) : (int32_t)(number - 0.5));
}
	
inline static unsigned char nbbitsset(bm_sample_t s, bm_sample_type_t st) {
	unsigned char i,n=0;
	for (i=0; i<BM_ST_SZ(st); i++) {
		n += (s.u32 & 0x01 ? 1 : 0);
		s.u32 >>= 1;
	}
	return(n);
}

inline static int
bits_setb(uint8_t *vec, size_t size, uint16_t bit) {
   if (size <= (bit >> 3))
     return -1;

  *(vec + (bit >> 3)) |= (uint8_t)(1 << (bit & 0x07));
   return 1;
}

inline static int
bits_clrb(uint8_t *vec, size_t size, uint16_t bit) {
   if (size <= (bit >> 3))
     return -1;


  *(vec + (bit >> 3)) &= (uint8_t)(~(1 << (bit & 0x07)));
   return 1;
}

inline static int
 bits_getb(const uint8_t *vec, size_t size, uint16_t bit) {
   if (size <= (bit >> 3))
     return -1;


  return (*(vec + (bit >> 3)) & (1 << (bit & 0x07))) != 0;
}


inline static int
bits_cpy(	uint8_t *src,	uint16_t src_bit_start,
 			uint8_t *dest,	uint16_t dest_bit_start, uint16_t dest_sz,
 			uint16_t nb_bits) {

   if (dest_sz < (dest_bit_start + nb_bits)) return -1;

   while (nb_bits > 0) {

   		if (*(src + (src_bit_start >> 3)) & (1 << (src_bit_start & 0x07)))
   			*(dest + (dest_bit_start >> 3)) |= (uint8_t)(1 << (dest_bit_start & 0x07));
		else
			*(dest + (dest_bit_start >> 3)) &= (uint8_t)(~(1 << (dest_bit_start & 0x07)));

		src_bit_start++; dest_bit_start++; nb_bits--;
   }

   return(1);

}

inline static bm_sample_t
inv_byte_align (bm_sample_t s,bm_sample_type_t st) {

 	if (st == ST_U16) {
 		s.u16 = ((s.u16>>8) & 0x00FF) | ((s.u16<<8) & 0xFF00);

 	} else if (st == ST_I16) {
 		s.i16 = ((s.i16>>8) & 0x00FF) | ((s.i16<<8) & 0xFF00);

 	} else if ((st == ST_U32) || (st == ST_U24)) {
		s.u32 = ((s.u32>>24) & 0x000000FF) | ((s.u32>>8) & 0x0000FF00) | ((s.u32<<8) & 0x00FF0000) | ((s.u32<<24) & 0xFF000000);

 	} else if ((st == ST_I32) || (st == ST_I24)) {
		s.i32 = ((s.i32>>24) & 0x000000FF) | ((s.i32>>8) & 0x0000FF00) | ((s.i32<<8) & 0x00FF0000) | ((s.i32<<24) & 0xFF000000);

 	} else {
 		// Do nothing for other cases U8 and (FLOAT ?)
 	}

 	return s;

}


inline static int
bits_sample2buf(bm_sample_t s,bm_sample_type_t st,
 uint8_t *dest, uint16_t dest_bit_start, uint16_t dest_sz,
  uint16_t nb_bits) {
  // Allways copy least sigificant bist of sample:
  // Do not use with floats !!
 PRINTF("type:%d sample:%lx nb_bits: %d dest_bit_start:%d ",st,(uint32_t)BM_S2FL(s,st),nb_bits,dest_bit_start);
 if ((st == ST_FL) && (nb_bits != 32)) {return -1;} // Accept only FULL float copy
 if (dest_sz < (dest_bit_start + nb_bits)) {return -1;} // Verify that dest buf is large enough

  uint32_t u32 = s.u32;
  PRINTF("%lx ",(uint32_t)BM_S2FL(s,st));

  unsigned char nbytes    = (nb_bits-1)/8+1;
  unsigned char nbitsfrombyte = nb_bits%8;
  unsigned char bittoread = 0;
  if ((nbitsfrombyte == 0) && (nbytes > 0)) nbitsfrombyte = 8;

	while (nbytes >0) {
		bittoread = 0;
		while (nbitsfrombyte > 0) {
			if (u32 & (((uint32_t)1) << (((nbytes-1) * 8 ) + bittoread))){
				*(dest + (dest_bit_start >> 3)) |= (uint8_t)(1 << (dest_bit_start & 0x07));
				PRINTF("1");
			}
			else{
				*(dest + (dest_bit_start >> 3)) &= (uint8_t)(~(1 << (dest_bit_start & 0x07)));
				PRINTF("0");
			}
			nbitsfrombyte--; bittoread++;dest_bit_start++;
		}
		nbytes--;
		nbitsfrombyte = 8;
	}
	PRINTF("\n");
   return(1);
};

inline static int
bits_buf2sample(bm_sample_t *s, bm_sample_type_t st,
 uint8_t *src, uint16_t src_bit_start, uint16_t src_sz,
  uint16_t nb_bits) {
  // Allways copy least sigificant bist of sample:
  // Do not use with floats !!

 if ((st == ST_FL) && (nb_bits != 32)) {return -1;} // Accept only FULL float copy
 if (src_sz < (src_bit_start + nb_bits)) {return -1;} // Verify that dest buf is large enough

  uint32_t u32 = 0;

  unsigned char nbytes    = (nb_bits-1)/8+1;
    unsigned char nbitsfrombyte = nb_bits%8;
    unsigned char bittoread = 0;

    if ((nbitsfrombyte == 0) && (nbytes > 0)) nbitsfrombyte = 8;

  	while (nbytes >0) {
  		bittoread = 0;
		while (nbitsfrombyte > 0) {
			if (*(src + (src_bit_start >> 3)) & (1 << (src_bit_start & 0x07)))
				u32 |= (((uint32_t)1) << (((nbytes-1) * 8 ) + bittoread));
			nbitsfrombyte--; bittoread++;src_bit_start++;
		}
  		nbytes--;
  		nbitsfrombyte = 8;
  	}

 // Propagate the sign bit if 1
 if (((st == ST_I4) || (st == ST_I24)) && (u32 & (((uint32_t)1) << (nb_bits-1)))) {
  unsigned char i;
  for (i = nb_bits; i < 32; i++) {
   u32 |= (((uint32_t)1) << i); nb_bits++;
  }
 }

 s->u32 = u32;

   return(1);
};


inline static int
bits_HuffPattern2buf(uint16_t pattern,
 uint8_t *dest, uint16_t dest_bit_start, uint16_t dest_sz,
  uint16_t nb_bits) {
	PRINTF("HUFF2buff: sample:%x nb_bits: %d dest_bit_start:%d ",pattern,nb_bits,dest_bit_start);

	if (dest_sz < (dest_bit_start + nb_bits)) {return -1;} // Verify that dest buf is large enough
	uint16_t bittoread=0,sz=nb_bits-1;

	while (nb_bits > 0) {
		if (pattern & (((uint32_t)1) << (sz-bittoread))){
			*(dest + (dest_bit_start >> 3)) |= (uint8_t)(1 << (dest_bit_start & 0x07));
			PRINTF("1");
		}
		else{
			*(dest + (dest_bit_start >> 3)) &= (uint8_t)(~(1 << (dest_bit_start & 0x07)));
			PRINTF("0");
		}
		nb_bits--; bittoread++;dest_bit_start++;
	}

	PRINTF("\n");
   return(1);
};


inline static int
bits_buf2HuffPattern(uint16_t *pattern,
 uint8_t *src, uint16_t src_bit_start, uint16_t src_sz,
  uint16_t nb_bits)
{
	uint16_t sz=nb_bits-1;
	if (src_sz < (src_bit_start + nb_bits)) {return -1;} // Verify that dest buf is large enough
	uint16_t bittoread=0;
	*pattern=0;
	while (nb_bits > 0) {
		if (*(src + (src_bit_start >> 3)) & (1 << (src_bit_start & 0x07)))
			*pattern |= (1 << (sz-bittoread));
		nb_bits--; bittoread++;src_bit_start++;
	}

   return(1);
};

// To be carefull if abs(t)>abs(newt)
inline static bm_sample_t
change_representation(bm_sample_t s,bm_sample_type_t t,bm_sample_type_t newt ) {
	bm_sample_t ls;
	if(newt==t) return s;
	if ((newt==ST_U32)&&(t==ST_U24))return s;

	ls.fl=BM_S2FL(s,t);
	if (newt != ST_FL){
		if(ls.fl>=0)
			ls.u32=(uint32_t)BM_S2FL(s,t);
		else{
			ls.i32=(int32_t)BM_S2FL(s,t);
		}
	}
	return ls;
}

inline static bm_sample_type_t
get_u32_minimal_representation(uint32_t du32,bm_sample_t *s) {

	if (du32 < 2) {
		s->u8 = du32; return(ST_BL);

	} else if (du32 < 16) {
		s->u8 = du32; return(ST_U4);

	} else if (du32 < 256.0) {
		s->u8 = du32; return(ST_U8);

	} else if (du32 < 65536.0) {
		s->u16 = du32; return(ST_U16);

	} else if (du32 < 16777216.0) {
		s->u32 = du32; return(ST_U24);

	} else {
		s->u32 = du32; return(ST_U32);
	}
}

inline static bm_sample_type_t
get_i32_minimal_representation(int32_t di32,bm_sample_t *s) {

	if (di32>=0)
		return(get_u32_minimal_representation((uint32_t)di32, s));

	if (di32 < -8388607) {
		s->i32 = di32; return(ST_I32);

	} else if (di32 < -32767) {
		s->i32 = di32; return(ST_I24);

	} else if (di32 < -127) {
		s->i16 = di32; return(ST_I16);

	} else if (di32 < -7) {
		s->i8 = di32; return(ST_I8);

	} else {
		s->i8 = di32; return(ST_I4);
	}

}

inline static bm_sample_type_t
get_fl_minimal_representation(float df, float precision,bm_sample_t *s) {
	// TODO: Improve the interval determination speed (Dichotomie or other mean ...)

	if (df <0) {
		int32_t di32 = (int32_t)ceil(df);

		if ((df < -2147483648.0) || (((float)di32 - df) > precision)) {
			s->fl = df; return(ST_FL);

		} else
			return (get_i32_minimal_representation(di32, s));

	} else {
		uint32_t du32 = (uint32_t)floor(df);
		if ((df > (uint32_t)(4294967295.0)) || ((df - (float)du32) > precision)){
			s->fl = df; return(ST_FL);

		} else
			return(get_u32_minimal_representation(du32, s));
	}
}

#endif /* _WTC_BITS_MNGT_H_ */
