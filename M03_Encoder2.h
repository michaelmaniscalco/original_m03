#ifndef M03_ENCODER2_H
#define M03_ENCODER2_H

#include <boost/scoped_array.hpp>

namespace M03
{


	typedef enum
	{
		SKIP	= 0x00,		// a partition to skip over
		SCAN	= 0x04,		// a partition to scan and count
		LAST	= 0x08,		// last 'SCAN' partition in a context
		END		= 0x0C,		// end of a partitions for given order

		FLAGS_MASK = (SKIP | SCAN | LAST | END)
	} PARITION_FLAGS;

	class Queue
	{
	public:

		void Initialize(char * pAddress, size_t size, size_t maxIndex);

		void Push(size_t size, PARITION_FLAGS flags);

		void Pop(size_t & size, PARITION_FLAGS & flags);

	protected:

	private:

		void Write(size_t size, PARITION_FLAGS flags);

		void Read(size_t & size, PARITION_FLAGS & flags);

		char * 	m_pAddress;

		char * 	m_pEndAddress;

		char *	m_pRead;

		char *	m_pWrite;

		enum 
		{
			SIZE_1 = 0x00, 
			SIZE_2 = 0x40, 
			SIZE_4 = 0x80,

			SIZE_MASK = (SIZE_1 | SIZE_2 | SIZE_4)
		};

	}; // class Queue



	class Encoder
	{
	public:

		void Encode(const char *, size_t);

	protected:

	private:

		// initialize the RLE array
		void InitializeRLE();

		// initialize the first/last 16-bit index array
		void InitializeIndex();

		boost::scoped_array<unsigned char>	m_spRLE;
		const unsigned char *	m_pRLE;

		boost::scoped_array<unsigned short>	m_spIndex;
		const unsigned short *	m_pIndex;

		boost::scoped_array<char>	m_spQueue;
		Queue					m_pQueue[0x100];

		const unsigned char *	m_pData;


		size_t					m_sentinelIndex;

		size_t					m_szData;

		size_t					m_symbolFirstIndex[0x100];

		size_t					m_symbolCount[0x100];
	}; // class Encoder



} // namespace M03


#endif // M03_ENCODER2_H
