#ifndef MSUFSORT_3_1_1_H
#define MSUFSORT_3_1_1_H

#include <stack>
#include <iostream>
#include <algorithm>

using namespace std;

// macros to enable various verification features
//#define VERIFY_SORT							// enable to verify correctness of suffix sort.
//#define VERIFY_BWT							// enable to verify reverse BWT is same as original string.
//#define VERIFY_STRING_RESTORATION				// verify that the ISA to original string transform is correct.
#ifdef VERIFY_SORT
	#define MAINTAIN_COPY_OF_STRING
#elif defined VERIFY_STRING_RESTORATION
	#define MAINTAIN_COPY_OF_STRING
#elif defined VERIFY_BWT
	#define MAINTAIN_COPY_OF_STRING
#endif



// macros to enable various suffix sorting features
#define USE_SEVEN_WAY_QUICKSORT				
#define USE_INDUCTION_SORTING				// enable induction sorting
#define USE_TANDEM_REPEAT_SORTING			// enable tandem repeat sorting
#define USE_FAST_TWO_STAGE					// use cache aware code for faster second stage of two stage
											// if used then induction sorting must be enabled.

//#define VERBOSE
#define DONT_CHECK_DURING_TWO_STAGE				0x80000000
#define DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE	0x40000000

// USE_LITTLE_ENDIAN if enabled (Intel)
#define USE_LITTLE_ENDIAN


#ifdef USE_LITTLE_ENDIAN
__forceinline unsigned short GetU16(void * address){return *(unsigned short *)address;}
#else
	// a tiny bit of a hack here to support big endian platforms.  it would have been cleaner to swap the order on
	// the little endian but the code was written for little endian initially, so it was easier to make the hack for
	// big endian instead.  This shouldn't effect performance very much at all.
__forceinline unsigned short GetU16(void * address){unsigned short s = *(unsigned short *)address; return ((s >> 8) | (s << 8));}
#endif



class MSufSort 
{
public:

	// constructor
	MSufSort();

	// destructor
	virtual ~MSufSort();

	// clears object
	void Clear();

	// does suffix sort on the source of length nBytes
	bool Sort(unsigned char * source, unsigned int nBytes);

	// does BWT rather than suffix sort on source of length nBytes
	bool BWT(unsigned char * source, unsigned int nBytes, unsigned int & bwtIndex);

	// does the reverse BWT on the source provided.
	void UnBWT(unsigned char * source, unsigned int nBytes, unsigned int index, char * pWorkspace = 0);

	// returns const char with version information
	static const char * GetVersion();

	// count the number of B* suffixes in the string provided.
	static const unsigned int GetBStarCount(const unsigned char * data, const unsigned int nBytes);

protected:

private:

	class PartitionSize
	{
	public:
		unsigned int	m_size;
		unsigned short	m_suffix;
		bool operator < (const PartitionSize & p) const{return m_size < p.m_size;}
	};

	enum {MIN_LENGTH_FOR_QUICKSORT = 16, MIN_LENGTH_FOR_TANDEM_REPEAT = 32};

	// initializes pointers to work space and initializes counts
	void Initialize(unsigned char * source, unsigned int nBytes);

	// insertion sort for small arrays
	void InsertionSort(unsigned int ** array, unsigned int count, int matchLength);

	// multikey quick sort for larger arrays.
	void MultiKeyQuickSort(unsigned int ** array, unsigned int count, unsigned int matchLength);

	// sort a partition of the SA.
	void Sort(unsigned int ** array, unsigned int count, unsigned int matchLength = 2);

	//
	void Partition(unsigned int ** array, unsigned int count, unsigned int pivot, int offset,
				   unsigned int & leftCount, unsigned int & middleCount, unsigned int & rightCount);


	// select pivot value
	unsigned int SelectPivot(unsigned int * indexA, unsigned int * indexB, unsigned int * indexC);

	// swap references to two suffixes
	void Swap(unsigned int ** ptrA, unsigned int ** ptrB);

	// direct compare sort for two suffixes using the partial ISA values.
	bool CompareStrings(unsigned int * indexA, unsigned int * indexB);

	// get ISA value at given suffix index
	unsigned int GetValue(unsigned int * suffixIndex);

	// change the ISA value for a newly sorted suffix.
	void MarkSuffixSorted(unsigned int * suffixIndex);
	void MarkSuffixSorted(unsigned int * suffixIndex, unsigned char symbol);

	// compares to strings from the original source string.
	bool CompareStrings(unsigned char * stringA, unsigned char * stringB, unsigned char * endOfSource);

	// verify the correctness of the B* suffixes
	bool VerifySort();

	// reverts ISA to original string in same space
	void RestoreOriginalString();

	// completes the improved two stage sort.
	void CompleteImprovedTwoStage();

	// completes the improved two stage sort but computes the BWT as a result rather than the suffix array.
	void CompleteImprovedTwoStageAsBWT();

	//
	void SortTandemRepeats(unsigned int ** suffixArray, unsigned int suffixCount, int matchLength);

	//
	void SortTandemRepeats(unsigned int ** suffixArray, unsigned int suffixCount, int matchLength, int recursionCounter);

	// address of partial inverse suffix array during suffix sort
	unsigned int *		m_ISA;

	// address of partial suffix array during suffix sort.  Changed to 
	// address of complete suffix array once suffix sort is completed.
	unsigned int **		m_SA;

	// stack containing sub partition sizes of the suffix array during
	// suffix sort.
	typedef struct PartitionInfo
	{
		PartitionInfo(unsigned int size, unsigned int matchLength, bool potentialTandemRepeats,
					  unsigned int firstSortedRank = 0xffffffff, unsigned int partitionOffset = 0xffffffff):
					  m_size(size), m_matchLength(matchLength), m_firstSortedRank(firstSortedRank),
					  m_potentialTandemRepeats(potentialTandemRepeats), m_partitionOffset(partitionOffset){}
		bool			m_potentialTandemRepeats;
		unsigned int	m_size;
		unsigned int	m_matchLength;
		unsigned int	m_firstSortedRank;
		unsigned int	m_partitionOffset;
	} PartitionInfo;
	stack<PartitionInfo>	m_partitionSize;

	//
	unsigned int		m_firstRank[0x10000];

	//
	unsigned int		m_lastRank[0x10000];

	//
	unsigned int		m_nextSortedRank;

	//
	unsigned int		m_bStarCount;

	// buffer used only to maintain a copy of the input for verification of suffix sort.
	unsigned char *		m_copyOfInput;

	// length of string being sorted.
	unsigned int		m_sourceLength;

	//
	unsigned int		m_suffixCount[0x10000];

	//
	unsigned int		m_bStarSuffixCount[0x10000];
	
	//
	unsigned char *		m_source;

	//
	unsigned int *		m_completedSA;

	//
	unsigned char *		m_bwt;

	//
	unsigned int		m_bwtIndex;

	//
	unsigned char		m_tandemRepeatSymbol;

	//
	unsigned int		m_tandemRepeatDepth;
unsigned int m_currentMatchLength;
};




__forceinline void MSufSort::Sort(unsigned int ** array, unsigned int count, unsigned int matchLength)
{
	if (count < MIN_LENGTH_FOR_QUICKSORT)
		InsertionSort(array, count, matchLength);
	else
		MultiKeyQuickSort(array, count, matchLength);
}







__forceinline void MSufSort::MarkSuffixSorted(unsigned int * suffixIndex)
{
	#ifdef USE_INDUCTION_SORTING
		if (m_tandemRepeatDepth)
		{
			MarkSuffixSorted(suffixIndex, m_tandemRepeatSymbol);
			return;
		}
		suffixIndex[1] &= 0xff;
		suffixIndex[1] |= ((*suffixIndex & 0xff) << 8);
		*suffixIndex = m_nextSortedRank++;
	#endif
}






__forceinline void MSufSort::MarkSuffixSorted(unsigned int * suffixIndex, unsigned char symbol)
{
	#ifdef USE_INDUCTION_SORTING
		suffixIndex[1] &= 0xff;
		suffixIndex[1] |= (symbol << 8);
		*suffixIndex = m_nextSortedRank++;
	#endif
}








__forceinline bool MSufSort::CompareStrings(unsigned char * stringA, 
											unsigned char * stringB,
											unsigned char * endOfSource)
{
	while ((stringA < endOfSource) && (stringB < endOfSource) && (stringA[0] == stringB[0]))
	{
		stringA++;
		stringB++;
	}
	if (stringA == endOfSource)
		return true;
	if (stringB == endOfSource)
		return false;
	return (stringA[0] < stringB[0]);
}







__forceinline bool MSufSort::CompareStrings(unsigned int * indexA, unsigned int * indexB)
{
	unsigned int a, b;

	while ((a = GetValue(indexA)) == (b = GetValue(indexB)))
	{
		indexA += 2;
		indexB += 2;
	}
	return (a < b);
}






__forceinline unsigned int MSufSort::GetValue(unsigned int * suffixIndex)
{
	if (*suffixIndex < 0x80000000)
		return (suffixIndex[-1] & 0x3fffffff);
	return *suffixIndex;
}







__forceinline void MSufSort::Swap(unsigned int ** ptrA, unsigned int ** ptrB)
{
	unsigned int * temp = *ptrA;
	*ptrA = *ptrB;
	*ptrB = temp;
}







__forceinline unsigned int MSufSort::SelectPivot(unsigned int * index1, unsigned int * index2, unsigned int * index3)
{
	// middle of three method.
	unsigned int value1 = GetValue(index1);
	unsigned int value2 = GetValue(index2);
	unsigned int value3 = GetValue(index3);

	if (value1 < value2)
		return ((value2 < value3) ? value2 : (value1 < value3) ? value3 : value1);
	return ((value1 < value3) ? value1 : (value2 < value3) ? value3 : value2); 
}






#endif
