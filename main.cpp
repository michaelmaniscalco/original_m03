#include "M03_Encoder.h"
#include "M03_Decoder.h"
#include "M03_Encoder2.h"

#include <ios>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <boost/shared_array.hpp>
#include "time.h"
#include "conio.h"

using namespace std;
using namespace boost;




bool ConfirmOverwrite(const char * pPath)
{
	cout << "File " << pPath << " already exists.  Confirm overwrite (Y/N)";
	char c = cin.get();
	if ((c == 'y') || (c == 'Y'))
		return true;
	return false;
}



bool FileExists(const char * pPath)
{
return false;
	FILE * fp = fopen(pPath, "rb");
	if (!fp)
		return false;
	fclose(fp);
	return true;
}




size_t GetFileSize(const char * sourcePath)
{	
	fstream fIn(sourcePath, ios::binary | ios_base::in);
	size_t size = 0;
	if (fIn.fail())
		return 0;
	fIn.seekp(0, ios_base::end);
	size = fIn.tellp();
	fIn.close();
	return size;
}




void EncodeFile(const char * pPath, const char * pEncPath, size_t blocksize)
{
	if (FileExists(pEncPath))
		if (!ConfirmOverwrite(pEncPath))
			return;

	size_t size = GetFileSize(pPath);
	size_t rawSize = size;
	size_t compressedSize = 0;
	double elapsedTime = 0;

	if (size)
	{
		FILE * fpIn = fopen(pPath, "rb");
		if (!fpIn)
		{

			cout << "Failed to open input file " << pPath << endl;
			return;
		}		

		FILE * fpOut = fopen(pEncPath, "wb");
		if (!fpOut)
		{
			fclose(fpIn);			
			cout << "Failed to create output file " << pEncPath << endl;
			return;
		}


		fwrite((char *)&blocksize, 1, sizeof(size_t), fpOut);
		fwrite((char *)&size, 1, sizeof(size_t), fpOut);

		EncodeDataStream dataStream(fpOut);
		RangeEncoder rangeEncoder(&dataStream);
		rangeEncoder.init();
		M03::Encoder encoder2;
		M03_Encoder * pEncoder = new M03_Encoder(&rangeEncoder);
		size_t startingOffset = 0;
		
		shared_array<char> spData;
		int start = clock();
		while (size)
		{
			unsigned int bytesToEncode = size;
			if (bytesToEncode > blocksize)
				bytesToEncode = blocksize;
			cout << "Encoding bytes " << startingOffset << " -> " << (startingOffset + bytesToEncode) << (char)13;
			size_t allocationSpaceRequired = (((bytesToEncode + 1) * 6) + 4096 + (2048 * 0x101));
			if (!spData)
				spData.reset(new char[allocationSpaceRequired]);
			const char * pEnd = spData.get() + allocationSpaceRequired;
			fread(spData.get(), 1, bytesToEncode, fpIn);
		//	encoder2.Encode(spData.get(), bytesToEncode);
			pEncoder->Encode((unsigned char *)spData.get(), bytesToEncode);
			startingOffset += bytesToEncode;
			size -= bytesToEncode;
			spData.reset();
		}

		rangeEncoder.done();
		compressedSize = ftell(fpOut);
		fclose(fpOut);
		fclose(fpIn);
		delete pEncoder;

		int finish = clock();
		elapsedTime = ((double)(finish - start) / CLOCKS_PER_SEC);
	}

	cout << "                                                  " << (char)13;
	cout << "Compressed " << rawSize << " -> " << compressedSize << endl;
	cout << "Time: " << elapsedTime << " seconds" << endl;
}




void DecodeFile(const char * pPath, const char * pDecPath)
{
	if (FileExists(pDecPath))
		if (!ConfirmOverwrite(pDecPath))
			return;

	FILE * fpIn = fopen(pPath, "rb");
	if (!fpIn)
	{	
		cout << "Failed to open input file " << pPath;
		return;
	}

	FILE * fpOut = fopen(pDecPath, "wb");
	if (!fpOut)
	{
		cout << "Failed to create output file " << pDecPath << endl;
		fclose(fpIn);
		return;
	}

	size_t blocksize;
	size_t decSize;
	fread((char *)&blocksize, 1, sizeof(size_t), fpIn);
	fread((char *)&decSize, 1, sizeof(size_t), fpIn);

	DecodeDataStream dataStream(fpIn);
	RangeDecoder rangeDecoder(&dataStream);
	rangeDecoder.init();

	M03_Decoder * pDecoder = new M03_Decoder(&rangeDecoder);
	int start = clock();

	size_t bytesDecoded = 0;

	while (decSize)
	{
		unsigned char * pData = 0;
		size_t bytesToDecode = decSize;
		if (bytesToDecode > blocksize)
			bytesToDecode = blocksize;
		cout << "Decoding bytes " << bytesDecoded << " -> " << (bytesDecoded + bytesToDecode) << (char)13;
		pDecoder->Decode(pData, bytesToDecode);
		bytesDecoded += bytesToDecode;
		fwrite(pData, 1, bytesToDecode, fpOut);
		delete [] pData;
		decSize -= bytesToDecode;
	}

	int finish = clock();
	cout << "                                         " << (char)13;
	cout << "Time: " << ((double)(finish - start) / CLOCKS_PER_SEC) << " seconds" << endl;
	fclose(fpIn);
	fclose(fpOut);
}



bool IsM03(const char * pPath)
{
	size_t szPath = strlen(pPath);
	if ((szPath >= 4) && ((*(int *)(pPath + szPath - 4) == *(int *)".m03") || (*(int *)(pPath + szPath - 4) == *(int *)".M03")))
		return true;
	return false;
}




std::string MakeDecodePath(const char * pPath)
{
	size_t len = strlen(pPath);
	return std::string(pPath, len - 4);
}




std::string MakeEncodePath(const char * pPath)
{
	std::string ret(pPath);
	ret += ".M03";
	return ret;
}



void Usage()
{
	cout << "M03.exe e blocksize source dest" << endl;
	cout << "M03.exe d source dest" << endl;

	cout << endl << "Quick compress (max blocksize = 128MB)" << endl;
	cout << "M03.exe source" << endl;

	cout << endl << "Quick decompress " << endl;
	cout << "M03.exe source.M03" << endl;

}



void main(int argc, char * argv[])
{
	cout << endl;
	cout << "M03 (Beta) 0.3b - Context based BWT compressor (2.14.2010)" << endl;
	cout << "Author: Michael A. Maniscalco" << endl;
	cout << "email: michael@michael-maniscalco.com" << endl;
	cout << endl;

	if (argc < 2)
	{
		Usage();
		return;
	}

	if (strlen(argv[1]) != 1)
	{
		// second arg is not length of 1.  last chance is quick compress/decompress option
		if (argc == 2)
		{
			bool isCompressed = IsM03(argv[1]);
			if (isCompressed)
			{
				// quick decode
				std::string decodePath = MakeDecodePath(argv[1]);			
				DecodeFile(argv[1], decodePath.c_str());
			}
			else
			{
				// quick encode
				std::string encodePath = MakeEncodePath(argv[1]);
				#define MAX_QUICK_ENCODE_BLOCK_SIZE ((1 << 20) * 128)
				EncodeFile(argv[1], encodePath.c_str(), MAX_QUICK_ENCODE_BLOCK_SIZE);
			}
			getch();
		}

		Usage();
		return;	
	}

	switch (argv[1][0])
	{
		case 'e':
		{
			if (argc != 5)
			{
				Usage();
				return;
			}

			size_t blocksize = atoi(argv[2]);
			EncodeFile(argv[3], argv[4], blocksize);
			break;
		}

		case 'd':
		{
			if (argc != 4)
			{
				Usage();
				return;
			}

			DecodeFile(argv[2], argv[3]);
			break;
		}

		default:
		{
			Usage();
			break;
		}
	}

	cin.get();
}
