#include "Queue.h"


M03_Queue::M03_Queue():m_size(0), m_pStartAddress(0), m_pEndAddress(0)
{
	m_pFront = 0;
	m_pBack = 0;
}



M03_Queue::M03_Queue(char * pAddress, size_t size):m_size(size), m_pStartAddress(pAddress), m_pEndAddress(pAddress + size)
{
	m_pFront = pAddress;
	m_pBack = pAddress;
	count = 0;
}


M03_Queue::~M03_Queue()
{
}



size_t M03_Queue::Allocate(char * pAddress, size_t size)
{
	m_size = size;
	m_pStartAddress = pAddress;
	m_pEndAddress = m_pStartAddress + size;
	m_pFront = m_pBack = m_pStartAddress;
	count = 0;
	return PhysicalSize();
}



size_t M03_Queue::PhysicalSize() const
{
	return (m_size + 5);
}
