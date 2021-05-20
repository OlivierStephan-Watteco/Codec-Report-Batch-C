#include "wtc-bits-mngt.h"
#include "wtc-huff.h"
#include "wtc-batch-uncompress.h"

signed char get_index_uncompress_serie_from_tag(tag_t atag,br_uncompress_serie_t *br_uncompress_serie)
{
	signed char i;
	for(i=0;i<NUMBER_OF_SERIES;i++)
	{
		if((br_uncompress_serie[i].tag.bits.lbl==atag.bits.lbl)&&(br_uncompress_serie[i].tag.bits.sz==atag.bits.sz))	{ return i;}
	}
	return -1;
}

//return the lenght of huffman code
unsigned char get_bi_from_Hi(unsigned char *huff_buf,unsigned short huff_buff_bit_size,unsigned short aindex,unsigned char huff_coding,unsigned char *bi)
{
	unsigned char i,j;
	bm_sample_t lhuff;

	for(i=2;i<12;i++){
		bits_buf2HuffPattern(&(lhuff.u16), huff_buf,aindex, huff_buff_bit_size,i);
		for(j=0;j<NB_HUFF_ELEMENT;j++){
			if((huff[huff_coding][j].sz==i)&&(lhuff.u16==huff[huff_coding][j].lbl)) { *bi=j; return i;}
		}
	}
	return 0;
}

void br_uncompress_serie_dump(br_uncompress_series_t *br_uncompress_series)
{
	unsigned char i,j;
	br_uncompress_serie_t *br_uncompress_serie=br_uncompress_series->br_uncompress_serie;
	PRINTF("\nUNCOMPRESSED SERIE\n");
	if(br_uncompress_series->batch_req) PRINTF("requested batch\n");
	PRINTF("cnt: %u\n",br_uncompress_series->counter);
	PRINTF("%lu\n",br_uncompress_series->timestamp);
	for(i=0;i<NUMBER_OF_SERIES;i++)
	{
		if(br_uncompress_serie[i].uncompress_samples_nb!=0){
			for(j=0;j<br_uncompress_serie[i].uncompress_samples_nb;j++)
			{
				if(br_uncompress_serie[i].tv.bits.type!=ST_FL){
					if(br_uncompress_serie[i].tv.bits.type!=ST_U32){
						if (BM_S2FL(br_uncompress_serie[i].uncompress_samples[j].v,br_uncompress_serie[i].tv.bits.type)<0)
							printf("%ld %d %ld\n",br_uncompress_serie[i].uncompress_samples[j].t.u32,br_uncompress_serie[i].tag.bits.lbl,(signed long)BM_S2FL(br_uncompress_serie[i].uncompress_samples[j].v,br_uncompress_serie[i].tv.bits.type));
						else
							printf("%ld %d %ld\n",br_uncompress_serie[i].uncompress_samples[j].t.u32,br_uncompress_serie[i].tag.bits.lbl,(unsigned long)BM_S2FL(br_uncompress_serie[i].uncompress_samples[j].v,br_uncompress_serie[i].tv.bits.type));
					}
					else
						printf("%ld %d %lu\n",br_uncompress_serie[i].uncompress_samples[j].t.u32,br_uncompress_serie[i].tag.bits.lbl,br_uncompress_serie[i].uncompress_samples[j].v.u32);
				}else{
					float tmpFl = br_uncompress_serie[i].uncompress_samples[j].v.fl;

					// Below prepare to make correct rounding for displayed data
					char fmt[10];
					unsigned short decimals =  (unsigned short)bitsMngtRound(log10(1/br_uncompress_serie[i].resolution.fl));
					float precision = pow(10,decimals);
					tmpFl = bitsMngtRound(tmpFl * precision) / precision;
					sprintf(fmt,"%%d %%d %%.%df\n", decimals );

					// Display data
					printf(fmt,
						br_uncompress_serie[i].uncompress_samples[j].t.u32,
						br_uncompress_serie[i].tag.bits.lbl,
						tmpFl);
				}

			}
			PRINTF("\n");
		}
	}
}

unsigned char br_uncompress_bin(unsigned char *buf, unsigned short buf_sz,unsigned char tag_sz,br_uncompress_series_t *br_uncompress_series){
	unsigned short index=0;
	unsigned short i,j;
	unsigned char bi,sz;
	signed char ii;
	signed char index_of_the_first_sample;
	unsigned long abs_timestamp=0,last_timestamp=0;
	flag lflag;
	tag_t ltag;
	bm_sample_t ltemp;
	bits_buf2sample((bm_sample_t *)&lflag, ST_U8,buf,index, buf_sz,8);
	index+=8;

	br_uncompress_serie_t *br_uncompress_serie=br_uncompress_series->br_uncompress_serie;

	PRINTF("nb_of_type_measure: %d\n",lflag.bits.nb_of_type_measure);
	PRINTF("batch requested: %d\n",lflag.bits.batch_req);
	PRINTF("no sample: %d\n",lflag.bits.no_sample);
	br_uncompress_series->batch_req=lflag.bits.batch_req;
	if(!lflag.bits.no_sample)
		PRINTF("cts: %d\n",lflag.bits.cts);

	//counter
	bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,3);
	index+=3;
	br_uncompress_series->counter=ltemp.u8;
	PRINTF("cnt: %d\n",br_uncompress_series->counter);

	//bit reserved
	bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,1);
	index+=1;

	for(i=0;i<lflag.bits.nb_of_type_measure;i++)
	{
		//Tag
		bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,tag_sz);
		index+=tag_sz;
		ltag.bits.sz=tag_sz;ltag.bits.lbl=ltemp.u8;
		PRINTF(" tag: %d,",ltag.bits.lbl);

		if((ii=get_index_uncompress_serie_from_tag(ltag,br_uncompress_serie))<0) return 0;
		PRINTF(" index %d ",ii);

		//timestamp
		if(i==0){
			index_of_the_first_sample=ii; //keep index of the first sample for the common timestamp uncompressing
			bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[0].t, ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
			index+=BM_ST_SZ(ST_U32);
			PRINTF(" timestamp: %ld",br_uncompress_serie[ii].uncompress_samples[0].t.u32);
			abs_timestamp=br_uncompress_serie[ii].uncompress_samples[0].t.u32;
		}else{
			//Delta Timestamp
			sz=get_bi_from_Hi(buf,buf_sz,index,1,&bi);
			PRINTF(" bi: %d sz: %d",bi, sz);
			if (!sz) return 0;
			index+=sz;
			if(bi<=BR_HUFF_MAX_INDEX_TABLE)
			{
				if(bi>0){
					bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[0].t, ST_U32,buf,index, buf_sz,bi);
					index+=bi;
					PRINTF(" raw: %ld ",br_uncompress_serie[ii].uncompress_samples[0].t.u32);
					br_uncompress_serie[ii].uncompress_samples[0].t.u32+=abs_timestamp+pow(2,bi)-1;
				}else{
					br_uncompress_serie[ii].uncompress_samples[0].t.u32=abs_timestamp;
				}
			}
			else{
				bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[0].t, ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
				index+=BM_ST_SZ(ST_U32);
			}
			abs_timestamp=br_uncompress_serie[ii].uncompress_samples[0].t.u32;
			PRINTF (" timestamp: %ld",abs_timestamp);
		}
		last_timestamp=abs_timestamp;

		//Measure
		bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[0].v, br_uncompress_serie[ii].tv.bits.type,buf,index, buf_sz,BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type));
		index+=BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type);

		if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
			if(BM_S2FL(br_uncompress_serie[ii].uncompress_samples[0].v,br_uncompress_serie[ii].tv.bits.type)<0)
				PRINTF(" Measure: %ld",(signed long)BM_S2FL(br_uncompress_serie[ii].uncompress_samples[0].v,br_uncompress_serie[ii].tv.bits.type));
			else
				PRINTF(" Measure: %ld",(unsigned long)BM_S2FL(br_uncompress_serie[ii].uncompress_samples[0].v,br_uncompress_serie[ii].tv.bits.type));
		}else{
			PRINTF(" Measure: %f",br_uncompress_serie[ii].uncompress_samples[0].v.fl);
		}
		br_uncompress_serie[ii].uncompress_samples_nb=1;

		if(!lflag.bits.no_sample){
			//Coding type
			bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,2);
			index+=2;
			br_uncompress_serie[ii].coding_type=ltemp.u8;

			bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,2);
			index+=2;
			br_uncompress_serie[ii].coding_table=ltemp.u8;
			PRINTF(" Coding type: %d, Coding table %d ",br_uncompress_serie[ii].coding_type,br_uncompress_serie[ii].coding_table);
		}
		PRINTF("\n");
	}

	if(!lflag.bits.no_sample){
		if(lflag.bits.cts){//common time stamp
			PRINTF("common time stamp\n");
			//number of sample
			bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,8);
			unsigned char nb_sample_to_parse=ltemp.u8;
			index+=8;
			PRINTF(" number of sample: %d\n",nb_sample_to_parse);

			//TimeStamp Coding
			unsigned char ltimestamp_coding;
			bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,2);
			ltimestamp_coding=ltemp.u8;
			index+=2;
			PRINTF(" TimeStamp Coding(0-A/1-B/2-C): %d\n",ltimestamp_coding);

			bm_sample_t TimeStampCommon[250]; // PEG:  Arbitrary for compilation on C99 standard C
			// bm_sample_t TimeStampCommon[nb_sample_to_parse];

			for (i=0;i<nb_sample_to_parse;i++){
				//Delta Timestamp
				sz=get_bi_from_Hi(buf,buf_sz,index,ltimestamp_coding,&bi);
				if (!sz) return 0;
				PRINTF ("bi: %d sz: %d",bi,sz);
				index+=sz;
				if(bi<=BR_HUFF_MAX_INDEX_TABLE)
				{
					if(i==0){
						TimeStampCommon[i].u32=br_uncompress_serie[index_of_the_first_sample].uncompress_samples[0].t.u32;
					}else
					{
						if (bi>0){
							bits_buf2sample(&TimeStampCommon[i], ST_U32,buf,index, buf_sz,bi);
							index+=bi;
							PRINTF(" raw: %ld ",TimeStampCommon[i].u32);
							TimeStampCommon[i].u32+=TimeStampCommon[i-1].u32+pow(2,bi)-1;
						}else
							TimeStampCommon[i].u32=TimeStampCommon[i-1].u32;
					}
				}
				else{
					bits_buf2sample(&TimeStampCommon[i], ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
					index+=BM_ST_SZ(ST_U32);
				}
				last_timestamp=TimeStampCommon[i].u32;
				PRINTF(" timestamp: %ld \n",TimeStampCommon[i].u32);
			}


			for(j=0;j<lflag.bits.nb_of_type_measure;j++){
				unsigned char first_null_delta_value=1;
				//Tag
				bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,tag_sz);
				index+=tag_sz;

				ltag.bits.lbl=ltemp.u8;
				PRINTF(" tag: %d",ltag.bits.lbl);

				if((ii=get_index_uncompress_serie_from_tag(ltag,br_uncompress_serie))<0) return 0;
				PRINTF(" index: %d\n",ii);
				for (i=0;i<nb_sample_to_parse;i++){
					//Available bit
					bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,1);
					index+=1;
					PRINTF ("%d. available: %d ",i,ltemp.u8);
					if(ltemp.u8)//available bit
					{
						PRINTF ("coding table: %d ",br_uncompress_serie[ii].coding_table);
						//Delta Value
						sz=get_bi_from_Hi(buf,buf_sz,index,br_uncompress_serie[ii].coding_table,&bi);
						if (!sz) return 0;
						index+=sz;
						PRINTF ("bi: %d sz: %d",bi,sz);
						//if (!bi) PRINTF ("\n");
						if(bi<=BR_HUFF_MAX_INDEX_TABLE)
						{
							if(bi>0){
								bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v, ST_U16,buf,index, buf_sz,bi);
								index+=bi;
								PRINTF(" RawValue: %d ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16);
								if(br_uncompress_serie[ii].coding_type==0)//ADLC
								{
									if(br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16>=pow(2,bi-1))
									{
										if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=((unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16)*br_uncompress_serie[ii].resolution.u32;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
										}else{
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=((float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16)*br_uncompress_serie[ii].resolution.fl;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
										}
									}
									else{
										if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+1-(unsigned long)pow(2,bi);
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
										}else{
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+1.0-(float)pow(2,bi);
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
										}
									}
								}
								else if(br_uncompress_serie[ii].coding_type==1) //Positive
								{
									if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+pow(2,bi)-1;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
									}else{
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+(float)pow(2,bi)-1.0;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
									}
								}
								else{		//Negative
									if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+pow(2,bi)-1;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32-br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32;
									}else{
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+(float)pow(2,bi)-1.0;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl-br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl;
									}
								}
								if(br_uncompress_serie[ii].tv.bits.type!=ST_FL)
									PRINTF(" Value: %ld ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
								else
									PRINTF(" Value: %f ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
							}else{
								if(first_null_delta_value){ // First value is yet recorded starting from the header
									PRINTF("\n");
									first_null_delta_value=0;
									continue;
								}
								else{
									if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
										PRINTF(" Value: %ld ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
									}else{
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
										PRINTF(" Value: %f ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
									}
								}
							}
						}
						else{
							bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v, br_uncompress_serie[ii].tv.bits.type,buf,index, buf_sz,BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type));
							index+=BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type);
							if(br_uncompress_serie[ii].tv.bits.type!=ST_FL)
								PRINTF(" Complete: %ld ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
							else
								PRINTF(" Complete: %f ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
						}
						//add the timestamp to the value
						br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32=TimeStampCommon[i].u32;
						PRINTF(" TimeStamp: %ld",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32);
						br_uncompress_serie[ii].uncompress_samples_nb++;
					}
					PRINTF("\n");
				}
			}
		}
		else{	// separate time stamp
			PRINTF("separate time stamp\n");
			for(i=0;i<lflag.bits.nb_of_type_measure;i++)
			{
				//Tag
				bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,tag_sz);
				index+=tag_sz;

				ltag.bits.lbl=ltemp.u8;
				PRINTF(" tag: %d",ltag.bits.lbl);

				if((ii=get_index_uncompress_serie_from_tag(ltag,br_uncompress_serie))<0) return 0;
				PRINTF(" index: %d ",ii);

				//number of sample
				bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,8);
				br_uncompress_serie[ii].compress_samples_nb=ltemp.u8;
				index+=8;
				PRINTF(" number of sample: %d\n",br_uncompress_serie[ii].compress_samples_nb);

				if(br_uncompress_serie[ii].compress_samples_nb)
				{
					//TimeStamp Coding
					unsigned char ltimestamp_coding;
					bits_buf2sample(&ltemp, ST_U8,buf,index, buf_sz,2);
					ltimestamp_coding=ltemp.u8;
					index+=2;
					PRINTF(" TimeStamp Coding: %d\n",ltimestamp_coding);

					for(j=0;j<br_uncompress_serie[ii].compress_samples_nb;j++){
						//Delta Timestamp
						sz=get_bi_from_Hi(buf,buf_sz,index,ltimestamp_coding,&bi);
						if (!sz) return 0;
						PRINTF ("  j: %d bi: %d sz: %d",j,bi,sz);
						index+=sz;
						if(bi<=BR_HUFF_MAX_INDEX_TABLE)
						{
							if(bi>0){
								bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t, ST_U32,buf,index, buf_sz,bi);
								index+=bi;
								PRINTF(" Huffindex: %ld ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32);
								br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].t.u32+pow(2,bi)-1;
							}else{
								br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].t.u32;
							}
							PRINTF(" timestamp: %ld ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32);
						}
						else{
							bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t, ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
							index+=BM_ST_SZ(ST_U32);
						}
						if (br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32>last_timestamp) last_timestamp=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].t.u32;

						//Delta Value
						sz=get_bi_from_Hi(buf,buf_sz,index,br_uncompress_serie[ii].coding_table,&bi);
						if (!sz) return 0;
						index+=sz;
						PRINTF ("bi: %d sz: %d",bi,sz);
						if(bi<=BR_HUFF_MAX_INDEX_TABLE)
						{
							if(bi>0){
								bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v, ST_U16,buf,index, buf_sz,bi);
								index+=bi;
								PRINTF(" RawValue: %d ",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16);
								if(br_uncompress_serie[ii].coding_type==0)//ADLC
								{
									if(br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16>=pow(2,bi-1))
									{
										if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=((unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16)*br_uncompress_serie[ii].resolution.u32;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
										}else{
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=((float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16)*br_uncompress_serie[ii].resolution.fl;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
										}
									}
									else{
										if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+1-(unsigned long)pow(2,bi);
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
										}else{
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+1.0-(float)pow(2,bi);
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
											br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
										}
									}
								}
								else if(br_uncompress_serie[ii].coding_type==1) //Positive
								{
									if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+pow(2,bi)-1;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
									}else{
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+(float)pow(2,bi)-1.0;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl+=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
									}
								}
								else{		//Negative
									if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=(unsigned long)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+pow(2,bi)-1;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32*=br_uncompress_serie[ii].resolution.u32;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32-br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32;
									}else{
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=(float)br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u16+(float)pow(2,bi)-1.0;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl*=br_uncompress_serie[ii].resolution.fl;
										br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl-br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl;
									}
								}
								if(br_uncompress_serie[ii].tv.bits.type!=ST_FL)
									PRINTF(" Value: %ld\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
								else
									PRINTF(" Value: %f\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
							}else{
								if(br_uncompress_serie[ii].tv.bits.type!=ST_FL){
									br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.u32;
									PRINTF(" Value: %ld\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
								}else{
									br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl=br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb-1].v.fl;
									PRINTF(" Value: %f\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
								}
							}
						}
						else{
							bits_buf2sample(&br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v, br_uncompress_serie[ii].tv.bits.type,buf,index, buf_sz,BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type));
							index+=BM_ST_SZ(br_uncompress_serie[ii].tv.bits.type);
							if(br_uncompress_serie[ii].tv.bits.type!=ST_FL)
								PRINTF(" Complete: %ld\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.u32);
							else
								PRINTF(" Complete: %f\n",br_uncompress_serie[ii].uncompress_samples[br_uncompress_serie[ii].uncompress_samples_nb].v.fl);
							}
							br_uncompress_serie[ii].uncompress_samples_nb++;
					}
				}
			}
		}
	}

	PRINTF("TimeStamp of the sending\n");
	if(!last_timestamp){
		bits_buf2sample(&ltemp, ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
	}else{
		//Delta Timestamp
		sz=get_bi_from_Hi(buf,buf_sz,index,1,&bi);
		PRINTF("bi: %d sz: %d",bi, sz);
		if (!sz) return 0;
		index+=sz;
		if(bi<=BR_HUFF_MAX_INDEX_TABLE)
		{
			if(bi>0){
				bits_buf2sample(&ltemp,ST_U32,buf,index, buf_sz,bi);
				index+=bi;
				PRINTF(" raw: %ld ",ltemp.u32);
				ltemp.u32+=last_timestamp+pow(2,bi)-1;
			}else
				ltemp.u32=last_timestamp;
		}
		else{
			bits_buf2sample(&ltemp, ST_U32,buf,index, buf_sz,BM_ST_SZ(ST_U32));
			index+=BM_ST_SZ(ST_U32);
		}
	}
	br_uncompress_series->timestamp=ltemp.u32;
	PRINTF(" timestamp: %ld\n",br_uncompress_series->timestamp);
	return lflag.bits.nb_of_type_measure; //number of sample's serie
}
