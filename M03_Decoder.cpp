#include "M03_Decoder.h"
#include <iostream>
#include <boost/scoped_ptr.hpp>
#include "MSufSort.h"

using namespace std;



M03_Decoder::M03_Decoder(RangeDecoder * pRangeDecoder):m_pRangeDecoder(pRangeDecoder)
{
}




M03_Decoder::~M03_Decoder()
{
}




void M03_Decoder::Decode(unsigned char * & pData, size_t size)
{
	// start by decoding blocksize, symbol list and symbol counts.
	m_blockSize = size;
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
		m_splitCounter[i] = 0;
	m_uniqueSplitCounter = ~0;

	DecodeHeader();
	m_currentOrder = 0;
	m_decodeFinished = false;
	unsigned int currentOrder = 0;

	m_decodedSymbolCount = 0;
	while (!m_decodeFinished)
		DecodeNextOrder();

	for (unsigned int i = m_sentinelIndex; i < m_blockSize; i++)
		m_pContextInfo[i * 3] = m_pContextInfo[(i + 1) * 3];

	for (unsigned int i = 0; i < m_blockSize; i++)
		m_pWorkspace[i] = m_pContextInfo[i * 3];
	boost::scoped_ptr<MSufSort> spMSufSort(new MSufSort);
	
	spMSufSort->UnBWT(m_pWorkspace, m_blockSize, m_sentinelIndex, (char *)m_pWorkspace + m_blockSize + 4);
	realloc(m_pWorkspace, m_blockSize);
	pData = m_pWorkspace;
	m_pWorkspace = 0;
}




void M03_Decoder::DecodeHeader()
{
	// decode list of symbols appearing in the block
	unsigned int symbolList[0x100];
	unsigned int symbolListSize = 0;
	for (unsigned int i = 1; i < MAX_SYMBOL; i++)
	{
		if (m_pRangeDecoder->decode_bits(1))
			symbolList[symbolListSize++] = (i - 1);
	}

	// decode count of each symbol 
	unsigned int temp[0x100];
	memset(temp, 0x00, sizeof(temp));
	DecodeInitialSymbolCounts(temp, symbolListSize, m_blockSize);

	// create array containing the original symbols and their counts
	unsigned int symbolCount[MAX_SYMBOL];
	memset(symbolCount, 0x00, sizeof(symbolCount));
	for (unsigned int i = 0; i < symbolListSize; i++)
		symbolCount[symbolList[i] + 1] = temp[i];
	symbolCount[0] = 1;	// include sentinel

	// allocate memory for decoding
	unsigned int numUniqueSymbols = (symbolListSize + 1);
	unsigned int contextInfoSpace = ((m_blockSize + 1) * 3);
	unsigned int queueSpace = (4096 + (2048 * 0x101) + (m_blockSize + 1));
	unsigned int workspaceRequired = (contextInfoSpace + queueSpace);
	
	workspaceRequired = ((m_blockSize + 1) * 5) + 4096 + (MAX_SYMBOL * 1024);
	m_pWorkspace = new unsigned char[workspaceRequired];
	m_pEndWorkspace = m_pWorkspace + workspaceRequired;

	// set up the partition queues with the initial sub context boundaries
	char * pStack = (char *)m_pWorkspace;
	unsigned int total = 0;
	m_numUniqueSymbols = 0;
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		unsigned int count = symbolCount[i];
		if (count)
		{
			m_uniqueSymbolList[m_numUniqueSymbols++] = i;
			m_symbolFirstIndex[i] = total;
			total += count;

			unsigned int spaceRequired = ((count * 2) + 1024);
			if (i == 0)
				spaceRequired = 4096;
			size_t allocationSize = m_queue[i].Allocate(pStack, spaceRequired);
			pStack += allocationSize;
			m_queue[0].Push(count, (i == 0) ? START_CONTEXT : 0);
		}
	}
	m_pContextInfo = (unsigned char *)pStack;

	// write symbol stats into initial context
	unsigned char * pWriteAddress = m_pContextInfo;
	unsigned int index = 0;
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		if (symbolCount[i])
		{
			pWriteAddress += WriteContextInfo(pWriteAddress, i, symbolCount[i], index);
			index += symbolCount[i];
		}
	}
}




void M03_Decoder::DecodeNextOrder()
{
	m_decodeFinished = true;	// assume the best ...
	const char * queueBack[MAX_SYMBOL];
	for (unsigned int i = 0; i < m_numUniqueSymbols; i++)
	{
		unsigned int symbol = m_uniqueSymbolList[i];
		m_symbolCurrentIndex[symbol] = m_symbolFirstIndex[symbol];
		queueBack[symbol] = m_queue[symbol].GetBack();
	}

	for (unsigned int i = 0; i < m_numUniqueSymbols; i++)
	{
		m_currentSymbol = m_uniqueSymbolList[i];
		m_currentIndex = m_symbolFirstIndex[m_currentSymbol];
		M03_Queue * pQueue = &m_queue[m_currentSymbol];
		const char * pFinished = queueBack[m_currentSymbol];
		while (pQueue->GetFront() != pFinished)
		{
			if (pQueue->Flags() & SKIP)
			{
				m_currentIndex += pQueue->Front();
				pQueue->Pop();
			}
			DecodeNextContext();
		}
	}
}




void M03_Decoder::DecodeNextContext()
{
	IncrementUniqueSplitCounter();

	// pop length of all sub contexts 
	M03_Queue * pQueue = &m_queue[m_currentSymbol];
	unsigned int totalContextLength = 0;
	unsigned int subContextLengths[MAX_SYMBOL];
	unsigned int numSubContexts = 0;
	do 
	{
		unsigned int n = pQueue->Front();
		pQueue->Pop();
		totalContextLength += n;
		subContextLengths[numSubContexts++] = n;
	} while ((!pQueue->Empty()) && (!pQueue->Flags()));

	// pop count of all symbols in parent context
	ContextSymbolStat contextSymbolStats[MAX_SYMBOL];
	unsigned int numContextSymbols = 0;
	unsigned int spaceRemainingInParentContext = totalContextLength;
	unsigned char * pReadAddress = m_pContextInfo + (m_currentIndex * 3);

	while (spaceRemainingInParentContext)
	{
		pReadAddress += ReadContextInfo(pReadAddress, contextSymbolStats[numContextSymbols].m_symbol, 
					contextSymbolStats[numContextSymbols].m_count, contextSymbolStats[numContextSymbols].m_firstIndex);
		spaceRemainingInParentContext -= contextSymbolStats[numContextSymbols].m_count;
		numContextSymbols++;
	}

	// decode the distribution of symbols from parent to sub-contexts.
	DecodeChildContextStats(contextSymbolStats, numContextSymbols, subContextLengths, numSubContexts, 0);
}




void M03_Decoder::DecodeChildContextStats(ContextSymbolStat * contextSymbolStats, unsigned int numContextSymbols, 
										  unsigned int * subContextLengths, unsigned int numSubContexts, 
										  unsigned int trend, unsigned int recursionDepth)
{
	if (numSubContexts == 1)
	{
		// no further decoding required
		if ((numContextSymbols == 1) && (contextSymbolStats[0].m_count == 1))
		{
			// decoded symbol
			if (contextSymbolStats[0].m_symbol == 0)
				m_sentinelIndex = m_currentIndex;
		//	m_spDecodedData[m_currentIndex] = (contextSymbolStats[0].m_symbol - 1);
			m_decodedSymbolCount++;
		}

		// push each new sub context boundary (caused when symbols are distributed between more than one child context)
		for (unsigned int i = 0; i < numContextSymbols; i++)
		{
			unsigned int symbol = contextSymbolStats[i].m_symbol;
			if (m_splitCounter[symbol] >= m_uniqueSplitCounter)
			{
				// symbol splits
				unsigned int subContextLength = contextSymbolStats[i].m_count;
				if (m_splitCounter[symbol] == m_uniqueSplitCounter)
				{
					// first split in parent context
					if (m_symbolCurrentIndex[symbol] != contextSymbolStats[i].m_firstIndex)
					{
						// need to add skip value first
						unsigned int skipLength = (contextSymbolStats[i].m_firstIndex - m_symbolCurrentIndex[symbol]);
						m_queue[symbol].Push(skipLength, SKIP);
						m_symbolCurrentIndex[symbol] += skipLength;
					}
					m_queue[symbol].Push(subContextLength, START_CONTEXT);
					m_splitCounter[symbol]++;
				}
				else
				{
					// second or subsequent split in parent context
					m_queue[symbol].Push(subContextLength, 0);
				}
				m_symbolCurrentIndex[symbol] += subContextLength;			
			}

		}		
			
		// write the symbol count for each symbol (the context info) into the correct location.
		unsigned char * pWriteAddress = (m_pContextInfo + (m_currentIndex * 3));
		for (unsigned int i = 0; i < numContextSymbols; i++)
		{
			pWriteAddress += WriteContextInfo(pWriteAddress, contextSymbolStats[i].m_symbol, contextSymbolStats[i].m_count, contextSymbolStats[i].m_firstIndex);
			m_currentIndex += contextSymbolStats[i].m_count;
		}
		return;
	}
	recursionDepth++;

	// more than one sub context.  Decode split.

	// count all symbols in left half
	unsigned int numContextsOnLeft = ((numSubContexts + 1) >> 1);
	unsigned int numContextsOnRight = (numSubContexts - numContextsOnLeft);

	// calculate total count in left half and right half
	unsigned int leftTotalCount = 0;
	unsigned int rightTotalCount = 0;
	for (unsigned int i = 0; i < numContextsOnLeft; i++)
		leftTotalCount += subContextLengths[i];
	for (unsigned int i = numContextsOnLeft; i < numSubContexts; i++)
		rightTotalCount += subContextLengths[i];

	// sort parent symbol list
	std::sort(contextSymbolStats, contextSymbolStats + numContextSymbols);
	// reverse list if number of symbols in context is > 8
	if (numContextSymbols > 8)
		std::reverse(contextSymbolStats, contextSymbolStats + numContextSymbols);

	// begin decoding distribution of symbols from parent into the left and the right halves
	unsigned int numSymbolsOnLeft = 0;
	unsigned int numSymbolsOnRight = 0;
	ContextSymbolStat leftSymbolStats[MAX_SYMBOL];
	ContextSymbolStat rightSymbolStats[MAX_SYMBOL];

	m_isVowel = 0;

	for (unsigned int i = 0; i < numContextSymbols; i++)
	{
		unsigned int symbol = contextSymbolStats[i].m_symbol;
		unsigned int totalCount = contextSymbolStats[i].m_count;
		unsigned int countOnLeft = 0;
		unsigned int countOnRight = 0;

		if ((leftTotalCount) && (rightTotalCount))
		{
			countOnLeft = DecodeValue(totalCount, leftTotalCount, rightTotalCount, numContextSymbols - i, trend);
			countOnRight = totalCount - countOnLeft;
		}	
		else
		{
			if (leftTotalCount)
				countOnLeft = totalCount;
			else
				countOnRight = totalCount;
		}
		if ((countOnLeft > 0) && (countOnRight > 0))
		{
			m_decodeFinished = false;
			if (m_splitCounter[symbol] < m_uniqueSplitCounter)
				m_splitCounter[symbol] = m_uniqueSplitCounter;	// record that this symbol split between two or more child contexts
		}

		if (countOnLeft)
		{
			leftTotalCount -= countOnLeft;
			leftSymbolStats[numSymbolsOnLeft].m_count = countOnLeft;
			leftSymbolStats[numSymbolsOnLeft].m_symbol = symbol;
			leftSymbolStats[numSymbolsOnLeft].m_firstIndex = contextSymbolStats[i].m_firstIndex;
			numSymbolsOnLeft++;
		}
		if (countOnRight)
		{
			rightTotalCount -= countOnRight;
			rightSymbolStats[numSymbolsOnRight].m_count = countOnRight;
			rightSymbolStats[numSymbolsOnRight].m_symbol = symbol;
			rightSymbolStats[numSymbolsOnRight].m_firstIndex = (contextSymbolStats[i].m_firstIndex + countOnLeft);
			numSymbolsOnRight++;
		}	
	}

	unsigned int localTrend = trend;

	// decode symbol distribution below left half
	DecodeChildContextStats(leftSymbolStats, numSymbolsOnLeft, subContextLengths, numContextsOnLeft, localTrend, recursionDepth);

	// decode symbol distribution below right half
	DecodeChildContextStats(rightSymbolStats, numSymbolsOnRight, subContextLengths + numContextsOnLeft, numContextsOnRight, localTrend, recursionDepth);
}



unsigned int M03_Decoder::WriteContextInfo(unsigned char * pAddress, unsigned int symbol, unsigned int count, unsigned int index)
{
	// write symbol information at current address
	if (symbol == 0)
	{
		// write the special sentinel symbol
		pAddress[0] = 0;
		pAddress[1] = 0;
		return 2;
	}

	unsigned char * pStartAddress = pAddress;
	*(pAddress++) = (symbol - 1);
	if (count > 0x3f)
	{
		// four byte count required
		*(unsigned int *)pAddress = ((count << 1) | 1);
		pAddress += 4;
	}
	else
	{
		// 1 byte count required
		*(pAddress++) = (((unsigned char)count) << 1);
	}

	if (count > 1)
	{
		// index required
		*(unsigned int *)pAddress = index;
		pAddress += 4;
	}
	unsigned int ret = (unsigned int)(pAddress - pStartAddress);
	return ret;
}




unsigned int M03_Decoder::ReadContextInfo(unsigned char * pAddress, unsigned int & symbol, unsigned int & count, unsigned int & index)
{
	// read symbol information from current address
	unsigned char * pStartAddress = pAddress;

	symbol = *(pAddress++);
	symbol++;
	unsigned char countSizeFlag = (pAddress[0] & 0x01);
	count = *(unsigned int *)pAddress;
	if (countSizeFlag == 0)
	{
		// 1 byte count
		count = (unsigned char)count;
		pAddress += 1;
	}
	else
	{
		// four byte count
		pAddress += 4;
	}
	count >>= 1;

	if (count == 0)
	{
		// special indicator of sentinel
		count = 1;
		symbol = 0;
	}
	if (count > 1)
	{
		// stardard symbol with count greater than 1
		index = *(unsigned int *)pAddress;
		pAddress += 4;
	}
	else
	{
		index = ~0;
	}
	return (unsigned int)(pAddress - pStartAddress);
}




void M03_Decoder::DecodeInitialSymbolCounts(unsigned int * symbolCount, unsigned int numSymbols, unsigned int totalCount)
{
	if (numSymbols < 2)
	{
		symbolCount[0] = totalCount;
		return;
	}

	unsigned int numSymbolsOnLeft = (numSymbols >> 1);
	unsigned int numSymbolsOnRight = (numSymbols - numSymbolsOnLeft);
	// decode distribution of symbols on left vs right
	unsigned int trend = 0;
	unsigned int totalOnLeft = DecodeValue(totalCount, totalCount, totalCount, numSymbols, trend);
	// decode symbol distribution below left half
	DecodeInitialSymbolCounts(symbolCount, numSymbolsOnLeft, totalOnLeft);
	// decode symbol distribution below right half
	DecodeInitialSymbolCounts(symbolCount + numSymbolsOnLeft, numSymbolsOnRight, totalCount - totalOnLeft);
}




unsigned int M03_Decoder::DecodeValue(unsigned int maxValue, unsigned int listCountA, 
									  unsigned int listCountB, unsigned int numSymbols,
									  unsigned int & trend)
{
	unsigned int assumedInA = 0;
	unsigned int assumedInB = 0;
	unsigned int forcedDistribution = 0;

	if (maxValue > listCountA)
	{
		assumedInB = maxValue - listCountA;
		maxValue -= assumedInB;
		listCountB -= assumedInB;
		forcedDistribution |= 1;
	}
	if (maxValue > listCountB)
	{
		assumedInA = maxValue - listCountB;
		maxValue -= assumedInA;
		listCountA -= assumedInA;
		forcedDistribution |= 2;
	}

	if (maxValue == 0)
		return assumedInA;

	unsigned int ratio = (listCountA << SIGN_MODEL_RATIO_BITS) / (listCountB + listCountA);
	unsigned int freqA, freqB, freqC, totalFreq;
	unsigned int primaryModelIndex, secondaryModelIndex;
	GetSignProb(numSymbols, maxValue, ratio, forcedDistribution, freqA, freqB, freqC, totalFreq, primaryModelIndex, secondaryModelIndex);

	unsigned int distribution = 0;

	// decode sign
	unsigned int w = m_pRangeDecoder->decode_culfreq(totalFreq);
	if (w >= freqA)
		distribution = (w >= (freqA + freqB)) ? 2 : 1;

	if (distribution == 2)
	{
		// decoded an even split.
		m_pRangeDecoder->decode_update(freqC, freqA + freqB, totalFreq);
		AdjustSignModel(primaryModelIndex + 2, secondaryModelIndex + 2);
		return (maxValue >> 1) + assumedInA;
	}

	if (distribution == 0)
	{
		// decoded that left is larger than right
		m_pRangeDecoder->decode_update(freqA, 0, totalFreq);
		AdjustSignModel(primaryModelIndex, secondaryModelIndex);
	}
	else
	{
		// decode that right is larger than left.
		m_pRangeDecoder->decode_update(freqB, freqA, totalFreq);
		AdjustSignModel(primaryModelIndex + 1, secondaryModelIndex + 1);
	}

	unsigned int maxDif;
	if (maxValue & 0x01)
		maxDif = (maxValue >> 0x01);
	else
		maxDif = ((maxValue - 0x01) >> 0x01);
	unsigned int actualDif = 0x00;

	if (maxDif)
	{
		actualDif = DecodeNumber(maxDif, ratio, trend);
		AdjustTrend(actualDif, maxDif, trend);
	}

	if (distribution == 0)
	{
		// majority left
		if (maxValue & 1)
			return (assumedInA + ((maxValue + 1) >> 1) + actualDif);	// odd maxDif
		else
			return (assumedInA + (maxValue >> 1) + actualDif + 1);		// even maxDif
	}
	else
	{
		// majority right
		if (maxValue & 1)
			return (assumedInA + (maxValue >> 1) - actualDif);			// odd maxDif
		else
			return (assumedInA + (maxValue >> 1) - (actualDif + 1));	// even maxDif
	}
}




unsigned int M03_Decoder::DecodeNumber(unsigned int maxValue, unsigned int ratio,
									unsigned int & trend, unsigned int lastBit, unsigned int depth)
{
	unsigned int freqA;
	unsigned int freqB;
	unsigned int totalFreq;

	unsigned int primaryModelIndex, secondaryModelIndex;
	GetBitModelProb(ratio, depth, maxValue, lastBit, trend, freqA, freqB, totalFreq, primaryModelIndex, secondaryModelIndex);
	unsigned int min = (maxValue >> 1);

	unsigned int w = m_pRangeDecoder->decode_culfreq(totalFreq);
	if (w < freqA)
	{
		m_pRangeDecoder->decode_update(freqA, 0, totalFreq);
		AdjustBitModel(primaryModelIndex, secondaryModelIndex);
		maxValue = min;
		if (maxValue)
			return DecodeNumber(maxValue, ratio, trend, (lastBit << 1), depth + 1);
		return 0;
	}
	else
	{
		m_pRangeDecoder->decode_update(freqB, freqA, totalFreq);
		maxValue -= (min + 1);
		AdjustBitModel(primaryModelIndex + 1, secondaryModelIndex + 1);
		if (maxValue)
			return (min + 1 + DecodeNumber(maxValue, ratio, trend, (lastBit << 1) | 1, depth + 1));
		return (min + 1);
	}
}
