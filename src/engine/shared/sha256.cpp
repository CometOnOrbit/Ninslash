#include "sha256.h"

#include <base/system.h>

namespace
{
unsigned int RotateRight(unsigned int X, int N)
{
	return (X >> N) | (X << (32 - N));
}
const unsigned int s_aK[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
} // namespace

CSha256::CSha256() : m_TotalBytes(0), m_BufferSize(0)
{
	const unsigned int aInitial[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	mem_copy(m_aState, aInitial, sizeof(m_aState));
}

void CSha256::Transform(const unsigned char *pBlock)
{
	unsigned int W[64];
	for(int i = 0; i < 16; i++)
		W[i] = (unsigned int)pBlock[i * 4] << 24 | (unsigned int)pBlock[i * 4 + 1] << 16 |
			   (unsigned int)pBlock[i * 4 + 2] << 8 | pBlock[i * 4 + 3];
	for(int i = 16; i < 64; i++)
	{
		const unsigned int S0 = RotateRight(W[i - 15], 7) ^ RotateRight(W[i - 15], 18) ^ (W[i - 15] >> 3);
		const unsigned int S1 = RotateRight(W[i - 2], 17) ^ RotateRight(W[i - 2], 19) ^ (W[i - 2] >> 10);
		W[i] = W[i - 16] + S0 + W[i - 7] + S1;
	}
	unsigned int A = m_aState[0], B = m_aState[1], C = m_aState[2], D = m_aState[3], E = m_aState[4], F = m_aState[5],
				 G = m_aState[6], H = m_aState[7];
	for(int i = 0; i < 64; i++)
	{
		const unsigned int S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25), Ch = (E & F) ^ ((~E) & G);
		const unsigned int T1 = H + S1 + Ch + s_aK[i] + W[i],
						   S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22),
						   Maj = (A & B) ^ (A & C) ^ (B & C);
		H = G;
		G = F;
		F = E;
		E = D + T1;
		D = C;
		C = B;
		B = A;
		A = T1 + S0 + Maj;
	}
	m_aState[0] += A;
	m_aState[1] += B;
	m_aState[2] += C;
	m_aState[3] += D;
	m_aState[4] += E;
	m_aState[5] += F;
	m_aState[6] += G;
	m_aState[7] += H;
}

void CSha256::Update(const void *pData, unsigned Size)
{
	const unsigned char *pBytes = static_cast<const unsigned char *>(pData);
	m_TotalBytes += Size;
	while(Size)
	{
		const int Space = 64 - m_BufferSize;
		const int Take = Size < (unsigned)Space ? (int)Size : Space;
		mem_copy(m_aBuffer + m_BufferSize, pBytes, Take);
		m_BufferSize += Take;
		pBytes += Take;
		Size -= Take;
		if(m_BufferSize == 64)
		{
			Transform(m_aBuffer);
			m_BufferSize = 0;
		}
	}
}

void CSha256::Finish(unsigned char aDigest[32])
{
	const unsigned long long Bits = m_TotalBytes * 8;
	const unsigned char One = 0x80, Zero = 0;
	Update(&One, 1);
	while(m_BufferSize != 56)
		Update(&Zero, 1);
	unsigned char aLength[8];
	for(int i = 0; i < 8; i++)
		aLength[7 - i] = (unsigned char)(Bits >> (i * 8));
	Update(aLength, sizeof(aLength));
	for(int i = 0; i < 8; i++)
		for(int j = 0; j < 4; j++)
			aDigest[i * 4 + j] = (unsigned char)(m_aState[i] >> (24 - j * 8));
}

void CSha256::ToHex(const unsigned char aDigest[32], char aHex[65])
{
	const char *pDigits = "0123456789abcdef";
	for(int i = 0; i < 32; i++)
	{
		aHex[i * 2] = pDigits[aDigest[i] >> 4];
		aHex[i * 2 + 1] = pDigits[aDigest[i] & 15];
	}
	aHex[64] = 0;
}
