#ifndef M03_CODER_H
#define M03_CODER_H


#include <boost/scoped_array.hpp>
#include <iostream>
#include "Queue/Queue.h"

#define __FORCEINLINE__		__forceinline



class M03_Coder
{
public:

	M03_Coder();

	virtual ~M03_Coder();

protected:

	enum {MAX_SYMBOL = 0x101};
	enum 
	{
		MAX_RUN_LENGTH	= ((1 << 20) - 1),
		SINGLETON		= 0x80000000,

		FLAGS_SHIFT		= 3,
		POST_PUSH_SHIFT = 1,
		SKIP			= 0x02,
		START_CONTEXT	= 0x01,
		FLAGS_MASK		= 0x03
	};

	enum
	{
		MAX_FOR_SECODARY_MODEL = 16
	};

	//
	enum 
	{
		SIGN_MODEL_RATIO_BITS		= 3,
		SIGN_MODEL_MAX_VALUE_BITS	= 3,
		SIGN_MODEL_NUM_SYMBOLS_BITS	= 3,
		SIGN_MODEL_FORCED_BITS		= 2,

		SIGN_MODEL_TOTAL_BITS		=  SIGN_MODEL_NUM_SYMBOLS_BITS + SIGN_MODEL_RATIO_BITS + SIGN_MODEL_MAX_VALUE_BITS + SIGN_MODEL_FORCED_BITS
	};

	unsigned char			m_signModel[(1 << SIGN_MODEL_TOTAL_BITS) << 2];
	unsigned char			m_secondarySignModel[(1 << (SIGN_MODEL_RATIO_BITS + SIGN_MODEL_NUM_SYMBOLS_BITS)) << 2];
	enum
	{
		BIT_MODEL_RATIO_BITS		= SIGN_MODEL_RATIO_BITS,
		BIT_MODEL_MAX_VALUE_BITS	= 3,
		BIT_MODEL_LAST_BITS			= 3,
		BIT_MODEL_DEPTH_BITS		= 2,
		BIT_MODEL_TREND_BITS		= 2,

		BIT_MODEL_TOTAL_BITS		= BIT_MODEL_TREND_BITS + BIT_MODEL_RATIO_BITS + BIT_MODEL_MAX_VALUE_BITS + BIT_MODEL_LAST_BITS + BIT_MODEL_DEPTH_BITS
	};


	unsigned char			m_bitModel[(1 << BIT_MODEL_TOTAL_BITS) << 1];
	unsigned char			m_secondaryBitModel[(1 << (BIT_MODEL_RATIO_BITS + BIT_MODEL_LAST_BITS)) << 1];

	M03_Queue			m_queue[MAX_SYMBOL];

	void GetSignProb
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
	);

	void GetBitModelProb
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
	);


	void AdjustSignModel(unsigned int index, unsigned int secondaryIndex);

	void AdjustBitModel(unsigned int index, unsigned int secondaryIndex);

	void AdjustTrend(unsigned int dif, unsigned int max, unsigned int & trend);

private:
};



#endif
