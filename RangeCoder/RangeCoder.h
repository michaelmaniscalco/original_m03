#ifndef RANGE_CODER_H
#define RANGE_CODER_H

// this range coder directly adapted from Michael Schindler's range coder
// transfered to c++ classes by Michael Maniscalco, 2002

/*
#ifndef rangecod_h
#define rangecod_h

  rangecod.h     headerfile for range encoding

  (c) Michael Schindler
  1997, 1998, 1999, 2000
  http://www.compressconsult.com/
  michael@compressconsult.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.  It may be that this
  program violates local patents in your country, however it is
  belived (NO WARRANTY!) to be patent-free here in Austria. Glen
  Langdon also confirmed my poinion that IBM UK did not protect that
  method.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  Range encoding is based on an article by G.N.N. Martin, submitted
  March 1979 and presented on the Video & Data Recording Conference,
  Southampton, July 24-27, 1979. If anyone can name the original
  copyright holder of that article please contact me; this might
  allow me to make that article available on the net for general
  public.

  Range coding is closely related to arithmetic coding, except that
  it does renormalisation in larger units than bits and is thus
  faster. An earlier version of this code was distributed as byte
  oriented arithmetic coding, but then I had no knowledge of Martin's
  paper from 1979.

  The input and output is done by the INBYTE and OUTBYTE macros
  defined in the .c file; change them as needed; the first parameter
  passed to them is a pointer to the rangecoder structure; extend that
  structure as needed (and don't forget to initialize the values in
  start_encoding resp. start_decoding). This distribution writes to
  stdout and reads from stdin.

  There are no global or static var's, so if the IO is thread save the
  whole rangecoder is - unless GLOBALRANGECODER in rangecod.h is defined.

  For error recovery the last 3 bytes written contain the total number
  of bytes written since starting the encoder. This can be used to
  locate the beginning of a block if you have only the end.

  For some application using a global coder variable may provide a better
  performance. This will allow you to use only one coder at a time and
  will destroy thread savety. To enabble this feature uncomment the
  #define GLOBALRANGECODER line below.
#include "port.h"
#if 0    
#include <limits.h>
#if INT_MAX > 0xffff
typedef unsigned int uint4;
typedef unsigned short uint2;
#else
typedef unsigned long uint4;
typedef unsigned int uint2;
#endif
#endif

extern char coderversion[];

typedef uint4 code_value;       

typedef uint4 freq; 



typedef struct {
    uint4 low,           
          range,         
          help;          
    unsigned char buffer;
    uint4 bytecount;     
} rangecoder;

#ifdef GLOBALRANGECODER
#define start_encoding(rc,a,b) M_start_encoding(a,b)
#define encode_freq(rc,a,b,c) M_encode_freq(a,b,c)
#define encode_shift(rc,a,b,c) M_encode_shift(a,b,c)
#define done_encoding(rc) M_done_encoding()
#define start_decoding(rc) M_start_decoding()
#define decode_culfreq(rc,a) M_decode_culfreq(a)
#define decode_culshift(rc,a) M_decode_culshift(a)
#define decode_update(rc,a,b,c) M_decode_update(a,b,c)
#define decode_byte(rc) M_decode_byte()
#define decode_short(rc) M_decode_short()
#define done_decoding(rc) M_done_decoding()
#endif

void start_encoding( rangecoder *rc, char c, int initlength);

void encode_freq( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f );
void encode_shift( rangecoder *rc, freq sy_f, freq lt_f, freq shift );
#define encode_byte(ac,b)  encode_shift(ac,(freq)1,(freq)(b),(freq)8)
#define encode_short(ac,s) encode_shift(ac,(freq)1,(freq)(s),(freq)16)
uint4 done_encoding( rangecoder *rc );
int start_decoding( rangecoder *rc );
freq decode_culfreq( rangecoder *rc, freq tot_f );
freq decode_culshift( rangecoder *ac, freq shift );
void decode_update( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f);
#define decode_update_shift(rc,f1,f2,f3) decode_update((rc),(f1),(f2),(freq)1<<(f3));
unsigned char decode_byte(rangecoder *rc);
unsigned short decode_short(rangecoder *rc);
void done_decoding( rangecoder *rc );
#endif
*/

#include "../DataStream/DataStream.h"


class RangeCoder{
	public:
		RangeCoder():m_bytecount(0){}
	protected:
		unsigned int	m_low;        
    	unsigned int	m_range;        
    	unsigned int	m_help;          
		unsigned char	m_buffer;
    	unsigned int	m_bytecount;
	private:
	};




class RangeEncoder : public RangeCoder
{
	public:

		RangeEncoder(EncodeDataStream * dataStream):m_dataStream(dataStream){}

		unsigned int done();
		void init();
		void encode_freq(unsigned int sy_f, unsigned int lt_f, unsigned int tot_f);
		void encode_bits(int value, int bits);
		void encode_shift(unsigned int sy_f, unsigned int lt_f, unsigned int shift);

		EncodeDataStream * GetDataStream() const {return m_dataStream;}
	private:
		inline void normalize();
		EncodeDataStream *	m_dataStream;
};




class RangeDecoder : public RangeCoder{
	public:
		RangeDecoder(DecodeDataStream * dataStream):m_dataStream(dataStream){}
		void decode_update(unsigned int sy_f, unsigned int lt_f, unsigned int tot_f);
		unsigned int decode_culshift(unsigned int shift);
		unsigned int decode_culfreq(unsigned int tot_f);
		int  init();
		void done();
		int decode_bits(int nbits);
	private:
		inline void normalize();
		DecodeDataStream * m_dataStream;
	};




__forceinline void RangeEncoder::normalize(){
	while(m_range <= 0x800000){   
		if (m_low < 0x7F800000){
			m_dataStream->Write8(m_buffer);
			for(; m_help; m_help--)
				m_dataStream->Write8(0xff);
			m_buffer = (unsigned char)(m_low >> 23);
			} 
			else 
			if (m_low & 0x80000000){   
				m_dataStream->Write8(m_buffer+1);
				for(; m_help; m_help--)
					m_dataStream->Write8(0);
				m_buffer = (unsigned char)(m_low >> 23);
				} 
			else
				m_help++;

			m_range <<= 8;
			m_low = (m_low<<8) & (0x80000000-1);
			m_bytecount++;
		}
	}



__forceinline void RangeDecoder::normalize(){   
	while (m_range <= 0x800000){
	m_low = (m_low << 8) | ((m_buffer << 7) & 0xff);
		m_buffer = m_dataStream->Read8();
		m_low |= m_buffer >> (8 - 7);
		m_range <<= 8;
		}
	}




#endif