#include "M03_Encoder2.h"
#include "MSufSort.h"
#include <boost/shared_ptr.hpp>


using namespace M03;

typedef unsigned int	uint32;
typedef unsigned short	uint16;
typedef unsigned char	uint8;


// ======= queue class

void Queue::Initialize(char * pAddress, size_t size, size_t maxIndex)
{
	m_pAddress = pAddress;
	m_pEndAddress = (pAddress + size);
	m_pRead = pAddress;
	m_pWrite = pAddress;

	Write(maxIndex, SKIP);	// initialize as single large 'skip' partition
}


void Queue::Push(size_t size, PARITION_FLAGS flags)
{
}


void Queue::Pop(size_t & size, PARITION_FLAGS & flags)
{
}


void Queue::Write(size_t size, PARITION_FLAGS flags)
{
	if (size > 0xffff)
	{
		// four bytes
		*(m_pWrite++) = (SIZE_4 | flags);
		*(uint32 *)(m_pWrite) = (uint32)size;
		m_pWrite += 4;
	}
	else
	{
		if (size > 0xff)
		{
			// two bytes
			*(m_pWrite++) = (SIZE_2 | flags);
			*(uint16 *)(m_pWrite) = (uint16)size;
			m_pWrite += 2;
		}
		else
		{
			// one byte
			*(uint8 *)(m_pWrite++) = (SIZE_1 | flags | (uint8)size);
		}
	}

	if (m_pWrite > (m_pEndAddress - 5))
		m_pWrite = m_pAddress;
}


void Queue::Read(size_t & size, PARITION_FLAGS & flags)
{
	// read the size bits
	uint8 sizeBits = (m_pRead[0] & SIZE_MASK);
	flags = (PARITION_FLAGS)(m_pRead[0] & FLAGS_MASK);
	if (sizeBits == SIZE_4)
	{
		// four byte size
		m_pRead++;
		size = (size_t)(*(uint32 *)m_pRead);
		m_pRead += 4;
	}
	else
		if (sizeBits == SIZE_2)
		{
			// two byte size
			m_pRead++;
			size = (size_t)(*(uint16 *)m_pRead);
			m_pRead += 2;
		}
		else
		{
			// one byte size
			size = (size_t)(*(uint8 *)m_pRead & 0x0f);
			m_pRead++;
		}

	if (m_pRead > (m_pEndAddress - 5))
		m_pRead = m_pAddress;
}


//=================

void Encoder::Encode(const char * pData, size_t szData)
{
	// calculate Burrows/Wheeler transform of input string
	boost::shared_ptr<MSufSort> spMSufSort(new MSufSort);
	unsigned int sentinelIndex;
	spMSufSort->BWT((unsigned char *)pData, (unsigned int)szData, sentinelIndex);
	m_sentinelIndex = sentinelIndex;
	spMSufSort.reset();

	// resize original buffer to 1N
	realloc((void *)pData, szData);
	m_pData = (unsigned char *)pData;
	m_szData = (size_t)szData;

	// allocate buffers needed for encoding
	m_spRLE.reset(new unsigned char[szData]);
	m_pRLE = m_spRLE.get();
	m_spIndex.reset(new unsigned short[szData]);
	m_pIndex = m_spIndex.get();

	// populate RLE info and index info
	InitializeRLE();
	InitializeIndex();

	// establish queues for partitions
	m_spQueue.reset(new char[szData + (0x101 * 5)]);
	for (size_t i = 0; i < 0x100; i++)
		m_pQueue[i].Initialize(m_spQueue.get() + (i * 5) + m_symbolFirstIndex[i], m_symbolCount[i] + 5, m_symbolCount[i]);
	
}


void Encoder::InitializeRLE()
{
	// initialize the RLE array
	const unsigned char * pData = m_pData;
	const unsigned char * pEndData = m_pData + m_szData;
	const unsigned char * pSentinel = m_pData + m_sentinelIndex;
	unsigned char * pRLE = (unsigned char *)m_pRLE;

	while ((pData < pEndData) && (pSentinel))
	{
		if (pData == pSentinel)
		{
			// count the sentinel
			pSentinel = 0;
		}
		else
		{
			const unsigned char * pStart = pData;
			while ((pData < pEndData) && (pData != pSentinel) && (*pStart == *pData))
				pData++;
			size_t runLength = (size_t)(pData - pStart);
			while (runLength >= 4)
			{
				size_t lengthToWrite = (runLength > 0xffff) ? 0xffff : runLength;
				runLength -= lengthToWrite;
				for (size_t i = 0; i < 4; i++)
				{
					*(pRLE++) = ((unsigned char)(lengthToWrite & 0x0f) | ((i == 0) << 7));
					lengthToWrite >>= 4;
				}
			}
			while (runLength--)
				*(pRLE++) = 0x00;
		}
	}
}


void Encoder::InitializeIndex()
{
	// initialize the first/last 16-bit index array
	unsigned short * pIndex = (unsigned short *)m_pIndex;
	memset(m_symbolCount, 0, sizeof(m_symbolCount));

	const unsigned char * pData = m_pData;
	const unsigned char * pEndData = m_pData + m_szData;

	while (pData < pEndData)
	{
		size_t count = m_symbolCount[*(pData++)]++;
		if (count & 0x01)
			*(pIndex++) = (unsigned short)((count >> 15) | 0x8000);
		else
			*(pIndex++) = (unsigned short)(count & 0x7fff);
	}

	// adjust m_symbolFirstIndex
	size_t total = 1;
	for (size_t i = 0; i < 0x100; i++)
	{
		size_t temp = m_symbolCount[i];
		m_symbolFirstIndex[i] = total;
		total += temp;
	}
}
