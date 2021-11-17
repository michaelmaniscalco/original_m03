#include "M03_Encoder.h"
#include "MSufSort.h"
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <iomanip>
#include "time.h"
#include "QuickSort7.h"


using namespace std;



M03_Encoder::M03_Encoder(RangeEncoder * pRangeEncoder)
{
	m_pRangeEncoder = pRangeEncoder;
	memset(m_parentContextSymbolCount, 0, sizeof(unsigned int) * MAX_SYMBOL);
	m_parentContextSymbolListSize = 0;
}




M03_Encoder::~M03_Encoder()
{
}




void M03_Encoder::Encode(unsigned char * data, unsigned int size, unsigned int saSentinel)
{
	if (saSentinel == ~0)
	{
		boost::scoped_ptr<MSufSort> spMSufSort(new MSufSort);

		// now do the BWT
		int start = clock();
		unsigned int sentinelIndex;
		spMSufSort->BWT((unsigned char *)data, (unsigned int)size, sentinelIndex);
		m_sentinelIndex = sentinelIndex;
		int finish = clock();
		spMSufSort.reset();
	}
	else
	{
		m_sentinelIndex = saSentinel;
	}

	m_inputSize = size + 1;	
	m_pData = data;
	m_pEndData = m_pData + m_inputSize;

	// calculate the starting index for each order 0 context
	unsigned int symbolCount[MAX_SYMBOL];
	unsigned int startingIndex[MAX_SYMBOL];
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		symbolCount[i] = 0;
		startingIndex[i] = 0;
	}
	symbolCount[0] = 1;
	for (unsigned int i = 0; i < size; i++)
		symbolCount[(int)data[i] + 1]++;

	// encode the symbol frequencies
	unsigned int t[0x100];
	unsigned int t2 = 0;
	for (unsigned int i = 1; i < MAX_SYMBOL; i++)
	{
		m_pRangeEncoder->encode_bits((symbolCount[i] != 0), 1);
		if (symbolCount[i] != 0)
			t[t2++] = symbolCount[i];
	}
	EncodeInitialSymbolCounts(t, t2);

	unsigned int total = 0;
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		startingIndex[i] = total;
		total += symbolCount[i];
	}

	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		m_localizedRecord[i].m_hasIndexCounter = 0;
		m_localizedRecord[i].m_subContextCounter = 0;
	}
	m_counter = 0;

	GenerateRunLengthInfo(data, size);
	// Load RLE lookup table
//	InitializeRleBuffer(data, size, m_sentinelIndex);
//	free(m_pData);
//	m_pData = 0;
//	int startRleTest = clock();
//	bool rleValidated = TestRle(size);
//	int finishRleTest = clock();
//
//	std::cout << std::endl << "RLE test completed in " << ((double)(finishRleTest - startRleTest) / CLOCKS_PER_SEC) << " seconds " << std::endl;
//	if (!rleValidated)
//		std::cout << "RLE error detected" << std::endl;
//	else
//		std::cout << "RLE validated" << std::endl;
//	delete [] m_pRleBuffer;
//	std::cout << std::endl;


	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		m_subContextSymbolListSize[i] = 0;
		for (unsigned int j = 0; j < MAX_SYMBOL; j++)		
			m_subContextSymbolCount[i][j] = 0;
	}

	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
		m_localizedRecord[i].m_symbolCount = 0;
	m_symbolListSize = 0;

	// load offset information for each symbol
	m_symbolFirstIndex = (unsigned short *)(data + 2);
	unsigned int oddEven[MAX_SYMBOL];
	unsigned int sCount[MAX_SYMBOL];
	memset(sCount, 0, MAX_SYMBOL * 4);
	memset(oddEven, 0, MAX_SYMBOL * 4);
	bool sentinelCounted = false;
	const unsigned char * pSymbol = data;
	unsigned char * pWrite = data + 2;
	for (unsigned int i = 0; i <= size; i++)
	{
		unsigned int symbol;
		if ((i == m_sentinelIndex) && (!sentinelCounted))
		{
			sentinelCounted = true;
			symbol = 0;
		}
		else
		{
			symbol = pSymbol[i << 2];
			symbol++;
		}	
		unsigned int n = (oddEven[symbol]++ & 0x01);
		if (n)
		{
			*(unsigned short *)pWrite = ((sCount[symbol] & 0x7fff) | 0x8000);
			sCount[symbol] += 2;
		}
		else
		{
			*(unsigned short *)pWrite = ((sCount[symbol] >> 15) & 0x7fff);
		}
		pWrite += 4;
	}

	// set up the partition queues with the initial sub context boundaries
	m_singletonCount = 0;
	m_numUniqueSymbols = 0;
	total = 0;
	realloc(data, ((size + 1) << 2));

	//char * pStack = (char *)(data + ((size + 1) << 2));
	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		m_symbolStart[i] = total;
		if (i == 0)
		{
			size_t sizeRequired = m_queue[i].Allocate(new char[4096 + 4096 + 1024], 4096 + 4096);//pStack, 4096);
		//	pStack += sizeRequired;
		}	
		else
		{
			size_t sizeRequired = m_queue[i].Allocate(new char[(symbolCount[i] << 1) + 4096], (symbolCount[i] << 1));
		//	pStack += sizeRequired;
		}
		if (symbolCount[i])
		{
			m_queue[0].Push(symbolCount[i], (i == 0) ? START_CONTEXT : 0);
			m_uniqueSymbolList[m_numUniqueSymbols++] = i;
		}
		total += symbolCount[i];
	}

	m_singletonCount = 0;
	unsigned int maxOrder = ~0;
	m_currentSymbol = 0;
	m_currentRunLength = 0;
	m_subContextCounter = 0;


	unsigned int lastSize = 0;
	unsigned int currentOrder = 0;

	do 
	{
		ProcessNextOrder();
		currentOrder++;
	} while (m_singletonCount < (size + 1));

	for (unsigned int i = 0; i < MAX_SYMBOL; i++)
	{
		delete [] m_queue[i].Get();
	}
}






void M03_Encoder::ProcessNextOrder()
{
	// iterate across all contexts of the next highest order
	const char * queueBack[MAX_SYMBOL];

	for (unsigned int i = 0; i < m_numUniqueSymbols; i++)
	{
		unsigned int s = m_uniqueSymbolList[i];
		queueBack[s] = m_queue[s].GetBack();
		m_localizedRecord[s].m_currentPushIndex = m_symbolStart[s];
	}

	for (unsigned int i = 0; i < m_numUniqueSymbols; i++)
	{
		m_currentSymbol = m_uniqueSymbolList[i];
		m_currentIndex = m_symbolStart[m_currentSymbol];
		M03_Queue * pQueue = &m_queue[m_currentSymbol];
		const char * pFinished = queueBack[m_currentSymbol];
		m_currentRunLength = 0;
		while (pQueue->GetFront() != pFinished)
		{
			if (pQueue->Flags() & SKIP)
			{
				m_currentIndex += pQueue->Front();
				pQueue->Pop();
				m_currentRunLength = 0;	
			}
			ProcessNextContext();
		}
	}
}




void M03_Encoder::ProcessNextContext()
{
	M03_Queue * pQueue = &m_queue[m_currentSymbol];
	SubContextStat subContextStats[MAX_SYMBOL];
	unsigned int numSubContexts = 0;
	unsigned int totalContextLength = 0;
	unsigned int j = 0;
	do 
	{
		unsigned int n = pQueue->Front();
		pQueue->Pop();
		totalContextLength += n;
		subContextStats[numSubContexts].index = j++;
		subContextStats[numSubContexts++].size = n;
	} while ((!pQueue->Empty()) && (!pQueue->Flags()));

	IncrementContextCounter();
	IncrementSubContextCounter();
	unsigned int endCurrentContext = m_currentIndex + totalContextLength;

	for (unsigned int subContextIndex = 0; subContextIndex < numSubContexts; subContextIndex++)
	{
		// pop length of next sub-context
		unsigned int subContextLength = subContextStats[subContextIndex].size;

		if (subContextLength == 1)
			m_singletonCount++;

		unsigned int * pSymbol = m_symbolList;
		unsigned int * pEndSymbol = pSymbol + m_symbolListSize;
		do
		{
			// get size of run for next symbol in sub-context
			if (m_currentRunLength == 0)
				GetSymbolAndRunLength(m_currentIndex, m_currentRunSymbol, m_currentRunLength, subContextLength);
			unsigned int runLength = m_currentRunLength;
			if (runLength > subContextLength)
				runLength = subContextLength;
			m_currentRunLength -= runLength;

			// check if we have the first index of the first occurrence of this symbol in the parent context
			if (m_localizedRecord[m_currentRunSymbol].m_hasIndexCounter < m_counter)
				GetIndex(runLength);

			if ((m_localizedRecord[m_currentRunSymbol].m_symbolCount += runLength) == runLength)
			{
				// first time for this symbol in this sub context
				*(pEndSymbol++) = m_currentRunSymbol;
				if (m_localizedRecord[m_currentRunSymbol].m_subContextCounter <= m_subContextCounter)
				{
					if (m_localizedRecord[m_currentRunSymbol].m_subContextCounter < m_subContextCounter)
						m_localizedRecord[m_currentRunSymbol].m_subContextCounter = m_subContextCounter;
					else
						m_localizedRecord[m_currentRunSymbol].m_subContextCounter++;
				}
			}
			subContextLength -= runLength;
			m_currentIndex += runLength;
		} while (subContextLength);

		unsigned int * subContextSymbolCount = m_subContextSymbolCount[subContextIndex];
		while (pSymbol < pEndSymbol)
		{
			unsigned int symbol = *(pSymbol++);
			m_subContextSymbolList[subContextIndex][m_subContextSymbolListSize[subContextIndex]++] = symbol;
			unsigned int count = m_localizedRecord[symbol].m_symbolCount;
			m_subContextSymbolCount[subContextIndex][symbol] = count;
			m_localizedRecord[symbol].m_symbolCount = 0;

			if (m_parentContextSymbolCount[symbol] == 0)
				m_parentContextSymbolList[m_parentContextSymbolListSize++] = symbol;
			m_parentContextSymbolCount[symbol] += count;

			if (m_localizedRecord[symbol].m_subContextCounter > m_subContextCounter)
			{
				// symbol is distributed between two or more sub contexts.
				if (m_localizedRecord[symbol].m_hasIndexCounter == m_counter)
				{
					// first split between two sub contexts
					unsigned int firstSubContextCount = m_localizedRecord[symbol].m_firstSubContextCount;
					unsigned int skipLength = (m_localizedRecord[symbol].m_index - m_localizedRecord[symbol].m_currentPushIndex);
					if (skipLength)
					{
						// add skip value
						if (skipLength >= m_inputSize)
						{
							cout << "SKIP COUNT ERROR.  symbol = " << symbol << endl;
						}
						m_queue[symbol].Push(skipLength, SKIP);
						m_localizedRecord[symbol].m_currentPushIndex += skipLength;					
					}

					m_localizedRecord[symbol].m_hasIndexCounter++;
					m_queue[symbol].Push(firstSubContextCount, START_CONTEXT);
					m_queue[symbol].Push(count, 0);
					m_localizedRecord[symbol].m_currentPushIndex += (count + firstSubContextCount);
				}
				else
				{
					// symbol is distributed between three or more sub contexts.
					m_queue[symbol].Push(count, 0);
					m_localizedRecord[symbol].m_currentPushIndex += count;
				}
			}
			else
			{
				// first sub context to contain this symbol.  no more work unless
				// this same symbol is also distributed to at least one other sub context.
				m_localizedRecord[symbol].m_firstSubContextCount = count;
			}
		}
	}

	// all sub contexts counted. Encode distribution of symbols from parent context to sub contexts.
	EncodeSymbolDistribution(subContextStats, 0, numSubContexts, 0, 0);

	for (unsigned int i = 0; i < numSubContexts; i++)
	{
		unsigned int index = subContextStats[i].index;
		for (unsigned int j = 0; j < m_subContextSymbolListSize[index]; j++)
			m_subContextSymbolCount[index][m_subContextSymbolList[index][j]] = 0;
		m_subContextSymbolListSize[index] = 0;
	}

	for (unsigned int i = 0; i < m_parentContextSymbolListSize; i++)
		m_parentContextSymbolCount[m_parentContextSymbolList[i]] = 0;
	m_parentContextSymbolListSize = 0;
}



void M03_Encoder::EncodeInitialSymbolCounts(unsigned int * symbolCount, unsigned int numSymbols)
{
	if (numSymbols < 2)
		return;

	// count total on left half
	unsigned int numSymbolsOnLeft = (numSymbols >> 1);
	unsigned int leftTotalCount = 0;
	for (unsigned int i = 0; i < numSymbolsOnLeft; i++)
		leftTotalCount += symbolCount[i];

	// count total on right half
	unsigned int numSymbolsOnRight = (numSymbols - numSymbolsOnLeft);
	unsigned int rightTotalCount = 0;
	for (unsigned int i = numSymbolsOnLeft; i < numSymbols; i++)
		rightTotalCount += symbolCount[i];

	// encode distribution of symbols on left vs right
	unsigned int totalCount = (leftTotalCount + rightTotalCount);
	unsigned int trend = 0;
	EncodeValue(leftTotalCount, rightTotalCount, totalCount, totalCount, numSymbols, trend);

	// encode symbol distribution below left half
	if (numSymbolsOnLeft > 1)
		EncodeInitialSymbolCounts(symbolCount, numSymbolsOnLeft);

	// encode symbol distribution below right half
	if (numSymbolsOnRight > 1)
		EncodeInitialSymbolCounts(symbolCount + numSymbolsOnLeft, numSymbolsOnRight);
}




void M03_Encoder::EncodeSymbolDistribution(SubContextStat * subContextStats, 
							 unsigned int firstSubContext, unsigned int lastSubContext,
							 unsigned int trend, unsigned int recursionDepth)
{
	if ((lastSubContext - firstSubContext) < 2)
		return;

	recursionDepth++;

	static unsigned int counter = 0;
	static unsigned int leftSymbolCountInitialized[MAX_SYMBOL];
	static unsigned int rightSymbolCountInitialized[MAX_SYMBOL];
	if (counter == 0)
	{
		memset(leftSymbolCountInitialized, 0, sizeof(unsigned int) * MAX_SYMBOL);
		memset(rightSymbolCountInitialized, 0, sizeof(unsigned int) * MAX_SYMBOL);
	}
	counter++;

	// count all symbols in left half
	unsigned int numSubContexts = lastSubContext - firstSubContext;
	unsigned int numContextsOnLeft = ((numSubContexts + 1) >> 1);

	unsigned int parentSymbolListSize = 0;
	unsigned int parentSymbolList[MAX_SYMBOL];
	unsigned int leftSymbolCount[MAX_SYMBOL];
	unsigned int rightSymbolCount[MAX_SYMBOL];
	unsigned int leftTotalCount = 0;
	for (unsigned int j = firstSubContext; j < (firstSubContext + numContextsOnLeft); j++)
	{
		unsigned int subContextIndex = subContextStats[j].index;
		unsigned int numSymbolsInSubContext = m_subContextSymbolListSize[subContextIndex];
		for (unsigned int i = 0; i < numSymbolsInSubContext; i++)
		{
			unsigned int symbol = m_subContextSymbolList[subContextIndex][i];
			unsigned int count = m_subContextSymbolCount[subContextIndex][symbol];
			leftTotalCount += count;
			if (leftSymbolCountInitialized[symbol] != counter)
			{
				leftSymbolCountInitialized[symbol] = counter;
				leftSymbolCount[symbol] = count;
				rightSymbolCount[symbol] = 0;
				parentSymbolList[parentSymbolListSize++] = symbol;
			}
			else
			{
				leftSymbolCount[symbol] += count;
			}
		}
	}

	// count all symbols in right half
	unsigned int numContextsOnRight = ((lastSubContext - firstSubContext) - numContextsOnLeft);

	unsigned int rightTotalCount = 0;
	for (unsigned int j = (firstSubContext + numContextsOnLeft); j < lastSubContext; j++)
	{
		unsigned int subContextIndex = subContextStats[j].index;
		unsigned int numSymbolsInSubContext = m_subContextSymbolListSize[subContextIndex];
		for (unsigned int i = 0; i < numSymbolsInSubContext; i++)
		{
			unsigned int symbol = m_subContextSymbolList[subContextIndex][i];
			unsigned int count = m_subContextSymbolCount[subContextIndex][symbol];
			rightTotalCount += count;
			if (rightSymbolCountInitialized[symbol] != counter)
			{
				rightSymbolCountInitialized[symbol] = counter;
				rightSymbolCount[symbol] = count;
				if (leftSymbolCountInitialized[symbol] != counter)
				{
					leftSymbolCount[symbol] = 0;
					parentSymbolList[parentSymbolListSize++] = symbol;
				}
			}
			else
			{
				rightSymbolCount[symbol] += count;
			}
		}
	}



	// encode distribution of symbols on left vs right
	SymbolStat symbolStat[MAX_SYMBOL];
	unsigned int totalCount = 0;
	for (unsigned int i = 0; i < parentSymbolListSize; i++)
	{
		unsigned int symbol = parentSymbolList[i];
		unsigned int leftCount = (leftSymbolCountInitialized[symbol] == counter) ? leftSymbolCount[symbol] : 0;
		unsigned int rightCount = (rightSymbolCountInitialized[symbol] == counter) ? rightSymbolCount[symbol] : 0;
		symbolStat[i].count = leftCount + rightCount;
		symbolStat[i].symbol = parentSymbolList[i];
		totalCount += symbolStat[i].count;
	}

	QuickSort7(symbolStat, parentSymbolListSize);
	for (unsigned int i = 0; i < parentSymbolListSize; i++)
		parentSymbolList[i] = symbolStat[i].symbol;

	if (parentSymbolListSize > 8)
		std::reverse(parentSymbolList, parentSymbolList + parentSymbolListSize);

	unsigned int numSymbolsOnLeft = 0;
	unsigned int numSymbolsOnRight = 0;

	for (unsigned int i = 0; i < parentSymbolListSize; i++)
	{
		unsigned int symbol = parentSymbolList[i];
		unsigned int leftCount = (leftSymbolCountInitialized[symbol] == counter) ? leftSymbolCount[symbol] : 0;
		unsigned int rightCount = (rightSymbolCountInitialized[symbol] == counter) ? rightSymbolCount[symbol] : 0;
		unsigned int totalCount = leftCount + rightCount;

		if (leftCount)
			numSymbolsOnLeft++;
		if (rightCount)
			numSymbolsOnRight++;
		if ((leftTotalCount) && (rightTotalCount))
		{
			EncodeValue(leftCount, rightCount, leftTotalCount, rightTotalCount, parentSymbolListSize - i, trend);
			leftTotalCount -= leftCount;
			rightTotalCount -= rightCount;
		}	
	}

	unsigned int localTrend = trend;

	// encode symbol distribution below left half
	if ((numContextsOnLeft > 1) && (numSymbolsOnLeft > 1))
		EncodeSymbolDistribution(subContextStats, firstSubContext, firstSubContext + numContextsOnLeft, localTrend, recursionDepth);
	else
		for (unsigned int i = 0; i < parentSymbolListSize; i++)
		{
			unsigned int symbol = parentSymbolList[i];
			unsigned int leftCount = leftSymbolCount[symbol];
			if (leftCount)
				m_localizedRecord[symbol].m_index += leftCount;
		}

	// encode symbol distribution below right half
	if ((numContextsOnRight > 1) && (numSymbolsOnRight > 1))
		EncodeSymbolDistribution(subContextStats, firstSubContext + numContextsOnLeft, lastSubContext, localTrend, recursionDepth);
	else
		for (unsigned int i = 0; i < parentSymbolListSize; i++)
		{
			unsigned int symbol = parentSymbolList[i];
			unsigned int rightCount = rightSymbolCount[symbol];
			if (rightCount)
				m_localizedRecord[symbol].m_index += rightCount;
		}

/*
	static unsigned int yes = 0;
	static unsigned int no = 0;
	if (parentSymbolListSize == numSubContexts)
	{
		QuickSort7(symbolStat, parentSymbolListSize);
		QuickSort7(subContextStats, numSubContexts);
		bool same = true;
		bool greaterThanOne = false;
		for (size_t i = 0; i < numSubContexts; i++)
		{
			if (subContextStats[i].size > 1)
				greaterThanOne = true;
			if (symbolStat[i].count != subContextStats[i].size)
				same = false;
			if ((i > 0) && (subContextStats[i].size == subContextStats[i - 1].size))
				same = false;
		}
		if (same)
		{
			if (totalskew)
				yes++;
			else
				no++;
			std::cout << "YES = " << yes << "  NO = " << no << std::endl;
		}
	}
*/
}



unsigned int M03_Encoder::CountUniqueSymbols(unsigned int index, unsigned int length)
{
	unsigned int ret = 0;
	static unsigned int counter = 0;
	static unsigned int symbolSeen[MAX_SYMBOL];
	if (counter == 0)
	{
		for (unsigned int i = 0; i < MAX_SYMBOL; i++)
			symbolSeen[i] = 0;

	}
		counter++;
	bool sentinelSeen = false;
	unsigned char * pData = m_pData + (index << 2);
	for (unsigned int i = 0; i < length; i++)
	{
		if (((index + i) == m_sentinelIndex) && (sentinelSeen = true))
		{
			sentinelSeen = true;
			ret++;
		}
		else
		{
			if (symbolSeen[*pData] != counter)
			{
				ret++;
				symbolSeen[*pData] = counter;
			}

		}
		pData += 4;	
	}

	return ret;
}




void M03_Encoder::InitializeRleBuffer(const symbol_type * pData, size_t szData, sentinel_index sentinelIndex)
{
	//===================================================================================================
	// RLE 
	//
	// For faster counting of symbol runs an array of size 1N is arranged such that the majority of
	// longer runs can be counted with a quick look up rather than counting and re-counting the
	// runs repeatedly with each higher order context.  This typically speeds up encoding for any
	// data with long runs and does not slow down encoding of data which does not have such runs very
	// much at all.
	//
	// Approach: Data is arranged replacing the original data (1N space) using an additional 1N space
	// such that every two bytes contains a symbol and some amount of RLE information in the second 
	// byte.  Runs can be no longer than (2^16) - 1 in length and longer runs are broken into multiple
	// runs of no longer than (2^16) - 1 each.
	//
	// Data layout: Each index into the original data can be located at 2k where k is the index of the
	// data.  Byte [2k] is the symbol type and byte [2k+1] is the partial RLE data.
	// The partial RLE data is considered to be a run length of 1 if the most significant bit [MSB] of [2k+1]
	// is set to zero regardless of what data is contained in this byte.  If the MSB is set to one then
	// this location is the first of four successive partial RLE info structs (located at 2k+1, 2k+3,
	// 2k+5, 2k+7) which, when combined, indicate a run length for the current symbol which is up to
	// (2^16) - 1 in length (0xfffc).  This run length is distributed in the four least significant bits (LSB)
	// of each of the bytes 2k+1, 2k+3, 2k+5, 2k+7.  The remaining three bits of each byte at 2k+1, 2k+3, 2k+5
	// and 2k+7 are reserved and provide 3 bits per symbol of storage which can be directly indexed during
	// encoding and is used to improve statistical modeling and compression.
	// 
	// Example: if a run of length 1234 (0x04D2) for symbol 'a' began at index k=53 of the data then 
	// this run would be indicated starting at [2k] as follows:
	// ['a'][0x80]['a'][0x04]['a'][0x0D]['a'][0x02]
	// The run would then be written again at k = 57 but with a length which is 4 less (1230).
	// ['a'][0x80]['a'][0x04]['a'][0x0C]['a'][0x0E]
	// This would continue with each k + 4 and length - 4 until length becomes less than 4.
	// 
	// Conclusion: with this approach the run length of any symbol starting at any position can be gathered
	// quickly by examining the MSB of no more than 4 bytes starting at k+1,k+3,k+5 and k+7.
	// Since the BWT often contains long runs and these runs must be recalculated from various starting positions
	// repeatedly with each high order context modeling, this approach (for and additional 1N space) can radically
	// speed up counting the number of symbols in any given context starting at any given position in the data.
	//=============================================================================================================

	m_pRleBuffer = new symbol_type[((szData + 1) << 1) + 1];
	memset(m_pRleBuffer, 0x00, ((szData + 1) << 1) + 1);
	m_pRleBuffer[((szData + 1) << 1)] = (pData[szData - 1] + 1); // trick to make sure run at end of buffer stops there.
	const symbol_type * pEndData = pData + szData;
	const symbol_type * pSentinel = pData + sentinelIndex;
	m_pSentinel = (m_pRleBuffer + (sentinelIndex << 1));
	symbol_type * pCur = m_pRleBuffer;
	symbol_type * pEnd = pCur + ((szData + 1) << 1);

	while ((pCur < pEnd) || (pSentinel != 0))
	{
		if (pData == pSentinel)
		{
			symbol_type s = (sentinelIndex != 0) ? (pData[-1] + 1) : 0;
			if (pData[0] == s)
				s++;
			*(pCur++) = s;
			*(pCur++) = 0x00;	// zero length (zero length indicates sentinel)
			pSentinel = 0;		// to prevent reuse of sentinel 
		}
		else
		{
			// count the run length for the next symbol
			const symbol_type * pStart = pData;
			symbol_type currentSymbol = *pData;
			while ((pData < pEndData) && (pData != pSentinel) && (*pData == currentSymbol))
				pData++;
			size_t runLength = (size_t)(pData - pStart);
			while (runLength > 0x03)
			{	
				size_t lengthToWrite = runLength;
				if (lengthToWrite > 0xffff)
					lengthToWrite = 0xffff;
				*(pCur++) = currentSymbol;
				*(pCur++) = (0x80 | ((lengthToWrite >> 12) & 0x0f));
				*(pCur++) = currentSymbol;
				*(pCur++) = ((lengthToWrite >> 8) & 0x0f);
				*(pCur++) = currentSymbol;
				*(pCur++) = ((lengthToWrite >> 4) & 0x0f);
				*(pCur++) = currentSymbol;
				*(pCur++) = (lengthToWrite & 0x0f);
				runLength -= 4;
			}

			while (runLength)
			{
				*(pCur++) = currentSymbol;
				*(pCur++) = runLength;
				runLength--;
			}
		}
	}
}

	
void M03_Encoder::GetRleInfo(suffix_index index, symbol_type & symbolType, size_t & runLength)
{
	if (index == m_sentinelIndex)
	{
		symbolType = 0;
		runLength = 0;	// run length of zero indicates -1 symbol (sentinel)
		return;
	}

	const symbol_type * pReadAddress = m_pRleBuffer + (index << 1);

	symbolType = *pReadAddress;
	runLength = 0;

	while (pReadAddress[0] == symbolType) 
	{
		if ((pReadAddress[1] & 0x80) == 0x80)
		{
			// run length info here ...
			size_t r = (pReadAddress[1] & 0x0f);
			r <<= 4;
			r |= (pReadAddress[3] & 0x0f);
			r <<= 4;
			r |= (pReadAddress[5] & 0x0f);
			r <<= 4;
			r |= (pReadAddress[7] & 0x0f);
			runLength += r;
			pReadAddress += (r << 1);
		}
		else
		{
			runLength++;
			pReadAddress += 2;
		}
	}
}


bool M03_Encoder::TestRle(size_t size)
{
	// TEST function for demonstrating the efficiency of the fast RLE scheme vs
	// counting each run length manually.
	// Test has switch to test only the fast method, only the manual method and
	// a verify feature if both methods are tested.
	// Test calculates the run length starting at each position in the string.

//	#define DO_MANUAL_RLE_SPEED_TEST
//	#define DO_OLDER_FAST_SPEED_TEST
	#define DO_FAST_RLE_SPEED_TEST
	#define VERIFY_TEST
	#define DISPLAY_PROGRESS

	const symbol_type * pCur = m_pRleBuffer;
	const symbol_type * pEnd = m_pRleBuffer + (size << 1);
	suffix_index index = 0;
	bool error = false;

	#ifdef DISPLAY_PROGRESS
		std::cout << "Beginning RLE test" << (char)13;
	#endif

	size_t percent = (size / 100);
	while (pCur < pEnd)
	{
		#ifdef DISPLAY_PROGRESS
			if ((index % percent) == 0)
				std::cout << (size_t)((index * 100) / size) << "% completed    " << (char)13;
		#endif

		#ifdef DO_MANUAL_RLE_SPEED_TEST
			// count run length manually
			symbol_type s = *pCur;
			const symbol_type * pStart = pCur;
			size_t r;
			if (m_pSentinel == pCur)
			{
				r = 1;	// sentinel
			}
			else
			{
				pCur += 2;
				while ((pCur < pEnd) && (pCur != m_pSentinel) && (*pCur == s))
					pCur += 2;	
				r = (size_t)(pCur - pStart) >> 1;
			}
			pCur = pStart;
		#endif // DO_MANUAL_RLE_SPEED_TEST

		#ifdef DO_OLDER_FAST_SPEED_TEST
			unsigned int s;
			unsigned int r;
			GetSymbolAndRunLength(index, s, r, ~0);
		#endif

		#ifdef DO_FAST_RLE_SPEED_TEST
			// count run length via RLE buffer
			size_t rleSize = 0;
			symbol_type rleSymbol;
			GetRleInfo(index, rleSymbol, rleSize);
		#endif

		#ifdef VERIFY_TEST 
		#ifdef DO_FAST_RLE_SPEED_TEST 
		#ifdef DO_MANUAL_RLE_SPEED_TEST
			// verify
			if (r != rleSize)
				error = true;
			if (s != rleSymbol)
				error = true;
		#endif 
		#endif
		#endif

		// advance to next index
		index++;
		pCur += 2;
	} 

	std::cout << "                                    " << (char)13;
	return !error;
}

