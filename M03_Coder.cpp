#include "M03_Coder.h"
#include "memory.h"
#include <iostream>

M03_Coder::M03_Coder()
{
	for (size_t i = 0; i < (sizeof(m_signModel) / sizeof(m_signModel[0])); i++)
		m_signModel[i] = 1;
	for (size_t i = 0; i < (sizeof(m_secondarySignModel) / sizeof(m_secondarySignModel[0])); i++)
		m_secondarySignModel[i] = 1;

	for (size_t i = 0; i < (sizeof(m_bitModel) / sizeof(m_bitModel[0])); i++)
		m_bitModel[i] = 1;	
	for (size_t i = 0; i < (sizeof(m_secondaryBitModel) / sizeof(m_secondaryBitModel[0])); i++)
		m_secondaryBitModel[i] = 1;
/*
	FILE * fp = fopen("c:/stats.dat", "rb");
	if (fp)
	{
		fread(m_bitModel, 1, sizeof(m_bitModel), fp);
		fread(m_signModel, 1, sizeof(m_signModel), fp);
		fclose(fp);
	}
*/
/*
	for (size_t ratio = 0; ratio < (1 << BIT_MODEL_RATIO_BITS); ratio++)
		for (size_t last = 0; last < (1 << BIT_MODEL_LAST_BITS); last++)
			for (size_t depth = 0; depth < (1 << BIT_MODEL_DEPTH_BITS); depth++)
				for (size_t ratio2 = 0; ratio2 < (1 << BIT_MODEL_RATIO_2_BITS); ratio2++)
					for (size_t max = 0; max < (1 << BIT_MODEL_MAX_VALUE_BITS); max++)
					{
						unsigned int index = 0;
						index |= depth;
						index <<= BIT_MODEL_RATIO_BITS;
						index |= ratio;
						index <<= BIT_MODEL_MAX_VALUE_BITS;
						index |= max;
						index <<= BIT_MODEL_LAST_BITS;
						index |= (last & ((1 << BIT_MODEL_LAST_BITS) - 1));
						index <<= BIT_MODEL_RATIO_2_BITS;
						index |= ratio2;
						index <<= 2;
						unsigned int total = m_bitModel[index] + m_bitModel[index + 1] + m_bitModel[index + 2] + m_bitModel[index + 3];
			
						std::cout << ((double)(m_bitModel[index] * 100) / total) << "%,  " << ((double)(m_bitModel[index + 1] * 100) / total) << "%,  " << ((double)(m_bitModel[index + 2] * 100) / total) << std::endl; 
					}
*/

}





M03_Coder::~M03_Coder()
{
/*
	FILE * fp = fopen("c:/stats.dat", "wb");
	fwrite(m_bitModel, 1, sizeof(m_bitModel), fp);
//	for (size_t i = 0; i < sizeof(m_bitModel); i++)
//		if (m_bitModel[i] > 7)
//			m_bitModel[i] >>= 4;

	fwrite(m_signModel, 1, sizeof(m_signModel), fp);
//	for (size_t i = 0; i < sizeof(m_signModel); i++)
//		if (m_signModel[i] > 7)
//			m_signModel[i] >>= 3;
	fclose(fp);
*/
}


void M03_Coder::GetSignProb
(
	unsigned int numSymbols,
	unsigned int maxValue,
	unsigned int ratio,
	unsigned int forcedDistribution,
	unsigned int & freqA, 
	unsigned int & freqB, 
	unsigned int & freqC, 
	unsigned int & totalFreq,
	unsigned int & index,
	unsigned int & secondaryIndex
)
{
	static unsigned char numSymbolsToBits[] = 
	{
		0, 1, 1, 2, 2, 2, 2, 3,	 3, 3, 3, 3, 3, 3, 3, 4,
		4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 4, 4, 5, 
		5, 5, 5, 5, 5, 5, 5, 5,  5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5,  5, 5, 5, 5, 5, 5, 5, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,	 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 7
	};
	if (numSymbols >= sizeof(numSymbolsToBits))
		numSymbols = (sizeof(numSymbolsToBits) - 1);
	unsigned int symbolBits = numSymbolsToBits[numSymbols];

	unsigned int maxValueBits = (maxValue > 7) ? 7 : maxValue;
	if (maxValueBits >= (1 << SIGN_MODEL_MAX_VALUE_BITS))
		maxValueBits = ((1 << SIGN_MODEL_MAX_VALUE_BITS) - 1);	

	index = 0;
	index |= ratio;
	index <<= SIGN_MODEL_MAX_VALUE_BITS;
	index |= maxValueBits;
	index <<= SIGN_MODEL_FORCED_BITS;
	index |= forcedDistribution;
	index <<= SIGN_MODEL_NUM_SYMBOLS_BITS;
	index |= symbolBits;
	index <<= 2;

	freqA = m_signModel[index];
	freqB = m_signModel[index + 1];
	freqC = (maxValue & 0x01) ? 0 : m_signModel[index + 2];
	totalFreq = (freqA + freqB + freqC);

	if (totalFreq < MAX_FOR_SECODARY_MODEL)
	{
		secondaryIndex = 0;	
		secondaryIndex |= ratio;
		secondaryIndex <<= SIGN_MODEL_NUM_SYMBOLS_BITS;
		secondaryIndex |= symbolBits;
		secondaryIndex <<= 2;
		
		freqA = m_secondarySignModel[secondaryIndex];
		freqB = m_secondarySignModel[secondaryIndex + 1];
		freqC = (maxValue & 0x01) ? 0 : m_secondarySignModel[secondaryIndex + 2];
		totalFreq = (freqA + freqB + freqC);
		return;
	}

	unsigned int tempIndex = index;
	if ((totalFreq < 32) && (symbolBits < 7))
	{
		tempIndex += 4;
		symbolBits++;
		unsigned int a = m_signModel[tempIndex];
		unsigned int b = m_signModel[tempIndex + 1];
		unsigned int c = (maxValue & 0x01) ? 0 : m_signModel[tempIndex + 2];
		
		freqA += a;
		freqB += b;
		freqC += c;
		totalFreq += (a + b + c);
	}
}


void M03_Coder::GetBitModelProb
(
	unsigned int childContextRatio,
	unsigned int recursionDepth,
	unsigned int maxValue,
	unsigned int lastBit,
	unsigned int distributionTrend,
	unsigned int & freqA,
	unsigned int & freqB,
	unsigned int & totalFreq,
	unsigned int & index,
	unsigned int & secondaryIndex
)
{
	static unsigned char normalizedMaxValueArray[] = 
	{
		0, 1, 2, 3
	};

	unsigned int normalizedMaxValue = maxValue;
	if (normalizedMaxValue >= sizeof(normalizedMaxValueArray))
		normalizedMaxValue = (sizeof(normalizedMaxValueArray) - 1);
	normalizedMaxValue = normalizedMaxValueArray[normalizedMaxValue];	

	static unsigned int maxRecursionDepth = ((1 << BIT_MODEL_DEPTH_BITS) - 1);
	if (recursionDepth > maxRecursionDepth)
		recursionDepth = maxRecursionDepth;

	index = 0;

	static unsigned char numSymbolsToBits[] = 
	{
		0, 1, 1, 2, 2, 2, 2, 3,	 3, 3, 3, 3, 3, 3, 3, 4,
		4, 4, 4, 4, 4, 4, 4, 4,  4, 4, 4, 4, 4, 4, 4, 5, 
		5, 5, 5, 5, 5, 5, 5, 5,  5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5,  5, 5, 5, 5, 5, 5, 5, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,	 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,  6, 6, 6, 6, 6, 6, 6, 7
	};

	index |= childContextRatio;
	index <<= BIT_MODEL_LAST_BITS;
	index |= (lastBit & ((1 << BIT_MODEL_LAST_BITS) - 1));
	index <<= BIT_MODEL_TREND_BITS;
	index |= distributionTrend;
	index <<= BIT_MODEL_DEPTH_BITS;
	index |= recursionDepth;
	index <<= BIT_MODEL_MAX_VALUE_BITS;
	index |= normalizedMaxValue;	

	index <<= 1;
	freqA = m_bitModel[index];
	freqB = m_bitModel[index + 1];
	totalFreq = freqA + freqB;

	if (totalFreq < MAX_FOR_SECODARY_MODEL)
	{
		secondaryIndex = 0;	
		secondaryIndex |= childContextRatio;
		secondaryIndex <<= BIT_MODEL_LAST_BITS;
		//secondaryIndex |= (lastBit & ((1 << BIT_MODEL_LAST_BITS) - 1));;
		secondaryIndex <<= 1;
		
		freqA = m_secondaryBitModel[secondaryIndex];
		freqB = m_secondaryBitModel[secondaryIndex + 1];
		totalFreq = (freqA + freqB);
		return;
	}

	unsigned int tempIndex = index;
	if ((totalFreq < 16) && (normalizedMaxValue < 3))
	{
		tempIndex += 2;
		normalizedMaxValue++;
		unsigned int a = m_bitModel[tempIndex];
		unsigned int b = m_bitModel[tempIndex + 1];
		freqA += a;
		freqB += b;
		totalFreq += (a + b);
	}
}


void M03_Coder::AdjustSignModel(unsigned int index, unsigned int secondaryIndex)
{
	unsigned int baseIndex = index & ~0x03;
	int tot = m_signModel[baseIndex] + m_signModel[baseIndex + 1] + m_signModel[baseIndex + 2];
	if ((m_signModel[index] += 1) > 250)
	{
		if (m_signModel[baseIndex] > 1)
			m_signModel[baseIndex] >>= 1;
		if (m_signModel[baseIndex + 1] > 1)
			m_signModel[baseIndex + 1] >>= 1;
		if (m_signModel[baseIndex + 2] > 1)
			m_signModel[baseIndex + 2] >>= 1;
	}
	if (tot < MAX_FOR_SECODARY_MODEL)
	{
		if ((m_secondarySignModel[secondaryIndex] += 1) > 250)
		{
			secondaryIndex &= ~0x03;
			if (m_secondarySignModel[secondaryIndex] > 1)
				m_secondarySignModel[secondaryIndex] >>= 1;
			if (m_secondarySignModel[secondaryIndex + 1] > 1)
				m_secondarySignModel[secondaryIndex + 1] >>= 1;
			if (m_secondarySignModel[secondaryIndex + 2] > 1)
				m_secondarySignModel[secondaryIndex + 2] >>= 1;
		}
	}
}


void M03_Coder::AdjustBitModel(unsigned int index, unsigned int secondaryIndex)
{
	unsigned int baseIndex = index & ~0x01;
	int tot = (m_bitModel[baseIndex] + m_bitModel[baseIndex + 1]);
	if ((m_bitModel[index] += 4) > 250)
	{
		if (m_bitModel[baseIndex] > 1)
			m_bitModel[baseIndex] >>= 1;
		if (m_bitModel[baseIndex + 1] > 1)
			m_bitModel[baseIndex + 1] >>= 1;
	}
	if (tot < MAX_FOR_SECODARY_MODEL)
	{
		if (m_secondaryBitModel[secondaryIndex]++ > 250)
		{
			secondaryIndex &= ~0x01;
			if (m_secondaryBitModel[secondaryIndex] > 1)
				m_secondaryBitModel[secondaryIndex] >>= 1;
			if (m_secondaryBitModel[secondaryIndex + 1] > 1)
				m_secondaryBitModel[secondaryIndex + 1] >>= 1;
		}
	}
}


void M03_Coder::AdjustTrend(unsigned int dif, unsigned int max, unsigned int & trend)
{
	unsigned int n = (max > 8) ? 2 : 1;
	if (max > 1)
	{
		if (dif == max)
		{
			trend += n;
			if (trend >= (1 << BIT_MODEL_TREND_BITS))
				trend = ((1 << BIT_MODEL_TREND_BITS) - 1);
		}
		else
		{
			if (trend > n)
				trend -= n;
			else
				trend = 0;
		}
	}
}