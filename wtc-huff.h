#ifndef _ZCD_HUFF_H_
#define _ZCD_HUFF_H_


// HUFFMAN TABLE *****************************************************
// structure for Huffman Table
typedef struct {
	unsigned char	sz;		//	size of the label
	unsigned short	lbl;	//	label
} br_huffman_t;


#define BR_HUFF_MAX_INDEX_TABLE			14
#define BR_HUFF_SIZE_LBL_OF_MAX_INDEX	11

#define NB_HUFF_ELEMENT	16
extern const br_huffman_t huff[3][NB_HUFF_ELEMENT];

#endif // _ZCD_HUFF_H_
