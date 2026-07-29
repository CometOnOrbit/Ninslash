#include <game/client/cloud_profile.h>

#include <assert.h>

int main()
{
	assert(CloudProfileDecide(1, 1, 1, false) == CLOUD_SYNC_CURRENT);
	assert(CloudProfileDecide(1, 2, 1, false) == CLOUD_SYNC_APPLY_REMOTE);
	assert(CloudProfileDecide(2, 1, 1, false) == CLOUD_SYNC_UPLOAD_LOCAL);
	assert(CloudProfileDecide(1, 2, 0, true) == CLOUD_SYNC_APPLY_REMOTE);
	assert(CloudProfileDecide(1, 2, 0, false) == CLOUD_SYNC_CONFLICT);
	assert(CloudProfileDecide(2, 3, 1, false) == CLOUD_SYNC_CONFLICT);
	return 0;
}
