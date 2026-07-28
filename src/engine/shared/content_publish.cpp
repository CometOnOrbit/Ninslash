#include "content_publish.h"
#include "content_package.h"

#include <base/system.h>

namespace
{
bool Fail(char *pError,int ErrorSize,const char *pText){if(pError&&ErrorSize>0)str_copy(pError,pText,ErrorSize);return false;}
bool SafeText(const char *pText,int Max){if(!pText||str_length(pText)>=Max)return false;for(int i=0;pText[i];i++)if((unsigned char)pText[i]<32||pText[i]=='"'||pText[i]=='\\')return false;return true;}
bool Copy(const char *pSource,const char *pTarget){if(fs_is_symlink(pSource))return false;IOHANDLE Source=io_open(pSource,IOFLAG_READ);if(!Source)return false;IOHANDLE Target=io_open(pTarget,IOFLAG_WRITE);if(!Target){io_close(Source);return false;}unsigned char aBuf[16384];bool Ok=true;for(;;){unsigned Read=io_read(Source,aBuf,sizeof(aBuf));if(!Read)break;if(io_write(Target,aBuf,Read)!=Read){Ok=false;break;}}io_close(Source);io_close(Target);return Ok;}
bool WriteManifest(const char *pPath,const char *pTemplate,const char *pHash){char aJson[4096];str_format(aJson,sizeof(aJson),pTemplate,pHash);IOHANDLE File=io_open(pPath,IOFLAG_WRITE);if(!File)return false;const unsigned Size=str_length(aJson);const bool Ok=io_write(File,aJson,Size)==Size;io_close(File);return Ok;}
}

bool ContentPublishPrepareMap(const char *pSourceMap,const char *pRoot,const char *pID,const char *pName,const char *pDescription,const char *pAuthor,const char *pVersion,const char *pProtocol,const char *pRating,char *pError,int ErrorSize)
{
	if(!pSourceMap||fs_is_symlink(pSourceMap)||!pRoot||!pID||!SafeText(pName,128)||!SafeText(pDescription,1024)||!SafeText(pAuthor,128)||!SafeText(pVersion,32)||!SafeText(pProtocol,128)||!SafeText(pRating,16))return Fail(pError,ErrorSize,"invalid map publication fields");
	if(fs_makedir(pRoot)!=0)return Fail(pError,ErrorSize,"unable to create publication directory");
	char aMaps[1536],aMap[1600],aManifest[1600];str_format(aMaps,sizeof(aMaps),"%s/maps",pRoot);if(fs_makedir(aMaps)!=0)return Fail(pError,ErrorSize,"unable to create map package directory");str_format(aMap,sizeof(aMap),"%s/main.map",aMaps);if(!Copy(pSourceMap,aMap))return Fail(pError,ErrorSize,"unable to copy saved map");str_format(aManifest,sizeof(aManifest),"%s/ninslash_content.json",pRoot);
	char aTemplate[4096];str_format(aTemplate,sizeof(aTemplate),"{\"schema_version\":1,\"content_type\":\"map\",\"published_file_id\":\"%s\",\"name\":\"%s\",\"description\":\"%s\",\"author\":\"%s\",\"version\":\"%s\",\"target_protocol\":\"%s\",\"content_hash\":\"%%s\",\"content_rating\":\"%s\",\"dependencies\":[],\"maps\":[\"maps/main.map\"],\"resources\":[]}",pID,pName,pDescription,pAuthor,pVersion,pProtocol,pRating);
	const char *pZero="0000000000000000000000000000000000000000000000000000000000000000";if(!WriteManifest(aManifest,aTemplate,pZero))return Fail(pError,ErrorSize,"unable to write content manifest");CContentManifest Manifest;char aJson[4096];IOHANDLE File=io_open(aManifest,IOFLAG_READ);if(!File)return false;const unsigned Size=io_read(File,aJson,sizeof(aJson)-1);io_close(File);aJson[Size]=0;if(!ContentManifestParse(aJson,Size,pProtocol,&Manifest,pError,ErrorSize))return false;char aHash[65];if(!ContentPackageComputeHash(pRoot,Manifest,aHash,pError,ErrorSize)||!WriteManifest(aManifest,aTemplate,aHash))return Fail(pError,ErrorSize,"unable to finalize content manifest");return ContentPackageValidate(pRoot,pID,pProtocol,&Manifest,pError,ErrorSize);
}
