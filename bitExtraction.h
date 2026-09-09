#ifndef BITEXTRACTION_H
#define BITEXTRACTION_H

const uint8_t BYTE_MASK = 255;
const uint64_t FILETYPE_MASK = ((((((
                 0ULL
         << 8) | BYTE_MASK)
         << 8) | BYTE_MASK)
         << 8) | BYTE_MASK)
         << 32;

const uint64_t FILETYPE_PNG = (((((
          0ULL | 'P')
         << 8) | 'N')
         << 8) | 'G')
         << 32;

template <typename T>
void printBits(T data)
{
  T backwards;

  for (int i = 0; i < 8 * (int)sizeof(T); i++)
  {
    backwards = backwards << 1;
    backwards |= ((data >> i) & 1);
  }
  
  for (int i = 0; i < 8 * (int)sizeof(T); i++)
  {
    std::cout << ((backwards >> i) & 1);
    if (i % 8 == 7) std::cout << " ";
  }

  std::cout << '\n';
}
template void printBits<uint32_t>(uint32_t);
template void printBits<uint64_t>(uint64_t);


template <typename T>
void readBigEndian(std::ifstream& file, T& data)
{
  char input[(int)sizeof(T)];
  file.read(input, sizeof(T));

  for (int i = 0; i < (int)sizeof(T); i++)
  {
    data = data << 8;
    data |= (T)input[i];
  }
}
template void readBigEndian<uint32_t>(std::ifstream&, uint32_t&);
template void readBigEndian<uint64_t>(std::ifstream&, uint64_t&);

#endif
