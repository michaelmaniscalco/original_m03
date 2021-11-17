#ifndef M03_DECODER_H
#define M03_DECODER_H

#include <boost/scoped_array.hpp>
#include <iostream>
#include "RangeCoder/RangeCoder.h"
#include "M03_Coder.h"



class M03_Decoder : public M03_Coder
{
public:

	M03_Decoder(RangeDecoder * pRangeDecoder);

	virtual ~M03_Decoder();

	void Decode(unsigned char * & pData, size_t size);

protected:

private:

	typedef struct ContextSymbolStat
	{
		unsigned int	m_symbol;
		unsigned int	m_firstIndex;
		unsigned int	m_count;
		unsigned int	m_subContextIndex;

		bool operator < (const struct ContextSymbolStat & s) const	
		{
			if (m_count != s.m_count)
				return (m_count < s.m_count);
			return (m_symbol < s.m_symbol);
		}
	} ContextSymbolStat;

	void DecodeHeader();

	void DecodeInitialSymbolCounts(unsigned int * symbolCount, unsigned int numSymbols, unsigned int totalCount);

	unsigned int DecodeValue(unsigned int maxValue, unsigned int listCountA, unsigned int listCountB, unsigned int numSymbols, unsigned int & trend);

	unsigned int DecodeNumber(unsigned int maxValue, unsigned int ratio, unsigned int & trend, unsigned int lastBit = 0, unsigned int depth = 0);

	void DecodeNextOrder();

	void DecodeNextContext();

	unsigned int WriteContextInfo(unsigned char * pAddress, unsigned int symbol, unsigned int count, unsigned int index);

	unsigned int ReadContextInfo(unsigned char * pAddress, unsigned int & symbol, unsigned int & count, unsigned int & index);

	void DecodeChildContextStats(ContextSymbolStat * contextSymbolStats, unsigned int numContextSymbols, 
					unsigned int * subContextLengths, unsigned int numSubContexts, unsigned int trend, unsigned int recursionDepth = 0);

	unsigned int						m_currentOrder;

	RangeDecoder * const				m_pRangeDecoder;

	unsigned char *						m_pWorkspace;
	unsigned char *						m_pEndWorkspace;

//	Queue								m_queue[MAX_SYMBOL];

	bool								m_decodeFinished;

	unsigned int						m_numUniqueSymbols;
	unsigned int						m_uniqueSymbolList[MAX_SYMBOL];

	unsigned int			m_currentSymbol;
	unsigned int			m_currentIndex;

	unsigned char *			m_pContextInfo;

	unsigned int			m_uniqueSplitCounter;
	unsigned int			m_splitCounter[MAX_SYMBOL];
	void IncrementUniqueSplitCounter();

	unsigned int			m_symbolFirstIndex[MAX_SYMBOL];
	unsigned int			m_symbolCurrentIndex[MAX_SYMBOL];

	boost::scoped_array<unsigned char>	m_spDecodedData;
	size_t					m_decodedSymbolCount;
	unsigned int			m_blockSize;
	unsigned int			m_sentinelIndex;

	unsigned int		m_isVowel;
};


__FORCEINLINE__ void M03_Decoder::IncrementUniqueSplitCounter()
{
	m_uniqueSplitCounter += 2;
	if (m_uniqueSplitCounter == ~0)
	{
		for (unsigned int i = 0; i < MAX_SYMBOL; i++)
			m_splitCounter[i] = 0;
	}
}



#endif
