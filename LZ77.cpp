#include <iostream>
#include <cmath>
using std::cout;

const int LENGTH_CODE_VALUE[29] = {
	3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};

const int DISTANCE_CODE_VALUE[30] = {
	1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};

const int extraBits(int x, int a);

class LZ77
{

	char* input;
	int inputSize;
	int inputPosition = 0;

	char* output;
	int outputSize;
	int outputPosition = 0;

	int window = 0;
	const int WINDOW_SIZE = 1 << 15;
	const int WINDOW_MASK = WINDOW_SIZE - 1;

	LZ77(char* input, int inputSize)
	{
		this->input = input;
		this->inputSize = inputSize;

		outputSize = std::pow(2, 1 + (int)std::log2(inputSize));
		output = new char[outputSize]();
	}
	
    void copyData(int length, int distance);
};

int main()
{
	for (int i = 0; i < 28; i++)
		cout << extraBits(LENGTH_CODE_VALUE[i], 2) << ", ";
	cout << extraBits(LENGTH_CODE_VALUE[28], 2) << "\n";

	for (int i = 0; i < 29; i++)
		cout << extraBits(DISTANCE_CODE_VALUE[i], 1) << ", ";
	cout << extraBits(DISTANCE_CODE_VALUE[29], 1) << "\n";

	return 0;
}

void LZ77::copyData(int length, int distance)
{
	int copyPosition = WINDOW_SIZE + outputPosition - distance;
	copyPosition = copyPosition & WINDOW_MASK;

	for ( int i = 0; i < length; i++)
	{
		//WINDOW[outputPosition] = WINDOW[COPY_POSITION];
		//OutputByte (WINDOW[outputPosition]);

		outputPosition += 1;
		outputPosition &= WINDOW_MASK;

		copyPosition += 1;
		copyPosition &= WINDOW_MASK;
	}
}

const int extraBits(int x, int a)
{
	int result = 0;
	if (x == 258)
		return 0;
	if (x == 24577)
		return 13;

	for (int dif = std::max(x - (int)std::pow(2,a) + 1, 1) / 2; dif > 0; dif /= 2)
		result++;
	
	return std::max(0, result - a);
}
