#include <engine/shared/sha256.h>
#include <base/system.h>
#include <assert.h>

int main()
{
	CSha256 Hash;
	Hash.Update("abc", 3);
	unsigned char aDigest[32];
	char aHex[65];
	Hash.Finish(aDigest);
	CSha256::ToHex(aDigest, aHex);
	assert(str_comp(aHex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
	return 0;
}
