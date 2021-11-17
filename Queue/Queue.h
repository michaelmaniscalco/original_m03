#ifndef M03_QUEUE_H
#define M03_QUEUE_H

#include <iostream>


class M03_Queue
{
public:

	M03_Queue(char * pAddress, size_t size);

	M03_Queue();

	virtual ~M03_Queue();

	size_t Allocate(char * pAddress, size_t size);

	size_t PhysicalSize() const;

	const char * GetBack() const;

	const char * GetFront() const;

	void Push(size_t value, size_t flags, size_t payload = 0);

	size_t Front() const;

	size_t Payload() const;

	size_t Flags() const;

	void Pop();

	bool Empty() const;

	void * Get() const{return m_pStartAddress;}
protected:

private:

	char *		m_pStartAddress;

	char *		m_pEndAddress;

	size_t		m_size;

	char *		m_pFront;

	char *		m_pBack;

	enum {SIZE_1 = 0x00, SIZE_5 = 0x80};

	int count;
};




__forceinline const char * M03_Queue::GetBack() const
{
	return m_pBack;
}



__forceinline const char * M03_Queue::GetFront() const
{
	return m_pFront;
}




__forceinline void M03_Queue::Push(size_t value, size_t flags, size_t payload)
{
	// payload is context sensitive info for use in stats models (bits 6,5,4 of 7-0)
	value--;	// zero can not be pushed
	if (value < 4)
	{
		// one byte of space needed
		char pushValue = (char)(value | (flags << 2)) | (payload);
	//	pushValue &= 0x8f;

		*(m_pBack++) = pushValue;
		count += 1;
	}
	else
	{
		// five bytes of space needed
		char pushValue = (char)(SIZE_5 | (flags << 2)) | (payload);;
		pushValue &= 0x8f;

		*(m_pBack++) = pushValue;
		*(unsigned int *)m_pBack = (unsigned int)value;
		m_pBack += 4;
		count += 5;
	}

	if (m_pBack >= m_pEndAddress)
		m_pBack = m_pStartAddress;	

	if (count >= m_size)
	{
		std::cout << "ERROR";
		std::cin.get();
	}
}



__forceinline size_t M03_Queue::Front() const
{
	if ((*m_pFront) & SIZE_5)
	{
		// size >= 4
		return (*(unsigned int *)(m_pFront + 1) + 1);
	}
	else
	{
		// size < 4
		return (((*m_pFront) & 0x03) + 1);
	}
}



__forceinline size_t M03_Queue::Flags() const
{
	return (((*m_pFront) & 0x7C) >> 2);
}



__forceinline void M03_Queue::Pop()
{
	if (*m_pFront & SIZE_5)
	{
		count -= 5;
		m_pFront += 5;
	}
	else
	{
		count -= 1;
		m_pFront += 1;
	}

	if (m_pFront >= m_pEndAddress)
		m_pFront = m_pStartAddress;	

	if (count < 0)
		int y = 9;
}



__forceinline bool M03_Queue::Empty() const
{
	return (m_pFront == m_pBack);
}


#endif
