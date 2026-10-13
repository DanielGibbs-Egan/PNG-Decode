#include <fstream>
using std::ofstream;

#include <iostream>
using std::cout;

#include <cstdio>
#include <cstdint>
#include <string>

#include "bitExtraction.h"
#include "chunk.h"

struct Image
{
	// dimensions
	std::uint32_t width, height;
	// format information
	uint8_t bitDepth, colorType,
		compressionMethod,
		filterMethod,
		interlaceMethod;
	// image data
	uint32_t* bitMap; 
};

struct TreeNode
{
	// stored data
	int ID = 0;
	// local path
	int Code = 0;
	// child nodes
	TreeNode* branches[2] = { nullptr, nullptr };
};

// Byte window for LZ77
struct Window
{
	uint8_t bytes[32000] = {}; // byte window
	int position = 0;			// location in window
	int windowSize = 0;			// current size of window
	int count = 0;				// number of characters read past window size

	// add a new byte to the window
	void pushNext(uint8_t byte)
	{
		if (windowSize < 32000)
			windowSize++;
		else
			count++;

		bytes[position] = byte;

		position = (position + 1) % 32000;
	}
};

// store the IDAT image data
void initializeImage(Image& image, Chunk& chunk)
{
  
	// 4 bytes == 32 bits
	for (int i = 0; i < 4; i++)
	{
		// shift the dimension variables to the left 8 bits
		image.width <<= 8;
		image.height <<= 8;

		// copy the next byte to the dimension variable
		image.width |= chunk.data[i];
		image.height |= chunk.data[4+i];
	}

	// format information

	image.bitDepth = chunk.data[8];
	image.colorType = chunk.data[9];
  
	image.compressionMethod = chunk.data[10];
	image.filterMethod = chunk.data[11];
	image.interlaceMethod = chunk.data[12];

	// output data

	std::cout << "dimensions: " << image.width << ", " << image.height << '\n';
	std::cout << "bit depth: " << (int)image.bitDepth << '\n';
	std::cout << "color type: " << (int)image.colorType << '\n';
}


// calculate window copy length fron code
int lengthCalc(BitStream& stream, uint8_t code)
{
	// calculate the number of extra bits to read from the stream
	int bits = (code < 8 || code == 28)? 0: code / 4 - 1;

	// determine the unaltered length from the code
	int length = (code == 28)? 258: ((code - 4 * bits) * (1 << bits) + 3);

	// check if the code requires the extraction of extra bits
	if (bits != 0)
		length += stream.readBits(bits);

	return length;
}

// calculate the distance to move back in the window fron code
int distCalc(BitStream& stream, uint8_t code)
{
	// calculate the number of extra bits to read from the stream
	int bits = (code < 4)? 0: code / 2 - 1;

	// determine the unaltered distance from the code
	int dist = (code - 2 * bits) * (1 << bits) + 1;

	// check if the code requires the extraction of extra bits
	if (bits != 0)
		dist += stream.readBits(bits);

	return dist;
}

TreeNode* buildHuffman(int M, int b1_count[], int N, uint8_t lengths[]);
void deleteHuffman(TreeNode*& root);
int readHuffmanData(BitStream& stream, TreeNode* root);

void DEFLATE(Chunk& chunk);

int main(int argc, char* argv[])
{
	// check if the program is being run with correct number of inputs
	if (argc != 2)
		return 1;

	// attempt to open a file, stop the program if unsuccessful
	std::ifstream inputFile(argv[1], std::ios::binary);
	if (inputFile.fail())
		return 1;

	// read the file header
	uint64_t header;
	readBigEndian(inputFile, header);
	
	// compare the file header to the PNG file header
	if ( ( header & FILETYPE_MASK ) != FILETYPE_PNG )
		return 1;

	// final image object
	Image image;
  
	// store most recent chunk extracted from the file
	Chunk currentChunk;

	// initalize the image data (IDAT) chunk object
	Chunk imageData;
	imageData.size = 0;
	imageData.type = IDAT_CHUNK;
	imageData.data = nullptr;
  
	// extract the next chunk
	readChunk(inputFile, currentChunk);
	while (!inputFile.eof() && currentChunk.type != IEND_CHUNK)
	{
		// check the chunk type
		switch (currentChunk.type)
		{
		// use IHDR to get image information
		case IHDR_CHUNK:
			initializeImage(image, currentChunk);
			break;
		// combine IDAT chunks to create one image data chunk
		case IDAT_CHUNK:
			appendChunk(imageData, currentChunk);
			break;
		}

		// free memory
		delete [] currentChunk.data;

		// extract the next chunk
		readChunk(inputFile, currentChunk);

	}

	// use DEFLATE algorithm on PNG images compressed data
	DEFLATE(imageData);
	
	// free memory used by the file object
	inputFile.close();
  
	// the program ran successfully
	return 0;
}

TreeNode* buildHuffman(int M, int b1_count[], int N, uint8_t lengths[])
{
	// create an array to store the initial code value for each code length
	int* len_code = new int[M]();

	// set the first initial code to 0
	len_code[0] = 0;
	// make sure there are no codes with 0 length
	b1_count[0] = 0;

	// each initial code is the sum of the previous initial code 
	// and the number of code lengths it has shifted by one bit
	for (int len = 1; len < M; len++)
		len_code[len] = (len_code[len - 1] + b1_count[len - 1]) << 1;

	// create the tree
	TreeNode* root = new TreeNode();

	// create tree nodes 
	for (int n = 0; n < N; n++)
	{
		// get the code length of the current code
		int len = lengths[n];

		// if the code length is non zero
		if (len != 0)
		{
			// initialize a walker at the tree root
			TreeNode* walker = root;

			// starting from the left most bit
			for (int i = len - 1; i > 0; i--)
			{
				// select the ith bit 
				int j = ((len_code[len] >> i) & 1);
				// make sure a branch exists
				if (!walker->branches[j])
					walker->branches[j] = new TreeNode();
				// step the walker along the branch
				walker = walker->branches[j];
			}

			// select the branch that will lead to the leaf node 
			TreeNode*& node = walker->branches[len_code[len] & 1];

			// create the leaf node
			node = new TreeNode();
			node->ID = n;
			node->Code = len_code[len];

			// increment the initial code 
			len_code[len]++;
		}
	}

	// free memory
	delete [] len_code;

	return root;
}

void deleteHuffman(TreeNode*& root)
{
	// check if the given root exists
	if (!root) return;
	// remove children
	deleteHuffman(root -> branches[0]);
	deleteHuffman(root -> branches[1]);
	// free memory from current node
	delete root;
	// remove access to free'd memory
	root = nullptr;
}

int readHuffmanData(BitStream& stream, TreeNode* root)
{
	// check if the huffman tree exists
	if (!root) throw std::runtime_error("invalid tree");

	// store the next node
	TreeNode* next = root -> branches[stream.peekBits(1)];

	// keep looping while child exists
	while (next)
	{
		// progress the search
		root = next;

		// check if there is data in the stream
		if (!stream.checkBits(1))
			throw std::runtime_error("not enough data");

		// progress the stream
		stream.readBits(1);

		// obtain the next node, in case search continues
		next = root -> branches[stream.peekBits(1)];
	}

	// return the ID of the found node
	return root -> ID;
}

Window window;

void DEFLATE(Chunk& chunk)
{
	// create a bit stream for the compressed image data
	BitStream stream(chunk.data, chunk.size);

	// read data until start code is found
	while (stream.readBits(8) != 120);

	// read data until start code is found
	stream.readBits(8);
	
	bool finalChunk = stream.readBits(1);
	bool staticCompression = stream.readBits(1);
	bool dynamicCompression = stream.readBits(1);

	// both compressions are not supported in a single run
	if (staticCompression && dynamicCompression)
		return;
	
	// variables for accessing tree roots
	TreeNode* huffmanTree = nullptr;
	TreeNode* literalLengthCodes = nullptr;
	TreeNode* distanceCodes = nullptr;

	// initialize variables for static compression
	uint8_t HLIT = 28;
	uint8_t HDIST = 29;

	// pointer variables
	int* b1_count = nullptr;
	uint8_t* codeLengths = nullptr;

	if (dynamicCompression)
	{
		// update and initalize variables for dynamic compression
		HLIT = stream.readBits(5);
		HDIST = stream.readBits(5);
		uint8_t HCLEN = stream.readBits(4); // number of optional codes

		// initialize pointer variables
		b1_count = new int[8]();
	    codeLengths = new uint8_t[19]();

		// map the (4) default codes and (15) optional codes to a linear index
		int clen_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

		// add the lengths of the (4) default codes and (HCLEN) optional codes  
		for (int i = 0; i < 4 + HCLEN; i++)
		{
			// get the length of the code in index (i)
			codeLengths[clen_order[i]] = stream.readBits(3);
			// count the number of codes with a specific length
			b1_count[codeLengths[clen_order[i]]]++;
		}

		// create a huffman tree with max code length of (8) and (19) unique codes
		huffmanTree = buildHuffman(8, b1_count, 19, codeLengths);

		// code lengths and their amounts no longer needed
		delete[] b1_count;
		delete [] codeLengths;

		// initialize a new array to store huffman code lengths for LZ77 image decoding
		codeLengths = new uint8_t[HLIT + HDIST + 258]();

		// for each code length
		int i = 0;
		while ( i < HLIT + HDIST + 258 )
		{
			// determine the code using the huffman tree
			int ID = readHuffmanData(stream, huffmanTree);
			std::cout << ID << "\n";
			// initialize variables
			int j = 0;

			switch (ID)
			{
			case 18:
				// find the number of codes to zero 
				j = 11 + stream.readBits(7);
				// zero the codes 
				for (int k = 0; k < j; k++)
				{
					codeLengths[i] = 0;
					i++;
				}
				break;
			case 17:
				// find the number of codes to zero 
				j = 3 + stream.readBits(3);
				// zero the codes 
				for (int k = 0; k < j; k++)
				{
					codeLengths[i] = 0;
					i++;
				}
				break;
			case 16:
				// find the number of codes to copy
				j = 3 + stream.readBits(2);
				// copy the previous code
				for (int k = 0; k < j; k++)
				{
					codeLengths[ i ] = codeLengths[i - 1];
					i++;
				}
				break;
			default:
				// use the given code
				codeLengths[i] = ID;
				i++;
			}
		}

		// current huffman tree can be free'd from memory
		deleteHuffman(huffmanTree);

	}
	else if (staticCompression)
	{
		// initialize a new array to store huffman code lengths for LZ77 image decoding
		codeLengths = new uint8_t[HLIT + HDIST + 258]();

		// update each code length to the default value
		for (int i = 0; i < HLIT + HDIST + 258; i++)
		{
			codeLengths[i] = i < 144? 8: i < 256? 9: i < 280? 7: 8;
		}
	}

	// store the number of codes of length with a max length of (15)
	b1_count = new int[16]();
	// count the codes of each length 
	for (int i = 0; i < HLIT + 257; i++)
	{
		b1_count[codeLengths[i]]++;
	}
	
	// create a huffman tree with max code length of (16) and (HLIT + 257) unique codes
	literalLengthCodes = buildHuffman(16, b1_count, HLIT + 257, codeLengths);

	// code lengths amounts no longer needed
	delete [] b1_count;

	// store the number of codes of length with a max length of (15)
	b1_count = new int[16]();

	// count the codes of each length 
	for (int i = 0; i < HDIST + 1; i++)
	{
		b1_count[codeLengths[i + HLIT + 257]]++;
	}

	// create a huffman tree with max code length of (16) and (HDIST + 1) unique codes
	// obtain the codelengths for distance codes by shifting over HLIT + 257 indices
	distanceCodes = buildHuffman(16, b1_count, HDIST + 1, codeLengths + (HLIT + 257));

	ofstream file("OUTPUT.txt");
	
	// repeat while there is data to read
	while (stream.checkBits(1))
	{
		// get the next data segment using the huffman tree
		int out = readHuffmanData(stream, literalLengthCodes);

		if (out < 256)
		{
			// save the byte value and seperator
			file << out << ", ";
			// progress the window
			window.pushNext(out);
			continue;
		}
		else if (out == 256)
		{
			// end case reached
			cout << "file end\n";
			break;
		}
		else
		{
			// offset read data for calculations
			out -= 257;
			// find the length of data to copy
			int length = lengthCalc(stream, out);
			// find the initial posistion offset for the data to copy
			int dist = distCalc(stream, readHuffmanData(stream, distanceCodes));

			// locate the initial position of the data
			int copyPos = (window.position - dist) % 32000;
			if (copyPos < 0)
				copyPos += 32000;

			// copy (length) bytes from the window 
			for (int i = 0; i < length; i++)
			{
				// copy the previous byte from window
				file << (int)window.bytes[copyPos] << ", ";
				// progress the window with the copied data
				window.pushNext(window.bytes[copyPos]);
				// progress the copy position
				copyPos = (copyPos + 1) % 32000;
			}
		}
	}


	// free trees from memory
	deleteHuffman(literalLengthCodes);
	deleteHuffman(distanceCodes);
	
	// output the amount of bytes used and the total amount of bytes
	stream.printCompletion();

	// if there are no more chunks end
	if (finalChunk) return;
	
	// TODO: support for post IDAT Chunks
}
