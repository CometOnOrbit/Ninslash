#ifndef ENGINE_SHARED_SHA256_H
#define ENGINE_SHARED_SHA256_H

class CSha256
{
	unsigned int m_aState[8];
	unsigned long long m_TotalBytes;
	unsigned char m_aBuffer[64];
	int m_BufferSize;
	void Transform(const unsigned char *pBlock);

  public:
	CSha256();
	void Update(const void *pData, unsigned Size);
	void Finish(unsigned char aDigest[32]);
	static void ToHex(const unsigned char aDigest[32], char aHex[65]);
};

#endif
