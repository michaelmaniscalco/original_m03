#include "MSufSort.h"
#include "time.h"



	
MSufSort::MSufSort()
{
	// constructor
	m_copyOfInput = 0;
	Clear();
}



	
MSufSort::~MSufSort()
{
	// destructor
	Clear();
}





void MSufSort::Clear()
{
	// clears object
	if (m_copyOfInput)
		delete [] m_copyOfInput;
	m_copyOfInput = 0;
	m_sourceLength = 0;
	m_tandemRepeatDepth = 0;
}






const unsigned int MSufSort::GetBStarCount(const unsigned char * data, const unsigned int nBytes)
{
	// count the number of B* suffixes in the string provided.
	unsigned int count = 0;
	unsigned char * endPtr = (unsigned char *)data;
	unsigned char * ptr = endPtr + nBytes - 1;
	unsigned char * startPtr = ptr;
	int last = -1;

	while (ptr >= endPtr)
	{
		while ((ptr >= endPtr) && (ptr[0] >= last))
			last = *ptr--;

		if (ptr >= endPtr)
		{
			count++;
			last = *ptr--;
			while ((ptr >= endPtr) && (ptr[0] <= last))
				last = *ptr--;
		}
	}

	return count;
}







void MSufSort::Initialize(unsigned char * source, unsigned int nBytes)
{
	// initializes pointers to work space and initializes counts
	memset(m_suffixCount, 0, sizeof(unsigned int) * 0x10000);
	memset(m_bStarSuffixCount, 0, sizeof(unsigned int) * 0x10000);

	m_source = source;
	m_ISA = (unsigned int *)source;
	m_sourceLength = nBytes;

	// count the suffixes and B* suffixes.
	unsigned char * endPtr = (unsigned char *)source;
	unsigned char * ptr = endPtr + nBytes - 1;
	unsigned char * startPtr = ptr;
	int last = -1;

	m_suffixCount[ptr[0]]++;

	while (ptr >= endPtr)
	{	
		while ((ptr >= endPtr) && (ptr[0] >= last))
		{
			if (ptr != startPtr)
				m_suffixCount[GetU16(ptr)]++;
			last = *ptr--;
		}

		if (ptr >= endPtr)
		{
			m_suffixCount[GetU16(ptr)]++;
			m_bStarSuffixCount[GetU16(ptr)]++;
			last = *ptr--;
			while ((ptr >= endPtr) && (ptr[0] <= last))
			{
				m_suffixCount[GetU16(ptr)]++;
				last = *ptr--;
			}
		}
	}

	// determine the partially sorted ranks of the suffixes for the ISA based
	// on the counts of each suffix using the first two symbols of each suffix.
	// Also, calculate the partition boundaries for the SA based on the first
	// two symbols of each B* suffix.
	unsigned int * firstSaPartitionIndex = new unsigned int[0x10000];
	unsigned int * partitionOffset = new unsigned int[0x10000];
	unsigned int rank = 0x80000300;
	unsigned int saPartitionIndex = 0;
	PartitionSize * pSize = new PartitionSize[0x10000];
	for (unsigned int i = 0; i < 0x10000; i++)
	{
		unsigned short suffix = (i >> 8) | ((i & 0xff) << 8);
		if (m_suffixCount[suffix])
		{
			pSize[suffix].m_size = m_suffixCount[suffix];
			pSize[suffix].m_suffix = suffix;

			partitionOffset[suffix] = saPartitionIndex;
			if (m_bStarSuffixCount[suffix])
			{
				firstSaPartitionIndex[suffix] = saPartitionIndex;
				saPartitionIndex += m_bStarSuffixCount[suffix];
			}
			m_firstRank[suffix] = rank;
			rank += m_suffixCount[suffix];
			rank += (0x200 - (rank & 0xff));
			m_lastRank[suffix] = rank;
			rank += 0x200;
		}
	}

	// push the SA partition lengths for B* suffixes sorted for first two symbols
	// onto stack of partially sorted SA partitions.
	sort(pSize, pSize + 0x10000);

	m_bStarCount = 0;
	for (int i = 0; i < 0x10000; i++)
	{
		unsigned short suffix = pSize[i].m_suffix;
		if (m_bStarSuffixCount[suffix])
		{
			m_bStarCount += m_bStarSuffixCount[suffix];
			m_partitionSize.push(PartitionInfo(m_bStarSuffixCount[suffix], 2, 
												false, m_firstRank[suffix], partitionOffset[suffix]));
		}
	}

	// Initialize the SA to reference each B* suffix
	m_SA = (unsigned int **)(source + (((m_sourceLength + 3) * 6) - (m_bStarCount * 4)));
	endPtr = (unsigned char *)source;
	ptr = endPtr + nBytes - 1;
	startPtr = ptr;
	last = -1;

	while (ptr >= endPtr)
	{
		while ((ptr >= endPtr) && (ptr[0] >= last))
			last = *ptr--;

		if (ptr >= endPtr)
		{
			// B* suffix
			unsigned short suffix = GetU16(ptr);
			m_SA[firstSaPartitionIndex[suffix]++] = m_ISA + ((unsigned int)(ptr - endPtr) + 2);
			last = *ptr--;
			while ((ptr >= endPtr) && (ptr[0] <= last))
				last = *ptr--;
		}
	}

	// initialize the partial ISA with the partially sorted ranks 
	// and initialize the SA for B* suffixes with partitions sorted 
	// by first two symbols.
	m_ISA[nBytes + 1] = 0x80000100 | source[1];
	m_ISA[nBytes] = 0x80000200 | source[0];
	m_ISA[nBytes - 1] = m_lastRank[source[nBytes - 1]];

	unsigned int c2 = 0x00;
	unsigned int c1 = source[nBytes - 1];

	for (int i = (int)nBytes - 2; i >= 0; i--)
	{
		unsigned int t = source[i];
		m_ISA[i] = m_lastRank[GetU16(source + i)] | c2;
		c2 = c1;
		c1 = t;
	}

	#ifdef VERBOSE
		cout << m_bStarCount << " B* suffixes\n";
	#endif

	//
	delete [] firstSaPartitionIndex;
	delete [] partitionOffset;
	delete [] pSize;
}




void MSufSort::SortTandemRepeats(unsigned int ** suffixArray, unsigned int suffixCount, int matchLength)
{
	// Change the ISA values for all suffixes in the array.
	unsigned int newIsaValue = (m_nextSortedRank + suffixCount - 1);

	// determine the tandem repeat length
	unsigned int startingValue = GetValue(suffixArray[0] - matchLength);

	if (m_tandemRepeatDepth == 0)
		m_tandemRepeatSymbol = startingValue & 0xff;

	for (unsigned int i = 0; i < suffixCount; i++)
		suffixArray[i][-matchLength] = newIsaValue;

	SortTandemRepeats(suffixArray, suffixCount, matchLength, 3);
	if (m_tandemRepeatDepth == 0)
		m_tandemRepeatSymbol = -1;
}






void MSufSort::SortTandemRepeats(unsigned int ** suffixArray, unsigned int suffixCount, int matchLength, int recursionCounter)
{
	unsigned int leftCount;
	unsigned int rightCount;
	unsigned int middleCount;
	unsigned int leftRepeatIsaValue;
	unsigned int middleRepeatIsaValue;
	unsigned int rightRepeatIsaValue = m_nextSortedRank + suffixCount - 1;

	m_tandemRepeatDepth++;
	Partition(suffixArray, suffixCount, rightRepeatIsaValue, -recursionCounter, leftCount, middleCount, rightCount);

	if (middleCount)
	{
		middleRepeatIsaValue = m_nextSortedRank + leftCount + middleCount - 1;
		for (unsigned int i = leftCount; i < leftCount + middleCount; i++)
		{
			suffixArray[i][-matchLength] = middleRepeatIsaValue;
			suffixArray[i][-matchLength + 1] &= 0x7fffffff;
		}
	}

	if (recursionCounter > 2)
	{
		if (leftCount)
		{
			if (leftCount != suffixCount)
			{
				leftRepeatIsaValue = m_nextSortedRank + leftCount - 1;
				for (unsigned int i = 0; i < leftCount; i++)
				{
					suffixArray[i][-matchLength] = leftRepeatIsaValue;
					suffixArray[i][-matchLength + 1] &= 0x7fffffff;
				}
			}
			SortTandemRepeats(suffixArray, leftCount, matchLength, recursionCounter - 1);
		}
		if (rightCount)
		{
			unsigned int tempNextRank = m_nextSortedRank;
			m_nextSortedRank += middleCount;
			SortTandemRepeats(suffixArray + leftCount + middleCount, rightCount, matchLength, recursionCounter - 1);
			m_nextSortedRank = tempNextRank;
		}
	}
	else
	{
		if (leftCount)
		{
			if (leftCount > 1)
			{
				// sort all the suffixes of the left partition
				if (leftCount != suffixCount)
				{
					leftRepeatIsaValue = m_nextSortedRank + leftCount - 1;
					for (unsigned int i = 0; i < leftCount; i++)
					{
						suffixArray[i][-matchLength] = leftRepeatIsaValue;
						suffixArray[i][-matchLength + 1] &= 0x7fffffff;
					}
				}
				Sort(suffixArray, leftCount, matchLength);
			}
			else
			{
				// mark the only suffix in the left partition as sorted.
				suffixArray[0] -= matchLength;
				MarkSuffixSorted(suffixArray[0]);
			}
		}
		if (rightCount)
		{
			if (rightCount > 1)
			{
				// sort all the suffixes of the right partition
				unsigned int tempNextRank = m_nextSortedRank;
				m_nextSortedRank += middleCount;
				Sort(suffixArray + leftCount + middleCount, rightCount, matchLength);
				m_nextSortedRank = tempNextRank;
			}
			else
			{
				// mark the only suffix on the right partition as sorted
				unsigned int tempNextRank = m_nextSortedRank;
				m_nextSortedRank += middleCount;
				suffixArray[leftCount + middleCount] -= matchLength;
				MarkSuffixSorted(suffixArray[leftCount + middleCount]);
				m_nextSortedRank = tempNextRank;
			}
		}
	}

	// resolve suffixes in the middle partition (the tandem repeats) using the
	// sorted order of the suffixes in the left and right partitions.
	unsigned int tandemCount = middleCount;

	// start by using the sorted suffixes of the left partition.
	int n = leftCount;
	int i = 0;
	while ((tandemCount) && (i < n))
	{
		unsigned int * k = suffixArray[i++] - (matchLength - recursionCounter);
		if ((k >= m_ISA) && (GetValue(k) == middleRepeatIsaValue))
		{
			tandemCount--;
			suffixArray[n++] = k;
		}
	}

	// now use the sorted suffixes of the right partition.
	n = middleCount + leftCount;
	i = suffixCount - 1;
	while ((tandemCount) && (i >= n))
	{
		unsigned int * k = suffixArray[i--] - (matchLength - recursionCounter);
		if ((k >= m_ISA) && (GetValue(k) == middleRepeatIsaValue))
		{
			tandemCount--;
			suffixArray[--n] = k;
		}
	}

	// now iterate the entire middle partition and mark all suffixes as sorted.
	for (unsigned int i = leftCount; i < leftCount + middleCount; i++)
		MarkSuffixSorted(suffixArray[i]);

	m_nextSortedRank += rightCount;

	--m_tandemRepeatDepth;
}





void MSufSort::Partition(unsigned int ** array, unsigned int count, unsigned int tandemRepeatValue, int offset,
				   unsigned int & leftCount, unsigned int & middleCount, unsigned int & rightCount)
{
	unsigned int ** ptr = array;
	unsigned int ** left = array;
	unsigned int ** right = array + count - 1;
	unsigned int temp;

	while (ptr <= right)
		if ((temp = GetValue((*ptr) + offset)) == tandemRepeatValue)
			ptr++;
		else
			if (temp < tandemRepeatValue)
				Swap(left++, ptr++);
			else
			{
				while ((right > ptr) && (GetValue((*right) + offset) > tandemRepeatValue))
					right--;
				Swap(ptr, right--);
			}

	// Calculate new partition sizes ...
	leftCount = (unsigned int)(left - array);
	middleCount = (unsigned int)(ptr - left);
	rightCount = count - (leftCount + middleCount);
}









void MSufSort::MultiKeyQuickSort(unsigned int ** array, unsigned int count, unsigned int matchLength)
{
	unsigned int ** startOfArray = array;
	unsigned int ** endOfArray = array + count;
	stack<PartitionInfo> localPartitionStack;
	bool potentialTandemRepeat = false;

	while (true) 
	{
		if (potentialTandemRepeat)
		{
			SortTandemRepeats(array, count, matchLength);
			array += count;
			if (localPartitionStack.empty())
				return;
			PartitionInfo partitionStruct = localPartitionStack.top();
			localPartitionStack.pop();
			count = partitionStruct.m_size;
			m_currentMatchLength = matchLength = partitionStruct.m_matchLength;
			potentialTandemRepeat = partitionStruct.m_potentialTandemRepeats;
		}
		else
		{
			if (count < MIN_LENGTH_FOR_QUICKSORT)
			{
				InsertionSort(array, count, matchLength);
				array += count;
				if (localPartitionStack.empty())
					return;
				PartitionInfo partitionStruct = localPartitionStack.top();
				localPartitionStack.pop();
				count = partitionStruct.m_size;
				m_currentMatchLength = matchLength = partitionStruct.m_matchLength;
				potentialTandemRepeat = partitionStruct.m_potentialTandemRepeats;
			}
			else
			{
				// do partitioning.
				#ifdef USE_SEVEN_WAY_QUICKSORT
				unsigned int ** ptrA = array;
				unsigned int ** ptrB = 0;
				unsigned int ** ptrC = array;
				unsigned int ** endOfArray = array + count - 1;
				unsigned int ** ptrD = endOfArray;
				unsigned int ** ptrE = 0;
				unsigned int ** ptrF = endOfArray;

				unsigned int ** ptr = array;

				// select the pivot value.
				unsigned int pivotB = SelectPivot(array[0], array[count >> 1], *ptrF);
				unsigned int pivotA = 0;
				unsigned int pivotC = 0;
				unsigned int temp;

				while (ptr < ptrD)
					if ((temp = GetValue(*ptr)) == pivotB)
						*(ptr++) += 2;
					else
						if (temp < pivotB)
						{
							if (ptrB == 0)
							{
								// first element in left partition.
								*ptr += 2;
								ptrB = array;
								pivotA = temp;
								ptrC++;
								Swap(ptrB++, ptr++);
							}
							else
							{
								if (temp == pivotA)
								{
									*ptr += 2;
									unsigned int * temp2 = *ptr;
									*ptr++ = *ptrC;
									*ptrC++ = *ptrB;
									*ptrB++ = temp2;
								}
								else
									if (temp < pivotA)
									{
										unsigned int * temp2 = *ptr;
										*ptr++ = *ptrC;
										*ptrC++ = *ptrB;
										*ptrB++ = *ptrA;
										*ptrA++ = temp2;
									}
									else
									{
										Swap(ptr++, ptrC++);
									}
							}
						}
						else
						{
							if (ptrE == 0)
							{
								// first element in right partition.
								*ptr += 2;
								ptrE = endOfArray;
								pivotC = temp;
								ptrD--;
								Swap(ptrE--, ptr);
							}
							else
							{
								if (temp == pivotC)
								{
									*ptr += 2;
									unsigned int * temp2 = *ptrD;
									*ptrD-- = *ptrE;
									*ptrE-- = *ptr;
									*ptr = temp2;
								}
								else
									if (temp < pivotC)
									{
										Swap(ptrD--, ptr);
									}
									else
									{
										unsigned int * temp2 = *ptrD;
										*ptrD-- = *ptrE;
										*ptrE-- = *ptrF;
										*ptrF-- = *ptr;
										*ptr = temp2;
									}
							}
						}

				if (ptr == ptrD)
					if ((temp = GetValue(*ptr)) == pivotB)
						*(ptr++) += 2;
					else
						if (temp < pivotB)
						{
							if (ptrB == 0)
							{
								// first element in left partition.
								*ptr += 2;
								ptrB = array;
								pivotA = temp;
								ptrC++;
								Swap(ptrB++, ptr++);
							}
							else
							{
								if (temp == pivotA)
								{
									*ptr += 2;
									unsigned int * temp2 = *ptr;
									*ptr++ = *ptrC;
									*ptrC++ = *ptrB;
									*ptrB++ = temp2;
								}
								else
									if (temp < pivotA)
									{
										unsigned int * temp2 = *ptr;
										*ptr++ = *ptrC;
										*ptrC++ = *ptrB;
										*ptrB++ = *ptrA;
										*ptrA++ = temp2;
									}
									else
									{
										Swap(ptr++, ptrC++);
									}
							}
						}
						else
						{
							if (ptrE == 0)
							{
								// first element in right partition.
								*ptr += 2;
								ptrE = endOfArray;
								pivotC = temp;
								ptrD--;
								Swap(ptrE--, ptr);
							}
							else
							{
								if (temp == pivotC)
								{
									*ptr += 2;
									Swap(ptrD--, ptrE--);
								}
								else
									if (temp < pivotC)
									{
										ptrD--;
									}
									else
									{
										unsigned int * temp2 = *ptr;
										*ptrD-- = *ptrE;
										*ptrE-- = *ptrF;
										*ptrF-- = temp2;
									}
							}
						}

				// Calculate new partition sizes ...
				unsigned int partitionA = (unsigned int)(ptrA - array);
				unsigned int partitionB = (ptrB) ? (unsigned int)(ptrB - ptrA) : 0;
				unsigned int partitionC = (unsigned int)(ptrC - ((ptrB) ? ptrB : ptrA));
				unsigned int partitionD = (unsigned int)(ptrD - ptrC) + 1;
				unsigned int partitionE = (ptrE) ? (unsigned int)(ptrE - ptrD) : 0;
				unsigned int partitionF = (unsigned int)(ptrF - ((ptrE) ? ptrE : endOfArray));
				unsigned int partitionG = (unsigned int)(endOfArray - ptrF);

				bool potentialTandemRepeats = false;

				if (partitionG)
					localPartitionStack.push(PartitionInfo(partitionG, matchLength, false));
				if (partitionF)
				{
					#ifdef USE_TANDEM_REPEAT_SORTING
					if (matchLength >= MIN_LENGTH_FOR_TANDEM_REPEAT)
					{
						unsigned int startingValue = GetValue(*ptrE - (matchLength + 2));
						potentialTandemRepeats = ((GetValue(*ptrE - 3) == startingValue) || (GetValue(*ptrE - 2) == startingValue)); 
					}
					#endif
					localPartitionStack.push(PartitionInfo(partitionF, matchLength + 2, potentialTandemRepeats));
				}
				if (partitionE)
					localPartitionStack.push(PartitionInfo(partitionE, matchLength, false));
				if (partitionD)
				{
					#ifdef USE_TANDEM_REPEAT_SORTING
					if (matchLength >= MIN_LENGTH_FOR_TANDEM_REPEAT)
					{
						unsigned int startingValue = GetValue(*ptrC - (matchLength + 2));
						potentialTandemRepeats = ((GetValue(*ptrC - 3) == startingValue) || (GetValue(*ptrC - 2) == startingValue)); 
					}
					#endif
					localPartitionStack.push(PartitionInfo(partitionD, matchLength + 2, potentialTandemRepeats));
				}
				if (partitionC)
					localPartitionStack.push(PartitionInfo(partitionC, matchLength, false));
				if (partitionB)
				{
					#ifdef USE_TANDEM_REPEAT_SORTING
					if (matchLength >= MIN_LENGTH_FOR_TANDEM_REPEAT)
					{
						unsigned int startingValue = GetValue(*ptrA - (matchLength + 2));
						potentialTandemRepeats = ((GetValue(*ptrA - 3) == startingValue) || (GetValue(*ptrA - 2) == startingValue)); 
					}
					#endif
					localPartitionStack.push(PartitionInfo(partitionB, matchLength + 2, potentialTandemRepeats));
				}

				if (!(count = partitionA))
				{
					PartitionInfo partitionStruct = localPartitionStack.top();
					localPartitionStack.pop();
					count = partitionStruct.m_size;
					m_currentMatchLength = matchLength = partitionStruct.m_matchLength;
					potentialTandemRepeat = partitionStruct.m_potentialTandemRepeats;
				}
				#else
				// do three way partitioning.
				unsigned int ** ptr = array;
				unsigned int ** left = array;
				unsigned int ** right = array + count - 1;
				// select the pivot value.
				unsigned int pivotA = SelectPivot(array[0], array[count >> 1], *right);
				unsigned int temp;

				while (ptr <= right)
					if ((temp = GetValue(*ptr)) == pivotA)
						*(ptr++) += 2;
					else
						if (temp < pivotA)
							Swap(left++, ptr++);
						else
						{
							while ((right > ptr) && (GetValue(*right) > pivotA))
								right--;
							Swap(ptr, right--);
						}

				// Calculate new partition sizes ...
				unsigned int leftSize = (unsigned int)(left - array);
				unsigned int middleSize = (unsigned int)(ptr - left);
				unsigned int rightSize = count - (leftSize + middleSize);

				if (rightSize)
					localPartitionStack.push(PartitionInfo(rightSize, matchLength, false));
				if (middleSize)
				{
					bool potentialTandemRepeats = false;
					#ifdef USE_TANDEM_REPEAT_SORTING
					if (matchLength >= MIN_LENGTH_FOR_TANDEM_REPEAT)
					{
						unsigned int startingValue = GetValue(array[leftSize] - (matchLength + 2));
						potentialTandemRepeats = ((GetValue(array[leftSize] - 3) == startingValue) || (GetValue(array[leftSize] - 2) == startingValue)); 
					}
					#endif
					localPartitionStack.push(PartitionInfo(middleSize, matchLength + 2, potentialTandemRepeats));
				}
				if (!(count = leftSize))
				{
					PartitionInfo partitionStruct = localPartitionStack.top();
					localPartitionStack.pop();
					count = partitionStruct.m_size;
					m_currentMatchLength = matchLength = partitionStruct.m_matchLength;
					potentialTandemRepeat = partitionStruct.m_potentialTandemRepeats;
				}
				#endif

			}
		}
	}
}




void MSufSort::InsertionSort(unsigned int ** suffixArray, unsigned int count, int matchLength)
{
	static unsigned int partitionLength[MIN_LENGTH_FOR_QUICKSORT + 1];
	static unsigned int partitionDepth[MIN_LENGTH_FOR_QUICKSORT + 1];
	static bool partitionHasPotentialTandemRepeat[MIN_LENGTH_FOR_QUICKSORT + 1];
	static unsigned int numPartitions = 0;

	partitionLength[numPartitions] = count;
	partitionDepth[numPartitions] = matchLength;
	partitionHasPotentialTandemRepeat[numPartitions] = false;
	unsigned int startingNumPartitions = numPartitions++;

	while (numPartitions > startingNumPartitions)
	{
		bool potentialTandemRepeats = partitionHasPotentialTandemRepeat[--numPartitions];
		count = partitionLength[numPartitions];
		m_currentMatchLength = matchLength = partitionDepth[numPartitions];

		if (potentialTandemRepeats)
		{
			SortTandemRepeats(suffixArray, count, matchLength);
			suffixArray += count;
		}
		else
			if (count < 3)
			{
				// special handling for less than three elements.
				if (count == 2)
				{
					if (!CompareStrings(suffixArray[0], suffixArray[1]))
						Swap(suffixArray, suffixArray + 1);				
					suffixArray[0] -= matchLength;
					suffixArray[1] -= matchLength;
					MarkSuffixSorted(suffixArray[0]);
					MarkSuffixSorted(suffixArray[1]);
				}
				else
				{
					suffixArray[0] -= matchLength;
					MarkSuffixSorted(suffixArray[0]);
				}

				suffixArray += count;
			}
			else
			{
				static unsigned int suffixValue[MIN_LENGTH_FOR_QUICKSORT + 1];
				for (unsigned int i = 0; i < count; i++)
					suffixValue[i] = GetValue(suffixArray[i]);
				suffixArray[0] += 2;

				for (unsigned int i = 1; i < count; i++)
				{
					suffixArray[i] += 2;
					
					unsigned int * offset = suffixArray[i];
					unsigned int value = suffixValue[i];

					bool swap = false;
					unsigned int j = i;
					for ( ; j > 0; j--)
						if (suffixValue[j - 1] > value)
						{
							suffixArray[j] = suffixArray[j - 1];
							suffixValue[j] = suffixValue[j - 1];
							swap = true;
						}
						else
							break;

					if (swap)
					{
						suffixArray[j] = offset;
						suffixValue[j] = value;
					}
				}


			matchLength += 2;
			for (int k = count - 1; k >= 0;)
			{
				int n = k;
				unsigned int val = suffixValue[k];
				while ((k >= 0) && (suffixValue[k] == val))
					k--;
				#ifdef USE_TANDEM_REPEAT_SORTING
					if ((partitionLength[numPartitions] = (n - k)) > 1)
					{
						unsigned int tandemRepeatIsaValue = suffixArray[n][-matchLength];
						partitionHasPotentialTandemRepeat[numPartitions] = ((matchLength >= MIN_LENGTH_FOR_TANDEM_REPEAT) && 
								((tandemRepeatIsaValue == GetValue(suffixArray[n] - 3)) || (tandemRepeatIsaValue == GetValue(suffixArray[n] - 2))));
					}
				#else
					partitionLength[numPartitions] = n - k;
					partitionHasPotentialTandemRepeat[numPartitions] = false;
				#endif
				partitionDepth[numPartitions++] = matchLength;
			}
		}
	}
}






void MSufSort::RestoreOriginalString()
{
	// reverts ISA to original string in same space
	int start = clock();

	unsigned char * ptr = (unsigned char *)m_ISA;
	unsigned char t[2];
	t[1] = m_ISA[m_sourceLength + 1] & 0xff;
	t[0] = m_ISA[m_sourceLength + 0] & 0xff;
	ptr += 2;

	for (unsigned int i = 0; i < m_sourceLength; )
		if (m_ISA[i + 1] < 0x80000000)
		{
			ptr[i] = (m_ISA[i + 1] >> 8) & 0xff;
			ptr[i + 1] = (m_ISA[i + 1] & 0xff);
			i += 2;
		}
		else
		{
			ptr[i] = (m_ISA[i] & 0xff);
			i++;
		}

	ptr -= 2;
	ptr[0] = t[0];
	ptr[1] = t[1];

	int finish = clock();

	#ifdef VERBOSE
		int finish2 = clock();
		cout << "Original string restored in " << (double(finish - start) / CLOCKS_PER_SEC) << " seconds\n";
	#endif


	#ifdef VERIFY_STRING_RESTORATION
		bool error = false;
		for (unsigned int i = 0; i < m_sourceLength; i++)
			if (ptr[i] != m_copyOfInput[i])
				error = true;
		if (error)
			cout << "ISA to string error detected.\n";
	#endif
}






void MSufSort::CompleteImprovedTwoStage()
{
	// completes the second stage of the improved two stage sort.
	int start = clock();

	// begin by expanding the partial SA to the complete SA space.
	unsigned int * SA = (unsigned int *)((unsigned char *)m_ISA + ((m_sourceLength + 3) & ~3));
	unsigned int * ptr = SA;

	*(ptr++) = 0xc0000000;
	unsigned int total = 1;

	for (unsigned int i = 0; i < 0x10000; i++)
	{
		unsigned short s = (unsigned short)((i << 8) | (i >> 8));
		if (m_suffixCount[s])
		{
			m_firstRank[s] = total + m_bStarSuffixCount[s];
			total += m_suffixCount[s];
			m_lastRank[s] = total;

			for (unsigned int j = 0; j < m_bStarSuffixCount[s]; j++)
				*(ptr++) = (unsigned int)(*(m_SA++) - m_ISA);
			for (unsigned int j = m_bStarSuffixCount[s]; j < m_suffixCount[s]; j++)
				*(ptr++) = 0xc0000000;
		}
	}

	// now do right to left 
	unsigned int lastRank = 0;
	int lastS = -1;
	unsigned int * lastSa = 0;
	unsigned int * saPtr = SA + m_sourceLength + 1;
	unsigned int * endSaPtr = SA;

	while (--saPtr > endSaPtr)
	{
		unsigned int n = *saPtr;
		unsigned int k;
		if (n < DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE)
		{
			if (n)
			{
				if (m_source[n - 1] <= m_source[n])
				{
					#ifdef USE_FAST_TWO_STAGE
						unsigned short s = GetU16(m_source + n - 1);
						if (s == lastS)
						{
							*(--lastSa) = n - 1;
							k = --lastRank;
						}
						else
						{
							if (lastS != -1)
								m_lastRank[lastS] = lastRank;
							lastS = s;
							lastRank = k = --m_lastRank[s];
							lastSa = SA + k;
							*lastSa = n - 1;
						}

						*saPtr |= DONT_CHECK_DURING_TWO_STAGE;	// don't check this index on the left to right
						if ((n > 1) && (m_source[n - 2] > m_source[n - 1]))
							SA[k] |= DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE;	// don't check this index on the remainder of the right to left.
					#else
						unsigned short s = GetU16(m_source + n - 1);
						k = --m_lastRank[s];
						SA[k] = n - 1;
					#endif
				}
			}
		}
	}

	// now do left to right
	SA[0] = m_sourceLength;
	SA[m_firstRank[m_source[m_sourceLength - 1]]++] = m_sourceLength - 1;

	lastRank = 0;
	lastS = -1;
	saPtr = SA;
	lastSa = 0;
	endSaPtr = SA + m_sourceLength + 1;
	while (++saPtr < endSaPtr)
	{
		unsigned int k;
		unsigned int n = *saPtr;
		if (n < DONT_CHECK_DURING_TWO_STAGE)
		{
			n &= 0x3fffffff;
			if ((n) && (m_source[n - 1] >= m_source[n]))
			{
				#ifdef USE_FAST_TWO_STAGE
					unsigned short s = GetU16(m_source + n - 1);
					if (s == lastS)
					{
						k = lastRank++;
						*(++lastSa) = n - 1;
					}
					else
					{
						if (lastS != -1)
							m_firstRank[lastS] = lastRank;
						lastS = s;
						k = m_firstRank[s]++;
						lastSa = SA + k;
						*lastSa = n - 1;
						lastRank = k + 1;
					}
				#else
					unsigned short s = GetU16(m_source + n - 1);
					SA[m_firstRank[s]++] = n - 1;
				#endif
			}
		}

		*saPtr &= 0x3fffffff;
	}

	m_completedSA = SA;
	int finish = clock();

	#ifdef VERBOSE
		cout << "Two stage elapsed time: " << (double)(finish - start) / CLOCKS_PER_SEC << " seconds.\n";
	#endif

	#ifdef VERIFY_TWO_STAGE
		unsigned int badIndexCount = 0;
		for (unsigned int i = 0; i <= m_sourceLength; i++)
			if (m_completedSA[i] > m_sourceLength)
				badIndexCount++;

		if (badIndexCount)
			cout << "*** Number of bad SA index: " << badIndexCount << "\n";
	#endif
}







void MSufSort::CompleteImprovedTwoStageAsBWT()
{
	// completes the second stage of the improved two stage sort but computes the BWT
	// directly rather than the suffix array.
	int start = clock();

	// begin by expanding the partial SA to the complete SA space.
	unsigned int * SA = (unsigned int *)((unsigned char *)m_ISA + ((m_sourceLength + 3) & ~3));
	unsigned int * ptr = SA;

	*(ptr++) = 0xc0000000;
	unsigned int total = 1;

	for (unsigned int i = 0; i < 0x10000; i++)
	{
		unsigned short s = (unsigned short)((i << 8) | (i >> 8));
		if (m_suffixCount[s])
		{
			m_firstRank[s] = total + m_bStarSuffixCount[s];
			total += m_suffixCount[s];
			m_lastRank[s] = total;

			for (unsigned int j = 0; j < m_bStarSuffixCount[s]; j++)
				*(ptr++) = (unsigned int)(*(m_SA++) - m_ISA);
			for (unsigned int j = m_bStarSuffixCount[s]; j < m_suffixCount[s]; j++)
				*(ptr++) = 0xc0000000;
		}
	}

	// now do right to left 
	unsigned int lastRank = 0;
	int lastS = -1;
	unsigned int * lastSa = 0;
	unsigned int * saPtr = SA + m_sourceLength + 1;
	unsigned int * endSaPtr = SA;

	while (--saPtr > endSaPtr)
	{
		unsigned int n = *saPtr;
		unsigned int k;
		if (n < DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE)
		{
			if (n)
			{
				if (m_source[n - 1] <= m_source[n])
				{
					#ifdef USE_FAST_TWO_STAGE
						unsigned short s = GetU16(m_source + n - 1);
						if (s == lastS)
						{
							*(--lastSa) = n - 1;
							k = --lastRank;
						}
						else
						{
							if (lastS != -1)
								m_lastRank[lastS] = lastRank;
							lastS = s;
							lastRank = k = --m_lastRank[s];
							lastSa = SA + k;
							*lastSa = n - 1;
						}

						*saPtr = m_source[n - 1] | DONT_CHECK_DURING_TWO_STAGE;

						if ((n > 1) && (m_source[n - 2] > m_source[n - 1]))
							SA[k] |= DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE;	// don't check this index on the remainder of the right to left.
					#else
						unsigned short s = GetU16(m_source + n - 1);
						k = --m_lastRank[s];
						SA[k] = m_source[n - 1] | DONT_CHECK_DURING_TWO_STAGE;
					#endif
				}
			}
		}
	}

	// now do left to right
	SA[0] = DONT_CHECK_DURING_RIGHT_LEFT_TWO_STAGE | m_source[m_sourceLength - 1];//m_sourceLength;
	SA[m_firstRank[m_source[m_sourceLength - 1]]++] = m_sourceLength - 1;

	lastRank = 0;
	lastS = -1;
	saPtr = SA;
	lastSa = 0;
	endSaPtr = SA + m_sourceLength + 1;
	unsigned int * startSaPtr = SA;

	while (++saPtr < endSaPtr)
	{
		unsigned int k;
		unsigned int n = *saPtr;
		if (n < DONT_CHECK_DURING_TWO_STAGE)
		{
			n &= 0x3fffffff;
			if ((n) && (m_source[n - 1] >= m_source[n]))
			{
				#ifdef USE_FAST_TWO_STAGE
					unsigned short s = GetU16(m_source + n - 1);
					if (s == lastS)
					{
						k = lastRank++;
						*(++lastSa) = n - 1;
						*saPtr = (s & 0xff) | DONT_CHECK_DURING_TWO_STAGE;
					}
					else
					{
						if (lastS != -1)
							m_firstRank[lastS] = lastRank;
						lastS = s;
						k = m_firstRank[s]++;
						lastSa = SA + k;
						*lastSa = n - 1;
						lastRank = k + 1;
						*saPtr = (s & 0xff) | DONT_CHECK_DURING_TWO_STAGE;
					}
				#else
					unsigned short s = GetU16(m_source + n - 1);
					*saPtr = m_source[n - 1] | DONT_CHECK_DURING_TWO_STAGE;
					SA[m_firstRank[s]++] = n - 1;
				#endif
			}
			else
			{
				if (n == 0)
					m_bwtIndex = (unsigned int)(saPtr - startSaPtr);
				else
					*saPtr = m_source[n - 1] | DONT_CHECK_DURING_TWO_STAGE;
			}
		}
	}

	unsigned char * bwtPtr = (unsigned char *)m_source;
	m_bwt = bwtPtr;
	for (unsigned int i = 0; i <= m_sourceLength; i++)
	{
		if (i != m_bwtIndex)
			*(bwtPtr++) = SA[i] & 0xff;
	}
	m_completedSA = 0;
	int finish = clock();

	#ifdef VERBOSE
		cout << "Two stage elapsed time: " << (double)(finish - start) / CLOCKS_PER_SEC << " seconds.\n";
	#endif
}








bool MSufSort::VerifySort()
{
	cout << "Checking sort ..." << (char)13;
	unsigned int errorCount = 0;
	
	for (unsigned int i = 1; i <= m_sourceLength; i++)
	{
		if (m_completedSA[i - 1] == m_completedSA[i])
			errorCount++;
		else

			if (!CompareStrings(m_copyOfInput + m_completedSA[i - 1], m_copyOfInput + m_completedSA[i], 
								m_copyOfInput + m_sourceLength))
				errorCount++;

		if ((i & 0xffff) == 0xffff)
			cout << "Checking sort: " << (int)(((double)i / m_sourceLength) * 100) << "% completed ...     " << (char)13;
	}

	if (errorCount)
		cout << "*** " << errorCount << " errors detected.                     \n";
	else
		cout << "                                               " << (char)13;
	return (errorCount == 0);
}






bool MSufSort::BWT(unsigned char * source, unsigned int nBytes, unsigned int & bwtIndex)
{
	// does BWT rather than suffix sort on source of length nBytes
	#ifdef MAINTAIN_COPY_OF_STRING
		if (m_copyOfInput)
			delete [] m_copyOfInput;
		m_copyOfInput = new unsigned char[nBytes];
		memcpy(m_copyOfInput, source, nBytes);
	#endif

	int start = clock();
	Initialize(source, nBytes);
	int finish1 = clock();

	#ifdef VERBOSE
		cout << "Initialization time : " << (double(finish1 - start) / CLOCKS_PER_SEC) << " seconds\n";
	#endif

	unsigned int ** ptr = m_SA;
	while (!m_partitionSize.empty())
	{
		PartitionInfo partitionInfo = m_partitionSize.top();
		m_partitionSize.pop();
		m_nextSortedRank = partitionInfo.m_firstSortedRank;
		ptr = m_SA + partitionInfo.m_partitionOffset;
		Sort(ptr, partitionInfo.m_size);
		ptr += partitionInfo.m_size;
	}
	int finish3 = clock();

	#ifdef VERBOSE
		cout << "Direct sort time : " << (double(finish3 - finish1) / CLOCKS_PER_SEC) << " seconds\n";
	#endif

	// now restore original string from ISA
	RestoreOriginalString();

	// now complete the BWT using the improved two stage sort
	CompleteImprovedTwoStageAsBWT();
	int finish = clock();

	#ifdef VERBOSE
		cout << "Elapsed time: " << (double(finish - start) / CLOCKS_PER_SEC) << " seconds\n";
	#endif

	bool ret = true;
	#ifdef VERIFY_BWT
		unsigned char * temp = new unsigned char[m_sourceLength];
		memcpy(temp, m_bwt, m_sourceLength);
		UnBWT(temp, m_sourceLength, m_bwtIndex);
		for (unsigned int i = 0; i < m_sourceLength; i++)
		{
			if (m_copyOfInput[i] != temp[i])
				ret = false;
		}

		#ifdef VERBOSE
			if (ret)
				cout << "BWT verified\n";
			else
				cout << "BWT ERROR DETECTED\n";
		#endif

		delete [] temp;
	#endif

	if (m_copyOfInput)
		delete [] m_copyOfInput;
	m_copyOfInput = 0;

	bwtIndex = m_bwtIndex;
	return ret;
}






void MSufSort::UnBWT(unsigned char * source, unsigned int nBytes, unsigned int bwtIndex, char * pWorkspace)
{
	// does the reverse BWT on the source provided.
	// reverses the blocksort transform
	#ifdef VERBOSE
		cout << "Reversing the BWT ..." << (char)13;
	#endif

	int start = clock();
	unsigned int * index = (pWorkspace) ? (unsigned int *)pWorkspace : new unsigned int[nBytes + 1];
	unsigned int symbolRange[0x102];
	for (unsigned int i = 0; i < 0x102; i++)
		symbolRange[i] = 0;
	unsigned char * ptr;
	ptr = source;

	symbolRange[0] = 1;		// our -1 symbol
	for (unsigned int i = 0; i < nBytes; i++, ptr++)
		symbolRange[(*ptr) + 1]++;

	
	unsigned int n = 0;
	for (int i = 0; i < 0x101; i++)
	{
		unsigned int temp = symbolRange[i];
		symbolRange[i] = n;
		n += temp;
	}

	n = 0;
	index[0] = bwtIndex;
	for (unsigned int i = 0; i < nBytes; i++, n++)
	{
		if (i == bwtIndex)
			n++;
		unsigned char symbol = source[i];
		index[symbolRange[symbol + 1]++] = n;
	}

	n = bwtIndex;
	unsigned char * reversedBuffer = new unsigned char[nBytes];
	for (unsigned int i = 0; i < nBytes; i++)
	{
		n = index[n];
		if (n >= bwtIndex)
			reversedBuffer[i] = source[n - 1];
		else
			reversedBuffer[i] = source[n];
	}

	memcpy(source, reversedBuffer, nBytes * sizeof(unsigned char));
	int finish = clock();

	#ifdef VERBOSE
		cout << "Reverse BWT elapsed time: " << (double)(finish - start) / CLOCKS_PER_SEC << " seconds.\n";
	#endif

	if (pWorkspace == 0)
		delete [] index;
	delete [] reversedBuffer;
}









bool MSufSort::Sort(unsigned char * source, unsigned int nBytes)
{
	// initialize the SA into partitions with B* suffixes sorted by first two
	// symbols. Also initialize the ISA with ranks based on partial sort of
	// all suffixes according to first two symbols of each suffix.
	bool ret = true;

	#ifdef MAINTAIN_COPY_OF_STRING
		if (m_copyOfInput)
			delete [] m_copyOfInput;
		m_copyOfInput = new unsigned char[nBytes];
		memcpy(m_copyOfInput, source, nBytes);
	#endif

	int start = clock();
	Initialize(source, nBytes);
	int finish1 = clock();

	#ifdef VERBOSE
		cout << "Initialization time : " << (double(finish1 - start) / CLOCKS_PER_SEC) << " seconds\n";
	#endif

	unsigned int ** ptr = m_SA;
	while (!m_partitionSize.empty())
	{
		PartitionInfo partitionInfo = m_partitionSize.top();
		m_partitionSize.pop();
		m_nextSortedRank = partitionInfo.m_firstSortedRank;
		ptr = m_SA + partitionInfo.m_partitionOffset;
		Sort(ptr, partitionInfo.m_size);
		ptr += partitionInfo.m_size;
	}
	int finish3 = clock();

	#ifdef VERBOSE
		cout << "Direct sort time : " << (double(finish3 - finish1) / CLOCKS_PER_SEC) << " seconds\n";
	#endif

	// now restore original string from ISA
	RestoreOriginalString();

	// now complete the improved two stage sort
	CompleteImprovedTwoStage();
	int finish = clock();
	#ifdef VERBOSE
		cout << "Elapsed time: " << (double(finish - start) / CLOCKS_PER_SEC) << " seconds\n";
	#endif
	#ifdef VERIFY_SORT
		ret = VerifySort();
	#endif

	if (m_copyOfInput)
		delete [] m_copyOfInput;
	m_copyOfInput = 0;

	return ret;
}





const char * MSufSort::GetVersion()
{
	// returns const char with version information
	return (const char *)"MSufSort version 3.1.1\0";
}
