#ifndef ENGINE_SERVER_LOCALIZATION_H
#define ENGINE_SERVER_LOCALIZATION_H

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <engine/storage.h>
#include <engine/external/json-parser/json.h>
#include <engine/localization.h>

struct SLanguageFile
{
    char m_aLanguageName[64];
    std::unordered_map<std::string, std::string> m_aLocalizedTexts;
};

class CLocalization : public ILocalization
{
private:
    IStorage *m_pStorage;
    std::unordered_map<std::string, SLanguageFile> m_aLocalize;
    mutable std::mutex m_LocalizeMutex;  // mutable for const member functions
    bool m_Initialized;
    
    // Thread-safe initialization flag
    std::atomic<bool> m_LoadComplete;

    static void LoadLocalizations(void *pUser);

    bool LoadLanguage(const char *pFile);
    bool LoadLanguageFromData(const char *pLanguageCode, const char *pFileData, int FileSize);
    void AddNewLocalize(const char *pName, const char *pKey, const char *pValue);
    
    // Helper functions
    std::unique_ptr<char[]> ReadFileContents(const char *pFilePath, int &OutFileSize);
    void SafeAddLocalization(const char *pLanguage, const char *pKey, const char *pValue);

public:
    CLocalization(IStorage *pStorage);
    ~CLocalization() = default;
    
    virtual void Init();
    virtual const char *GetLanguageCode(int Country);
    virtual const char *Localize(const char *pLanguage, const char *pText);
    
    // Additional safety checks
    bool IsLanguageLoaded(const char *pLanguage) const;
    void WaitForInitialization();
};

#endif