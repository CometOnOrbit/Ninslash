#ifndef ENGINE_SHARED_CONTENT_PUBLISH_H
#define ENGINE_SHARED_CONTENT_PUBLISH_H

// Builds a validated map package ready for UpdateWorkshopItem. The caller
// owns preview generation and Steam's CreateItem/SubmitItemUpdate lifecycle.
bool ContentPublishPrepareMap(const char *pSourceMap,
							  const char *pPackageRoot,
							  const char *pPublishedFileID,
							  const char *pName,
							  const char *pDescription,
							  const char *pAuthor,
							  const char *pVersion,
							  const char *pProtocol,
							  const char *pRating,
							  char *pError,
							  int ErrorSize);

#endif
