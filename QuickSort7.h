#ifndef QUICK_SORT_7_H
#define QUICK_SORT_7_H

#define MIN_LENGTH_FOR_QUICKSORT    8

#ifndef __FORCEINLINE__
#ifdef WIN32
#define __FORCEINLINE__    __forceinline
#else
#define __FORCEINLINE__ __inline__
#endif
#endif



template <class T> __FORCEINLINE__ void Swap(T * const ptrA, T * const ptrB)
{
    T temp = *ptrA;
    *ptrA = *ptrB;
    *ptrB = temp;
}





template <class T> __FORCEINLINE__ const T & SelectPivot(T & value1, T & value2, T & value3)
{
    // middle of three method.

    if (value1 < value2)
        return ((value2 < value3) ? value2 : (value1 < value3) ? value3 : value1);
    return ((value1 < value3) ? value1 : (value2 < value3) ? value3 : value2);
}





template <class T> void QuickSort7(T * array, unsigned int const count)
{
    if (count == 0)
        return;

    if (count < MIN_LENGTH_FOR_QUICKSORT)
        return InsertionSort(array, count);

    T * ptrA = array;
    T * ptrB = 0;
    T * ptrC = array;
    T * endOfArray = array + count - 1;
    T * ptrD = endOfArray;
    T * ptrE = 0;
    T * ptrF = endOfArray;

    T * ptr = array;

    // select the pivot value.
    T pivotB = SelectPivot(array[0], array[count >> 1], *ptrF);
    T pivotA;
    T pivotC;
    T temp;

    while (ptr < ptrD)
    {
        if ((temp = *ptr) == pivotB)
            ptr++;
        else
        {
            if (temp < pivotB)
            {
                if (ptrB == 0)
                {
                    // first element in left partition.
                    ptrB = array;
                    pivotA = temp;
                    ptrC++;
                    Swap(ptrB++, ptr++);
                }
                else
                {
                    if (temp == pivotA)
                    {
                        T temp2 = *ptr;
                        *ptr++ = *ptrC;
                        *ptrC++ = *ptrB;
                        *ptrB++ = temp2;
                    }
                    else
                        if (temp < pivotA)
                        {
                            T temp2 = *ptr;
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
                    ptrE = endOfArray;
                    pivotC = temp;
                    ptrD--;
                    Swap(ptrE--, ptr);
                }
                else
                {
                    if (temp == pivotC)
                    {
                        T temp2 = *ptrD;
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
                            T temp2 = *ptrD;
                            *ptrD-- = *ptrE;
                            *ptrE-- = *ptrF;
                            *ptrF-- = *ptr;
                            *ptr = temp2;
                        }
                }
            }
        }
    }

    if (ptr == ptrD)
        if ((temp = *ptr) == pivotB)
            ptr++;
        else
            if (temp < pivotB)
            {
                if (ptrB == 0)
                {
                    // first element in left partition.
                    ptrB = array;
                    pivotA = temp;
                    ptrC++;
                    Swap(ptrB++, ptr++);
                }
                else
                {
                    if (temp == pivotA)
                    {
                        T temp2 = *ptr;
                        *ptr++ = *ptrC;
                        *ptrC++ = *ptrB;
                        *ptrB++ = temp2;
                    }
                    else
                        if (temp < pivotA)
                        {
                            T temp2 = *ptr;
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
                    ptrE = endOfArray;
                    pivotC = temp;
                    ptrD--;
                    Swap(ptrE--, ptr);
                }
                else
                {
                    if (temp == pivotC)
                    {
                        Swap(ptrD--, ptrE--);
                    }
                    else
                        if (temp < pivotC)
                        {
                            ptrD--;
                        }
                        else
                        {
                            T temp2 = *ptr;
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

    if (partitionA)
    {
        if (partitionA < MIN_LENGTH_FOR_QUICKSORT)
            InsertionSort(array, partitionA);
        else
            QuickSort7(array, partitionA);
    }
    array += partitionA + partitionB;

    if (partitionC)
    {
        if (partitionC < MIN_LENGTH_FOR_QUICKSORT)
            InsertionSort(array, partitionC);
        else
            QuickSort7(array, partitionC);
    }
    array += partitionC + partitionD;

    if (partitionE)
    {
        if (partitionE < MIN_LENGTH_FOR_QUICKSORT)
            InsertionSort(array, partitionE);
        else
            QuickSort7(array, partitionE);
    }
    array += partitionE + partitionF;

    if (partitionG)
    {
        if (partitionG < MIN_LENGTH_FOR_QUICKSORT)
            InsertionSort(array, partitionG);
        else
            QuickSort7(array, partitionG);
    }
}


template <class T> __FORCEINLINE__ void InsertionSort(T * const array, unsigned int const count)
{
    if (count < 3)
    {
        // special handling for less than three elements.
        if ((count == 2) && (array[0] > array[1]))
            Swap(array, array + 1);               
        return;
    }

    T * ptr = array + 1;
    T * endPtr = array + count;

    while (ptr < endPtr)
    {
        T temp = *ptr;
        T * tempPtr = ptr++;
        while ((tempPtr > array) && (tempPtr[-1] > temp))
        {
            tempPtr[0] = tempPtr[-1];
            tempPtr--;
        }
        *tempPtr = temp;
    }
}



#endif