#ifndef CHUNK_H
#define CHUNK_H

const uint64_t IHDR_CHUNK = (((((((
          0ULL | 'I')
         << 8) | 'H')
         << 8) | 'D')
         << 8) | 'R');

const uint64_t IDAT_CHUNK = (((((((
          0ULL | 'I')
         << 8) | 'D')
         << 8) | 'A')
         << 8) | 'T');

const uint64_t IEND_CHUNK = (((((((
          0ULL | 'I')
         << 8) | 'E')
         << 8) | 'N')
         << 8) | 'D');

struct Chunk
{
  uint32_t size;
  uint32_t type;
  uint8_t* data;
  uint32_t CRC;
};

bool crcCheck(Chunk chunk)
{
  return true;
}

void printChunk(Chunk chunk)
{
  std::cout << "Chunk:\n";

  std::cout << "size: " << chunk.size << "\n";

  std::cout << "type: "
            << (uint8_t)((chunk.type >> 24) & 255)
            << (uint8_t)((chunk.type >> 16) & 255)
            << (uint8_t)((chunk.type >> 8) & 255)
            << (uint8_t)(chunk.type & 255)
            << '\n';

  std::cout << "data: ";
  for (uint32_t i = 0; i < chunk.size; i++)
  {
    for (int j = 0; j < 8; j++)  
    {   
      std::cout << ((chunk.data[i] >> (7-j)) & 1);
    }
    std::cout << " ";
  }

  std::cout << "\nCRC: ";
  printBits(chunk.CRC);

}
void printChunkHex(Chunk chunk)
{
  std::cout << "Chunk:\n";

  std::cout << "size: " << chunk.size << "\n";

  std::cout << "type: "
            << (uint8_t)((chunk.type >> 24) & 255)
            << (uint8_t)((chunk.type >> 16) & 255)
            << (uint8_t)((chunk.type >> 8) & 255)
            << (uint8_t)(chunk.type & 255)
            << '\n';

  std::cout << "data: ";
  for (uint32_t i = 0; i < chunk.size; i++)
  {
    std::cout << (uint32_t)chunk.data[i] << " ";
  }

  std::cout << "\nCRC: ";
  printBits(chunk.CRC);

}

void readChunk(std::ifstream& file, Chunk& chunk)
{
  readBigEndian(file, chunk.size);
  readBigEndian(file, chunk.type);
  
  chunk.data = new uint8_t[chunk.size]();
  file.read(reinterpret_cast<char*>(chunk.data), chunk.size);
  
  readBigEndian(file, chunk.CRC);

  if (!crcCheck(chunk))
    return;
}

void appendChunk(Chunk& chunk, Chunk& appendingChunk)
{
	uint32_t totalSize = chunk.size + appendingChunk.size;
	uint8_t* combinedData = new uint8_t[totalSize]();
	for (int i = 0; i < (int)chunk.size; i++)
	{
		combinedData[i] = chunk.data[i];
	}
	for (int i = 0; i < (int)appendingChunk.size; i++)
	{
		combinedData[(int)chunk.size + i] = appendingChunk.data[i];
	}
	chunk.size = totalSize;

  
	delete [] chunk.data;
	chunk.data = combinedData;
  
}

#endif
