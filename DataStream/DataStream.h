#ifndef DATA_STREAM_H
#define DATA_STREAM_H

#include "stdio.h"
class RangeEncoder;


class EncodeDataStream
{
public:


	EncodeDataStream(FILE * outputFile, unsigned int bufferSize = (1 << 12)):
	m_count(0), 
	m_outputFile(outputFile), 
	m_bufferSize(bufferSize),
	m_outputSize(0)
	{
		m_stream = new unsigned char[bufferSize];
	}



	virtual ~EncodeDataStream()
	{
		Flush();
		delete [] m_stream;
	}



	void Write8(unsigned char value)
	{
		m_stream[m_count++] = value;
		if (m_count >= m_bufferSize)
			Flush();
	}


	void Flush()
	{
		m_outputSize += m_count;
		if (m_count)
			fwrite(m_stream, 1, m_count, m_outputFile);
		m_count = 0;
	}

	unsigned int GetOutputSize(){return (m_outputSize + m_count);}


	FILE * GetOutputFile(){return m_outputFile;}

protected:

	unsigned int		m_outputSize;

	FILE *				m_outputFile;

	unsigned char *		m_stream;

	unsigned int		m_count;

	unsigned int		m_bufferSize;
};








class DecodeDataStream
{
public:

	DecodeDataStream(FILE * inputFile, unsigned int bufferSize = (1 << 12)):m_count(0), m_inputFile(inputFile), m_bufferSize(bufferSize)
	{
		m_stream = new unsigned char[bufferSize];
		Clear();
	}

	virtual ~DecodeDataStream()
	{
		delete [] m_stream;
	}


	unsigned char Read8()
	{
		unsigned char ret = m_stream[m_count++];
		if (m_count >= m_bufferSize)
			Clear();
		return ret;
	}

	
	void Clear()
	{
		fread(m_stream, 1, m_bufferSize, m_inputFile);
		m_count = 0;
	}

	FILE * GetInputFile(){return m_inputFile;}

private:

	FILE *			m_inputFile;

	unsigned char *	m_stream;

	unsigned int	m_count;

	unsigned int	m_bufferSize;
};



#endif
