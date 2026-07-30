#include <engine/shared/content_collection.h>
#include <base/system.h>
#include <assert.h>

static CContentManifest Make(const char *pID, const char *pVersion, const char *pHash)
{
	CContentManifest M;
	mem_zero(&M, sizeof(M));
	str_copy(M.m_aPublishedFileID, pID, sizeof(M.m_aPublishedFileID));
	str_copy(M.m_aVersion, pVersion, sizeof(M.m_aVersion));
	str_copy(M.m_aContentHash, pHash, sizeof(M.m_aContentHash));
	return M;
}

int main()
{
	const char *pHashA = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	const char *pHashB = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
	CContentManifest A = Make("1", "1", pHashA), B = Make("2", "2", pHashB);
	A.m_DependencyCount = 1;
	str_copy(A.m_aDependencies[0].m_aPublishedFileID, "2", 32);
	str_copy(A.m_aDependencies[0].m_aVersion, "2", 32);
	str_copy(A.m_aDependencies[0].m_aContentHash, pHashB, 65);
	CContentCollection Collection;
	char aError[128];
	assert(Collection.AddManifest(A, "a", aError, sizeof(aError)));
	assert(Collection.AddManifest(B, "b", aError, sizeof(aError)));
	const char *apRoots[] = {"1"};
	int aOrder[64], Count = 0;
	char aHash[65];
	assert(Collection.Resolve(apRoots, 1, aOrder, &Count, aHash, aError, sizeof(aError)));
	assert(Count == 2 && aOrder[0] == 1 && aOrder[1] == 0);
	B.m_DependencyCount = 1;
	str_copy(B.m_aDependencies[0].m_aPublishedFileID, "1", 32);
	str_copy(B.m_aDependencies[0].m_aVersion, "1", 32);
	str_copy(B.m_aDependencies[0].m_aContentHash, pHashA, 65);
	Collection.Clear();
	assert(Collection.AddManifest(A, "a", aError, sizeof(aError)));
	assert(Collection.AddManifest(B, "b", aError, sizeof(aError)));
	assert(!Collection.Resolve(apRoots, 1, aOrder, &Count, aHash, aError, sizeof(aError)));
	return 0;
}
