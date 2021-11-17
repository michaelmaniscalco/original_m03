#ifndef M03_ENCODER_H
#define M03_ENCODER_H

#include <boost/scoped_array.hpp>
#include <iostream>
#include "RangeCoder/RangeCoder.h"
#include "M03_Coder.h"


class M03_Encoder : public M03_Coder
{
public:

	M03_Encoder(RangeEncoder * pRangeEncoder);

	virtual ~M03_Encoder();

	void Encode(unsigned char * data, unsigned int size, unsigned int saSentinel = ~0);

protected:

private:

	typedef unsigned char	symbol_type;
	typedef size_t sentinel_index;
	typedef size_t suffix_index;

	symbol_type * m_pRleBuffer;
	const symbol_type * m_pSentinel;
	void InitializeRleBuffer(const symbol_type *, size_t, sentinel_index);
	void GetRleInfo(suffix_index, symbol_type &, size_t &);
	bool TestRle(size_t);

	void GetIndex(unsigned int runLength);

	unsigned int	m_uniqueSymbolList[MAX_SYMBOL];
	unsigned int	m_numUniqueSymbols;
	
	unsigned short *m_symbolFirstIndex;

	unsigned int	m_currentSymbol;

	unsigned int	m_currentIndex;
	unsigned int	m_sentinelIndex;

	unsigned int	m_inputSize;
	unsigned int	m_symbolStart[MAX_SYMBOL];		// starting index for symbol top level context
	unsigned int	m_symbolList[MAX_SYMBOL];
	unsigned int	m_symbolListSize;
	unsigned int	m_singletonCount;

	typedef struct LocalizedRecord
	{
		unsigned int m_index;
		unsigned int m_hasIndexCounter;
		unsigned int m_currentPushIndex;
		unsigned int m_symbolCount;
		unsigned int m_subContextCounter;
		unsigned int m_firstSubContextCount;
	} LocalizedRecord;
	LocalizedRecord m_localizedRecord[MAX_SYMBOL];

	unsigned char *	m_pData;
	unsigned char *	m_pEndData;

	unsigned char *	m_pRLE;
	unsigned char * m_pEndRLE;

	unsigned int	m_counter;

	unsigned int	m_currentRunLength;
	unsigned int	m_currentRunSymbol;
	unsigned int	m_subContextCounter;

	unsigned int	m_parentContextSymbolCount[MAX_SYMBOL];
	unsigned int	m_parentContextSymbolList[MAX_SYMBOL];
	unsigned int	m_parentContextSymbolListSize;

	unsigned int	m_subContextSymbolCount[MAX_SYMBOL][MAX_SYMBOL];
	unsigned int	m_subContextSymbolList[MAX_SYMBOL][MAX_SYMBOL];
	unsigned int	m_subContextSymbolListSize[MAX_SYMBOL];

	void ConstructRLE();
	void ProcessNextOrder();
	void ProcessNextContext();

	void GetSymbolAndRunLength(unsigned int contextIndex, unsigned int & symbol, unsigned int & runLength, unsigned int maxRunLength);
	void GenerateRunLengthInfo(unsigned char * pData, unsigned int size);
	void IncrementContextCounter();
	void IncrementSubContextCounter();

	unsigned int CountUniqueSymbols(unsigned int index, unsigned int length);


	typedef struct SubContextStat
	{
		unsigned int index;
		unsigned int size;
		bool operator < (const struct SubContextStat & s) const
		{
			if (size != s.size)
				return (size < s.size);
			return (index < s.index);
		}
		bool operator == (const struct SubContextStat & s) const
		{
			return (size == s.size);
		}
		bool operator > (const struct SubContextStat & s) const
		{
			return (size > s.size);
		}
	} SubContextStat;

	void EncodeInitialSymbolCounts(unsigned int * counts, unsigned int numSymbols);
	void EncodeSymbolDistribution(SubContextStat * subContextLengths, 
							 unsigned int firstSubContext, unsigned int lastSubContext, 
							 unsigned int trend, unsigned int recursionDepth);
	void EncodeNumber(unsigned int value, unsigned int maxValue, unsigned int ratio, unsigned int & trend, unsigned int lastBit = 0, unsigned int depth = 0);
	void EncodeValue(unsigned int countA, unsigned int countB, unsigned int listCountA, unsigned int listCountB, unsigned int numSymbols, unsigned int & trend, unsigned int depth = 0);
	RangeEncoder *	m_pRangeEncoder;

	typedef struct SymbolStat
	{
		unsigned int symbol;
		unsigned int count;

		bool operator < (const struct SymbolStat & s) const
		{
			if (s.count != count)
				return (count < s.count);
			return (symbol < s.symbol);
		}
		bool operator > (const struct SymbolStat & s) const
		{
			if (s.count != count)
				return (count > s.count);
			return (symbol > s.symbol);
		}
		bool operator == (const struct SymbolStat & s) const
		{
			return ((s.count == count) && (s.symbol == symbol));
		}
	} SymbolStat;

	unsigned int	m_currentSymbol2;
};



__FORCEINLINE__ void M03_Encoder::IncrementContextCounter()
{
	m_counter += 4;
	if (m_counter == 0)
	{
		for (unsigned int i = 0; i < MAX_SYMBOL; i++)
			m_localizedRecord[i].m_hasIndexCounter = 0;
		m_counter = 4;
	}
}




__FORCEINLINE__ void M03_Encoder::IncrementSubContextCounter()
{
	m_subContextCounter += 2;
	if (m_subContextCounter == 0)
	{
		for (unsigned int i = 0; i < MAX_SYMBOL; i++)
			m_localizedRecord[i].m_subContextCounter = 0;
		m_subContextCounter = 2;
	}	
}




__FORCEINLINE__ void M03_Encoder::GetSymbolAndRunLength(unsigned int index, unsigned int & symbol, 
						unsigned int & runLength, unsigned int maxRunLength)
{
	if (index == m_sentinelIndex)
	{
		symbol = 0;
		runLength = 1;
		return;
	}

	unsigned char * pRead = m_pRLE + (index << 2) + 4;
	unsigned char * pEnd = m_pEndRLE;
	symbol = pRead[-4];
	symbol++;

	unsigned int ret = 1;
	while (pRead != pEnd)
	{
		if (pRead[-3] & 0x80)
		{
			// found run length info
			unsigned int runLength = (pRead[-3] & 0x7f);
			runLength <<= 7;
			runLength |= (pRead[1] & 0x7f);
			runLength <<= 7;
			runLength |= (pRead[5] & 0x7f);
			runLength <<= 7;
			runLength |= (pRead[9] & 0x7f);
			ret += runLength;
			ret--;
			break;
		}
		if (*pRead != pRead[-4])
			break;
		ret += 1;
		pRead += 4;
		if (ret >= maxRunLength)
			break;
	}
	runLength = ret;
}




__FORCEINLINE__ void M03_Encoder::GenerateRunLengthInfo(unsigned char * pData, unsigned int size)
{
	m_pRLE = pData;
	m_pEndRLE = m_pRLE + ((size + 1) << 2);
	unsigned int n = (size << 2);
	bool sentinelCounted = false;

	if (size == m_sentinelIndex)
	{
		sentinelCounted = true;
		pData[n] = (pData[size - 1] + 1);
		n -= 4;		
	}

	for (int i = size - 1; i >= 0; )
	{
		bool addSentinel = (i == m_sentinelIndex);
		pData[n] = pData[i];
		i--;
		n -= 4;

		if ((addSentinel) && (!sentinelCounted))
		{
			sentinelCounted = true;
			pData[n] = (i >= 0) ? (pData[i] + 1) :( pData[1] + 1);
			n -= 4;		
		}

	}

	unsigned char * pRLE = m_pRLE + 1;
	unsigned char * pEndData = pData + ((size + 1) << 2);
	unsigned char * pSentinel = pData + (m_sentinelIndex << 2);

	while ((pData != pEndData) || (pSentinel))
	{
		if (pData == pSentinel)
		{
			*pRLE = 0;
			pRLE += 4;
			pSentinel = 0;
			pData += 4;
		}
		else
		{
			const unsigned char * pStart = pData;
			pData += 4;
			while ((pData != pEndData) && (pData != pSentinel) && (*pStart == *pData))
				pData += 4;
			unsigned int runLength = (unsigned int)((pData - pStart) >> 2);
			if (runLength >= 4)
			{
				// can add RLE info
				while (runLength > 3)
				{
					*pRLE = (unsigned char)(0x80 | (runLength >> 21));
					pRLE += 4;
					*pRLE = (unsigned char)((runLength >> 14) & 0x7f);
					pRLE += 4;
					*pRLE = (unsigned char)((runLength >> 7) & 0x7f);
					pRLE += 4;
					*pRLE = (unsigned char)(runLength & 0x7f);
					pRLE += 4;
					runLength -= 4;
				}
			}
			while (runLength)
			{
				*pRLE = 0;
				pRLE += 4;
				runLength--;
			}
		}
	}
}




__FORCEINLINE__ void M03_Encoder::EncodeNumber(unsigned int value, unsigned int maxValue, unsigned int ratio, unsigned int & trend, 
							   unsigned int lastBit, unsigned int depth)
{
	unsigned int freqA;
	unsigned int freqB;
	unsigned int totalFreq;

	unsigned int min = (maxValue >> 1);
	unsigned int primaryModelIndex, secondaryModelIndex;
	GetBitModelProb(ratio, depth, maxValue, lastBit, trend, freqA, freqB, totalFreq, primaryModelIndex, secondaryModelIndex);

	if (value <= min)
	{
		m_pRangeEncoder->encode_freq(freqA, 0, totalFreq);
		maxValue = min;
		AdjustBitModel(primaryModelIndex, secondaryModelIndex);
		if (maxValue)
			EncodeNumber(value, maxValue, ratio, trend, (lastBit << 1), depth + 1);
	}
	else
	{
		m_pRangeEncoder->encode_freq(freqB, freqA, totalFreq);
		value -= (min + 1);
		maxValue -= (min + 1);
		AdjustBitModel(primaryModelIndex + 1, secondaryModelIndex + 1);
		if (maxValue)
			EncodeNumber(value, maxValue, ratio, trend, (lastBit << 1) | 1, depth + 1);
	}
}




__FORCEINLINE__ void M03_Encoder::EncodeValue(unsigned int countA, unsigned int countB, unsigned int listCountA, unsigned int listCountB,
							  unsigned int numSymbols, unsigned int & trend, unsigned int depth)
{
	unsigned int maxValue = countA + countB;
	unsigned int assumedInA = 0;
	unsigned int assumedInB = 0;

	unsigned int forcedDistribution = 0;

	if (maxValue > listCountA)
	{
		assumedInB = maxValue - listCountA;
		countB -= assumedInB;
		maxValue -= assumedInB;
		listCountB -= assumedInB;
		forcedDistribution |= 1;
	}
	if (maxValue > listCountB)
	{
		assumedInA = maxValue - listCountB;
		countA -= assumedInA;
		maxValue -= assumedInA;
		listCountA -= assumedInA;
		forcedDistribution |= 2;
	}

	if (maxValue == 0)
		return;

	unsigned int ratio = (listCountA << SIGN_MODEL_RATIO_BITS) / (listCountB + listCountA);
	unsigned int freqA, freqB, freqC, totalFreq;
	unsigned int primaryModelIndex, secondaryModelIndex;
	GetSignProb(numSymbols, maxValue, ratio, forcedDistribution, freqA, freqB, freqC, totalFreq, primaryModelIndex, secondaryModelIndex);

	if (countA == countB)
	{
		// encode an even split.
		m_pRangeEncoder->encode_freq(freqC, freqA + freqB, totalFreq);
		AdjustSignModel(primaryModelIndex + 2, secondaryModelIndex + 2);
		return;
	}
	else
	{
		if (countA > countB)
		{
			// encode that left is larger than right
			m_pRangeEncoder->encode_freq(freqA, 0, totalFreq);
			AdjustSignModel(primaryModelIndex, secondaryModelIndex);
			countA--;
			listCountA--;
		}
		else
		{
			// encode that right is larger than left.
			m_pRangeEncoder->encode_freq(freqB, freqA, totalFreq);
			AdjustSignModel(primaryModelIndex + 1, secondaryModelIndex + 1);
			countB--;
			listCountB--;
		}
	}

	//
	unsigned int dif = (countA > countB) ? (countA - countB) >> 1 : (countB - countA) >> 1;
	unsigned int max = (countA + countB) >> 1;

	if (max)
	{
		EncodeNumber(dif, max, ratio, trend);
		AdjustTrend(dif, max, trend);
	}
}





__FORCEINLINE__ void M03_Encoder::GetIndex(unsigned int runLength)
{
	// symbol previously appears less than twice in this parent context
	if (m_localizedRecord[m_currentRunSymbol].m_hasIndexCounter < (m_counter - 1))
	{
		if (runLength > 1)
		{
			// first occurrence of two or more of the symbol in this parent context
			m_localizedRecord[m_currentRunSymbol].m_index = m_symbolFirstIndex[(m_currentIndex << 1)];
			m_localizedRecord[m_currentRunSymbol].m_index <<= 16;
			m_localizedRecord[m_currentRunSymbol].m_index |= m_symbolFirstIndex[(m_currentIndex << 1) + 2];
			m_localizedRecord[m_currentRunSymbol].m_hasIndexCounter = m_counter;
		}
		else
		{
			// first occurrence of a single symbol in this parent context
			m_localizedRecord[m_currentRunSymbol].m_index = m_symbolFirstIndex[(m_currentIndex << 1)];
			m_localizedRecord[m_currentRunSymbol].m_hasIndexCounter = (m_counter - 1);
			return;
		}
	}
	else
	{
		// second occurrence of a single symbol in this parent context
		m_localizedRecord[m_currentRunSymbol].m_index <<= 16;
		m_localizedRecord[m_currentRunSymbol].m_index |= m_symbolFirstIndex[(m_currentIndex << 1)];
		m_localizedRecord[m_currentRunSymbol].m_hasIndexCounter = m_counter;
	}


	// symbol now appears at least twice in the parent context
	unsigned int p = m_localizedRecord[m_currentRunSymbol].m_index;
	if (p >= 0x80000000)
	{
		unsigned int upper = (p & 0x7fff);
		p = (p >> 16) & 0x7fff;
		if (p == 0x7ffe)
			upper--;
		p |= (upper << 15);
		p++;
	}
	else
	{
		p &= 0xffff7fff;
		p = ((p & 0x7fff) | ((p & 0xffff0000) >> 1));
	}
	m_localizedRecord[m_currentRunSymbol].m_index = m_symbolStart[m_currentRunSymbol] + p;			
}



#endif
