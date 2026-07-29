#include <engine/shared/content_publish.h>
#include <engine/shared/content_package.h>
#include <base/system.h>
#include <assert.h>

int main(){char aRoot[256],aSource[300],aPackage[300],aInvalidPackage[300],aError[256];str_format(aRoot,sizeof(aRoot),"/tmp/ninslash-publish-%lld",(long long)time_get());assert(fs_makedir(aRoot)==0);str_format(aSource,sizeof(aSource),"%s/source.map",aRoot);IOHANDLE File=io_open(aSource,IOFLAG_WRITE);assert(File);io_write(File,"map",3);io_close(File);str_format(aInvalidPackage,sizeof(aInvalidPackage),"%s/invalid-package",aRoot);assert(!ContentPublishPrepareMap(aSource,aInvalidPackage,"42","","Description","Author","1","test","everyone",aError,sizeof(aError)));assert(str_comp(aError,"invalid map publication fields")==0);str_format(aPackage,sizeof(aPackage),"%s/package",aRoot);assert(ContentPublishPrepareMap(aSource,aPackage,"42","Map","Description","Author","1","test","everyone",aError,sizeof(aError)));CContentManifest Manifest;assert(ContentPackageValidate(aPackage,"42","test",&Manifest,aError,sizeof(aError)));assert(Manifest.m_ContentType==CONTENT_TYPE_MAP);return 0;}
