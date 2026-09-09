#include <fstream>
using std::ofstream;

#include <iostream>
using std::cout;

#include <stdint.h>

#include "bitExtraction.h"
#include "chunk.h"

struct Image
{
	uint32_t width, height;
	uint8_t bitDepth, colorType,
		compressionMethod,
		filterMethod,
		interlaceMethod;

	uint32_t* bitMap; 
};

struct TreeNode
{
	int ID = 0;
	int Code = 0;
	TreeNode* branches[2] = {nullptr, nullptr};
};

struct BitStream
{
private:
	uint8_t* byteArray;
    int size;

	int currentByte = 0;
	int offset = 0;
	
public:
	BitStream(uint8_t* byteArray, int size)
	{
		this -> byteArray = byteArray;
		this -> size = size;
	}

	void printCompletion()
	{
		cout << "bytes consumed: " << currentByte << "/" << size << "\n";
	}
	
	bool checkBits(int nbits)
	{
		return offset + nbits < (size - currentByte) * 8; 
	}
	
	int peekBits(int nbits)
	{
		if (nbits > (int)sizeof(nbits) * 8)
			nbits = (int)sizeof(nbits) * 8;

		int result = byteArray[currentByte];

		int extraBytes = (nbits + offset - 1) / 8 + 1;
		for (int i = 1; i < extraBytes; i++)
		{
			result |= byteArray[currentByte + i] << (8 * i);
		}

		return (result >> offset) & ((1 << nbits) - 1);
	}

	int readBits(int nbits)
	{
		if (nbits > (int)sizeof(nbits) * 8)
			nbits = (int)sizeof(nbits) * 8;

		int result = peekBits(nbits);

		offset += nbits;
		currentByte += offset / 8;
		offset %= 8;

		return result;
	}
	
};
  
void initializeImage(Image& image, Chunk& chunk)
{
  
	for (int i = 0; i < 4; i++)
	{
		image.width <<= 8;
		image.width |= chunk.data[i];
		image.height <<= 8;
		image.height |= chunk.data[4+i];
	}
  
	image.bitDepth = chunk.data[8];
	image.colorType = chunk.data[9];
  
	image.compressionMethod = chunk.data[10];
	image.filterMethod = chunk.data[11];
	image.interlaceMethod = chunk.data[12];
  
	std::cout << "dimensions: " << image.width << ", " << image.height << '\n';
	std::cout << "bit depth: " << (int)image.bitDepth << '\n';
	std::cout << "color type: " << (int)image.colorType << '\n';
}

uint8_t window[32000] = {};
int position = 0;
int windowSize = 0;

int lengthCalc(BitStream& stream, uint8_t code)
{
	int bits = (code < 8 || code == 28)? 0: code / 4 - 1;
	int length = (code == 28)? 258: ((code - 4 * bits) * (1 << bits) + 3);

	if (bits != 0)
		length += stream.readBits(bits);

	return length;
}

int distCalc(BitStream& stream, uint8_t code)
{
	int bits = (code < 4)? 0: code / 2 - 1;
	int dist = (code - 2 * bits) * (1 << bits) + 1;

	if (bits != 0)
		dist += stream.readBits(bits);

	return dist;
}

TreeNode* buildHuffman(int N, int M, int b1_count[], uint8_t lengths[]);
void deleteHuffman(TreeNode* root);	
int readHuffmanData(BitStream& stream, TreeNode* root);

void pushNext(uint8_t byte);
void DEFLATE(Chunk& chunk);

int main(int argc, char* argv[])
{
	if (argc != 2)
		return 1;

	std::ifstream inputFile(argv[1], std::ios::binary);
	if (inputFile.fail())
		return 1;

	uint64_t header;
	readBigEndian(inputFile, header);

	if ( ( header & FILETYPE_MASK ) != FILETYPE_PNG )
		return 1;

	Image image;
  
	Chunk currentChunk;
	Chunk idatChunk;
	idatChunk.size = 0;
	idatChunk.type = IDAT_CHUNK;
	idatChunk.data = nullptr;
  
	readChunk(inputFile, currentChunk);
	while (!inputFile.eof() && currentChunk.type != IEND_CHUNK)
	{
		switch (currentChunk.type)
		{
		case IHDR_CHUNK:
			initializeImage(image, currentChunk);
			break;
		case IDAT_CHUNK:
			appendChunk(idatChunk, currentChunk);
			break;
		}
    
		// delete mem alloc and read next chunk
		delete [] currentChunk.data;
		readChunk(inputFile, currentChunk);
	}

	DEFLATE(idatChunk);
	
	inputFile.close();
  
	return 0;
}


TreeNode* buildHuffman(int N, int M, int b1_count[], uint8_t lengths[])
{
	int* next_code = new int[M]();

	next_code[0] = 0;
	b1_count[0] = 0;

	for (int bits = 1; bits < M; bits++)
		next_code[bits] = (next_code[bits - 1] + b1_count[bits - 1]) << 1;

	TreeNode* root = new TreeNode();

	for (int n = 0; n < N; n++)
	{
		int len = lengths[n];
		if (len != 0)
		{
			TreeNode* walker = root;

			for (int i = len - 1; i > 0; i--)
			{
				int j = ((next_code[len] >> i) & 1);
				if (!walker -> branches[j])
					walker -> branches[j] = new TreeNode();
				walker = walker -> branches[j];
			}

			TreeNode*& node = walker-> branches[next_code[len] & 1];

			node = new TreeNode();
			node -> ID = n;
			node -> Code = next_code[len];

			next_code[len]++;
		}
	}

	delete [] next_code;

	return root;
}

void deleteHuffman(TreeNode* root)
{
	if (!root) return;
	deleteHuffman(root -> branches[0]);
	deleteHuffman(root -> branches[1]);
	delete root;
}

int readHuffmanData(BitStream& stream, TreeNode* root)
{
	if (!root) throw std::runtime_error("invalid tree");

	TreeNode* next = root -> branches[stream.peekBits(1)];

	while (next)
	{
		root = next;
		if (!stream.checkBits(1)) throw std::runtime_error("not enough data");
		stream.readBits(1);
		next = root -> branches[stream.peekBits(1)];
	}

	if (!root) throw std::runtime_error("invalid path");

	return root -> ID;
}

int count = 0;

void pushNext(uint8_t byte)
{
	if (windowSize < 32000)
		windowSize++;
	else count++;
	    //cout << (int)window[position] << " ";

	window[position] = byte;

	position = (position + 1) % 32000;
}

void DEFLATE(Chunk& chunk)
{
	BitStream stream(chunk.data, chunk.size);

	while (stream.readBits(8) != 120);
	stream.readBits(8);
	
	bool finalChunk = stream.readBits(1);
	bool staticCompression = stream.readBits(1);
	bool dynamicCompression = stream.readBits(1);

	// both compressions are not supported in a single run
	if (staticCompression && dynamicCompression)
		return;
	
	TreeNode* huffmanTree = nullptr;
	TreeNode* literalLengthCodes = nullptr;
	TreeNode* distanceCodes = nullptr;

	uint8_t HLIT = 28;
	uint8_t HDIST = 29;

	int* b1_count = nullptr;
	uint8_t* codeLengths = nullptr;

	if (dynamicCompression)
	{
		HLIT = stream.readBits(5);
		HDIST = stream.readBits(5);
		uint8_t HCLEN = stream.readBits(4);

		b1_count = new int[8]();
	    codeLengths = new uint8_t[19]();

		int clen_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

		for (int i = 0; i < HCLEN + 4; i++)
		{
			codeLengths[clen_order[i]] = stream.readBits(3);
			b1_count[codeLengths[clen_order[i]]]++;
		}

		huffmanTree = buildHuffman(19, 8, b1_count, codeLengths);

		//cout << "huffman tree:\n";
		//printHuffmanData(huffmanTree, 0, 0);
		//cout << "\n\n";
		
		delete [] codeLengths;
		codeLengths = new uint8_t[HLIT + HDIST + 258]();
		for (int i = 0; i < HLIT + HDIST + 258; i++)
		{
			int ID = readHuffmanData(stream, huffmanTree);

			int j = 0;
			switch (ID)
			{
			case 18:
				j = 11 + stream.readBits(7);
				for (int k = 0; k < j; k++)
				{
					codeLengths[i + k] = 0;
				}
				i += (j - 1);
				break;
			case 17:
				j = 3 + stream.readBits(3);
				for (int k = 0; k < j; k++)
				{
					codeLengths[i + k] = 0;
				}
				i += (j - 1);
				break;
			case 16:
				j = 3 + stream.readBits(2);
				for (int k = 0; k < j; k++)
				{
					codeLengths[i + k] = codeLengths[i - 1];
				}
				i += (j - 1);
				break;
			default:
				codeLengths[i] = ID;
			}
		}
		deleteHuffman(huffmanTree);
	}
	else if (staticCompression)
	{
		delete [] codeLengths;
		codeLengths = new uint8_t[HLIT + HDIST + 258]();

		for (int i = 0; i < HLIT + HDIST + 258; i++)
		{
			codeLengths[i] = i < 144? 8: i < 256? 9: i < 280? 7: 8;
		}
	}

	delete [] b1_count;
	b1_count = new int[16]();

	for (int i = 0; i < HLIT + 257; i++)
	{
		b1_count[codeLengths[i]]++;
	}
	
	literalLengthCodes = buildHuffman(HLIT + 257, 16, b1_count, codeLengths);
	
	delete [] b1_count;
	b1_count = new int[16]();

	for (int i = 0; i < HDIST + 1; i++)
	{
		b1_count[codeLengths[i + HLIT + 257]]++;
	}
	
	distanceCodes = buildHuffman(HDIST + 1, 16, b1_count, codeLengths + (HLIT + 257));

	ofstream file("OUTPUT.txt");
	
	while (stream.checkBits(1))
	{
		int out = readHuffmanData(stream, literalLengthCodes);

		if (out < 256)
		{
			file << out << ", ";
			pushNext(out);
		}
		else if (out == 256)
		{
			cout << "end case\n";
			break;
		}
		else
		{
			out -= 257;
			int length = lengthCalc(stream, out);

			int dist = distCalc(stream, readHuffmanData(stream, distanceCodes));

			int copyPos = (position - dist) % 32000;
			if (copyPos < 0)
				copyPos += 32000;

			for (int i = 0; i < length; i++)
			{
				file << (int)window[(copyPos + i) % 32000] << ", ";
				pushNext(window[(copyPos + i) % 32000]);
			}
		}
	}

	deleteHuffman(literalLengthCodes);
	deleteHuffman(distanceCodes);
	
	for (int i = 0; i < 32000; i++)
	{
		count++;
		//cout << (int)window[(position + i) % 32000] << " ";
	}
	
    cout << "count: " << count << "\n";
	stream.printCompletion();
	if (finalChunk) return;
}
