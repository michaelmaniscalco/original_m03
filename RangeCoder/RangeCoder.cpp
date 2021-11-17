/*
  rangecod.c     range encoding

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
  belived (NO WARRANTY!) to be patent-free. Glen Langdon also
  confirmed my poinion that IBM UK did not protect that method.


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
  paper from 1997.

  The input and output is done by the inbyte and outbyte macros
  defined in the .c file; change them as needed; the first parameter
  passed to them is a pointer to the rangecoder structure; extend that
  structure as needed (and don't forget to initialize the values in
  start_encoding resp. start_decoding). This distribution writes to
  stdout and reads from stdin.

  There are no global or static var's, so if the IO is thread save the
  whole rangecoder is - unless GLOBALRANGECODER in rangecod.h is defined.

  For error recovery the last 3 bytes written contain the total number
  of bytes written since starting the encoder. This can be used to
  locate the beginning of a block if you have only the end. The header
  size you pass to initrangecoder is included in that count.

  There is a supplementary file called renorm95.c available at the
  website (www.compressconsult.com/rangecoder/) that changes the range
  coder to an arithmetic coder for speed comparisons.

  define RENORM95 if you want the arithmetic coder type renormalisation.
  Requires renorm95.c
  Note that the old version does not write out the bytes since init.
  you should not define GLOBALRANGECODER then. This Flag is provided
  only for spped comparisons between both renormalizations, see my
  data compression conference article 1998 for details.


#define NOWARN



#include <stdio.h>		
#include "port.h"
#include "rangecod.h"



#define CODE_BITS 32
#define Top_value ((code_value)1 << (CODE_BITS-1))

#define outbyte(cod,x) putchar(x)
#define inbyte(cod)    getchar()


#ifdef RENORM95
#include "renorm95.c"

#else
#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS-2) % 8 + 1)
#define Bottom_value (Top_value >> 8)

#ifdef NOWARN
#ifdef GLOBALRANGECODER
char coderversion[]="rangecoder 1.3 NOWARN GLOBAL (c) 1997-2000 Michael Schindler";
#else
char coderversion[]="rangecoder 1.3 NOWARN (c) 1997-2000 Michael Schindler";
#endif
#else   
#ifdef GLOBALRANGECODER
char coderversion[]="rangecoder 1.3 GLOBAL (c) 1997-2000 Michael Schindler";
#else
char coderversion[]="rangecoder 1.3 (c) 1997-2000 Michael Schindler";
#endif
#endif   
#endif  


#ifdef GLOBALRANGECODER

static rangecoder rngc;
#define RNGC (rngc)
#define M_outbyte(a) outbyte(&rngc,a)
#define M_inbyte inbyte(&rngc)
#define enc_normalize(rc) M_enc_normalize()
#define dec_normalize(rc) M_dec_normalize()
#else
#define RNGC (*rc)
#define M_outbyte(a) outbyte(rc,a)
#define M_inbyte inbyte(rc)
#endif



void start_encoding( rangecoder *rc, char c, int initlength )
{   RNGC.low = 0;                
    RNGC.range = Top_value;
    RNGC.buffer = c;
    RNGC.help = 0;               
    RNGC.bytecount = initlength;
}


#ifndef RENORM95
static Inline void enc_normalize( rangecoder *rc )
{   while(RNGC.range <= Bottom_value)    
    {   if (RNGC.low < (code_value)0xff<<SHIFT_BITS)  
        {   M_outbyte(RNGC.buffer);
            for(; RNGC.help; RNGC.help--)
                M_outbyte(0xff);
            RNGC.buffer = (unsigned char)(RNGC.low >> SHIFT_BITS);
        } else if (RNGC.low & Top_value) 
        {   M_outbyte(RNGC.buffer+1);
            for(; RNGC.help; RNGC.help--)
                M_outbyte(0);
            RNGC.buffer = (unsigned char)(RNGC.low >> SHIFT_BITS);
        } else                         
#ifdef NOWARN
            RNGC.help++;
#else
            if (RNGC.bytestofollow++ == 0xffffffffL)
            {   fprintf(stderr,"Too many bytes outstanding - File too large\n");
                exit(1);
            }
#endif
        RNGC.range <<= 8;
        RNGC.low = (RNGC.low<<8) & (Top_value-1);
        RNGC.bytecount++;
    }
}
#endif


void encode_freq( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f )
{	code_value r, tmp;
	enc_normalize( rc );
	r = RNGC.range / tot_f;
	tmp = r * lt_f;
	RNGC.low += tmp;
#ifdef EXTRAFAST
    RNGC.range = r * sy_f;
#else
    if (lt_f+sy_f < tot_f)
		RNGC.range = r * sy_f;
    else
		RNGC.range -= tmp;
#endif
}

void encode_shift( rangecoder *rc, freq sy_f, freq lt_f, freq shift )
{	code_value r, tmp;
	enc_normalize( rc );
	r = RNGC.range >> shift;
	tmp = r * lt_f;
	RNGC.low += tmp;
#ifdef EXTRAFAST
	RNGC.range = r * sy_f;
#else
	if ((lt_f+sy_f) >> shift)
		RNGC.range -= tmp;
	else  
		RNGC.range = r * sy_f;
#endif
}


#ifndef RENORM95
uint4 done_encoding( rangecoder *rc )
{   uint tmp;
    enc_normalize(rc);    
    RNGC.bytecount += 5;
    if ((RNGC.low & (Bottom_value-1)) < ((RNGC.bytecount&0xffffffL)>>1))
       tmp = RNGC.low >> SHIFT_BITS;
    else
       tmp = (RNGC.low >> SHIFT_BITS) + 1;
    if (tmp > 0xff) 
    {   M_outbyte(RNGC.buffer+1);
        for(; RNGC.help; RNGC.help--)
            M_outbyte(0);
    } else  
    {   M_outbyte(RNGC.buffer);
        for(; RNGC.help; RNGC.help--)
            M_outbyte(0xff);
    }
    M_outbyte(tmp & 0xff);
    M_outbyte((RNGC.bytecount>>16) & 0xff);
    M_outbyte((RNGC.bytecount>>8) & 0xff);
    M_outbyte(RNGC.bytecount & 0xff);
    return RNGC.bytecount;
}


int start_decoding( rangecoder *rc )
{   int c = M_inbyte;
    if (c==EOF)
        return EOF;
    RNGC.buffer = M_inbyte;
    RNGC.low = RNGC.buffer >> (8-EXTRA_BITS);
    RNGC.range = (code_value)1 << EXTRA_BITS;
    return c;
}


static Inline void dec_normalize( rangecoder *rc )
{   while (RNGC.range <= Bottom_value)
    {   RNGC.low = (RNGC.low<<8) | ((RNGC.buffer<<EXTRA_BITS)&0xff);
        RNGC.buffer = M_inbyte;
        RNGC.low |= RNGC.buffer >> (8-EXTRA_BITS);
        RNGC.range <<= 8;
    }
}
#endif


freq decode_culfreq( rangecoder *rc, freq tot_f )
{   freq tmp;
    dec_normalize(rc);
    RNGC.help = RNGC.range/tot_f;
    tmp = RNGC.low/RNGC.help;
#ifdef EXTRAFAST
    return tmp;
#else
    return (tmp>=tot_f ? tot_f-1 : tmp);
#endif
}

freq decode_culshift( rangecoder *rc, freq shift )
{   freq tmp;
    dec_normalize(rc);
    RNGC.help = RNGC.range>>shift;
    tmp = RNGC.low/RNGC.help;
#ifdef EXTRAFAST
    return tmp;
#else
    return (tmp>>shift ? ((code_value)1<<shift)-1 : tmp);
#endif
}


void decode_update( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f)
{   code_value tmp;
    tmp = RNGC.help * lt_f;
    RNGC.low -= tmp;
#ifdef EXTRAFAST
    RNGC.range = RNGC.help * sy_f;
#else
    if (lt_f + sy_f < tot_f)
        RNGC.range = RNGC.help * sy_f;
    else
        RNGC.range -= tmp;
#endif
}


unsigned char decode_byte(rangecoder *rc)
{   unsigned char tmp = decode_culshift(rc,8);
    decode_update( rc,1,tmp,(freq)1<<8);
    return tmp;
}

unsigned short decode_short(rangecoder *rc)
{   unsigned short tmp = decode_culshift(rc,16);
    decode_update( rc,1,tmp,(freq)1<<16);
    return tmp;
}


void done_decoding( rangecoder *rc )
{   dec_normalize(rc); 
}
*/


#include "stdio.h"
#include "rangeCoder.h"

void RangeEncoder::init()
{
	m_low = 0;                
	m_range = 0x80000000;
	m_help = 0;    
	m_bytecount = 0;           
}




void RangeEncoder::encode_freq(unsigned int sy_f, unsigned int lt_f, unsigned int tot_f)
{
	unsigned int r, tmp;
	normalize();
	r = m_range / tot_f;
	tmp = r * lt_f;
	m_low += tmp;
    if (lt_f+sy_f < tot_f)
		m_range = r * sy_f;
	else
		m_range -= tmp;
}



void RangeEncoder::encode_shift(unsigned int sy_f, unsigned int lt_f, unsigned int shift){	
	unsigned int r, tmp;
	normalize();
	r = m_range >> shift;
	tmp = r * lt_f;
	m_low += tmp;
	if ((lt_f + sy_f) >> shift)
		m_range -= tmp;
	else  
		m_range = r * sy_f;
	}




unsigned int RangeEncoder::done()
{   
	unsigned int tmp;
    normalize();     
    m_bytecount += 5;
    if ((m_low & (0x800000 - 1)) < ((m_bytecount & 0xFFFFFF) >> 1))
       tmp = m_low >> 23;
    else
       tmp = (m_low >> 23) + 1;
    if (tmp > 0xff) 
    {   
		m_dataStream->Write8(m_buffer + 1);
        for(; m_help; m_help--)
            m_dataStream->Write8(0);
    } else 
    {  
		 m_dataStream->Write8(m_buffer);
        for(; m_help; m_help--)
            m_dataStream->Write8(0xFF);
    }
    m_dataStream->Write8(tmp & 0xFF);
    m_dataStream->Write8((m_bytecount >> 16) & 0xFF);
    m_dataStream->Write8((m_bytecount >> 8) & 0xFF);
    m_dataStream->Write8(m_bytecount & 0xFF);

	m_dataStream->Flush();

	init();
    return m_bytecount;
}




void RangeEncoder::encode_bits(int value, int bits){
	if (bits < 16){
		value &= ((1 << bits) - 1);
		encode_shift(1, value, bits);
		}	
		else{
		while (bits){
			bits--;
			if (value & 1)
				encode_freq(1, 1, 2);
				else
				encode_freq(1, 0, 2);
			value >>= 1;
			}
		}
	}


//=====


int RangeDecoder::decode_bits(int nbits){
	int ret = 0;
	if (nbits < 16){
		ret = decode_culshift(nbits);
		decode_update(1, ret, 1 << nbits);
		}
		else{
		int mask = 1;
		int b;

		while (nbits){
			nbits--;
			decode_update(1, b = decode_culfreq(2), 2);
			if (b)
				ret |= mask;
			mask <<= 1;
			}
		}
	return ret;
	}


int RangeDecoder::init()
{
	m_low = 0;                
	m_range = 0x80000000;
	m_help = 0;    
	m_bytecount = 0;           

	int c = m_dataStream->Read8();
	if (c == EOF)
		return EOF;
    m_buffer = m_dataStream->Read8();
    m_low = m_buffer >> (8 - 7);
    m_range = (unsigned int) 1 << 7;
    return c;
}



unsigned int RangeDecoder::decode_culfreq(unsigned int tot_f)
{   
	unsigned int tmp;
	normalize();
    m_help = m_range / tot_f;
    tmp = m_low / m_help;
    return (tmp >= tot_f ? tot_f-1 : tmp);
	}



unsigned int RangeDecoder::decode_culshift(unsigned int shift){
	unsigned int tmp;
	normalize();
	m_help = m_range >> shift;
	tmp = m_low / m_help;
    return (tmp>>shift ? ((unsigned int)1<<shift)-1 : tmp);
	}


void RangeDecoder::decode_update(unsigned int sy_f, unsigned int lt_f, unsigned int tot_f){   
	unsigned int tmp;
	tmp = m_help * lt_f;
	m_low -= tmp;
	if (lt_f + sy_f < tot_f)
		m_range = m_help * sy_f;
		else
		m_range -= tmp;
	}


void RangeDecoder::done()
{
	normalize(); 
	init();  
}


