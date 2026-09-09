#ifndef BITDISPLAY_H
#define BITDISPLAY_H

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

#endif
