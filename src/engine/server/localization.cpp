#include "localization.h"
#include <engine/localization.h>
#include <base/system.h>
#include <thread>
#include <atomic>
#include <cstring>

// Helper macro for safe string operations
#define LOCALIZATION_SAFE_STRNCPY(dst, src, size) \
    do { \
        strncpy(dst, src, size); \
        dst[(size)-1] = '\0'; \
    } while(0)

CLocalization::CLocalization(IStorage *pStorage)
    : m_pStorage(pStorage)
    , m_Initialized(false)
    , m_LoadComplete(false)
{
}

// Helper function: safely read file contents with RAII
std::unique_ptr<char[]> CLocalization::ReadFileContents(const char *pFilePath, int &OutFileSize)
{
    IOHANDLE File = m_pStorage->OpenFile(pFilePath, IOFLAG_READ, IStorage::TYPE_ALL);
    if(!File)
    {
        OutFileSize = 0;
        return nullptr;
    }
    
    OutFileSize = (int)io_length(File);
    if(OutFileSize <= 0)
    {
        io_close(File);
        return nullptr;
    }
    
    std::unique_ptr<char[]> pFileData(new char[OutFileSize + 1]);
    io_read(File, pFileData.get(), OutFileSize);
    io_close(File);
    
    // Null-terminate for safety
    pFileData[OutFileSize] = '\0';
    return pFileData;
}

// Thread-safe addition of localization entry
void CLocalization::SafeAddLocalization(const char *pLanguage, const char *pKey, const char *pValue)
{
    std::lock_guard<std::mutex> lock(m_LocalizeMutex);
    AddNewLocalize(pLanguage, pKey, pValue);
}

void CLocalization::Init()
{
    if(m_Initialized)
        return;
        
    m_Initialized = true;
    thread_init(LoadLocalizations, this);
}

void CLocalization::LoadLocalizations(void *pUser)
{
    CLocalization *pThis = static_cast<CLocalization*>(pUser);
    
    const char *pIndex = "./data/server/languages/index.json";
    int FileSize = 0;
    std::unique_ptr<char[]> pFileData = pThis->ReadFileContents(pIndex, FileSize);
    
    if(!pFileData)
    {
        dbg_msg("Localization", "Can't open localization index: %s", pIndex);
        pThis->m_LoadComplete = true;
        return;
    }

    // Parse JSON data
    json_settings JsonSettings = {};
    char aError[256] = {0};
    json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData.get(), FileSize, aError);
    
    if(!pJsonData)
    {
        dbg_msg("Localization", "Can't parse localization index %s: %s", pIndex, aError);
        pThis->m_LoadComplete = true;
        return;
    }

    // Use RAII wrapper for json_value
    struct JsonValueGuard {
        json_value *pValue;
        JsonValueGuard(json_value *p) : pValue(p) {}
        ~JsonValueGuard() { if(pValue) json_value_free(pValue); }
    } guard(pJsonData);

    const json_value &rStart = (*pJsonData)["language indices"];
    int loadedCount = 0;
    int failedCount = 0;

    if(rStart.type == json_array && rStart.u.array.length > 1)
    {
        // Start from i = 1 to skip English (assuming English is first)
        for(unsigned i = 1; i < rStart.u.array.length; ++i)
        {
            const json_value &fileEntry = rStart[i];
            if(fileEntry.type == json_object)
            {
                const char *pLanguageFile = fileEntry["file"];
                if(pLanguageFile)
                {
                    if(pThis->LoadLanguage(pLanguageFile))
                        loadedCount++;
                    else
                        failedCount++;
                }
            }
        }
    }
    else
    {
        dbg_msg("Localization", "Invalid index format in %s", pIndex);
    }

    pThis->m_LoadComplete = true;
    dbg_msg("Localization", "Localization loaded: %d languages loaded, %d failed", loadedCount, failedCount);
}

bool CLocalization::LoadLanguage(const char *pFile)
{
    if(!pFile || !pFile[0])
        return false;
        
    char aFilePath[128];
    str_format(aFilePath, sizeof(aFilePath), "./data/server/languages/%s.json", pFile);
    
    int FileSize = 0;
    std::unique_ptr<char[]> pFileData = ReadFileContents(aFilePath, FileSize);
    
    if(!pFileData)
    {
        dbg_msg("Localization", "Can't open language file: %s", aFilePath);
        return false;
    }
    
    return LoadLanguageFromData(pFile, pFileData.get(), FileSize);
}

bool CLocalization::LoadLanguageFromData(const char *pLanguageCode, const char *pFileData, int FileSize)
{
    // Parse JSON data
    json_settings JsonSettings = {};
    char aError[256] = {0};
    json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
    
    if(!pJsonData)
    {
        dbg_msg("Localization", "Can't parse language file for %s: %s", pLanguageCode, aError);
        return false;
    }
    
    // RAII guard for json_value
    struct JsonValueGuard {
        json_value *pValue;
        JsonValueGuard(json_value *p) : pValue(p) {}
        ~JsonValueGuard() { if(pValue) json_value_free(pValue); }
    } guard(pJsonData);

    const json_value &translations = (*pJsonData)["translation"];
    int translationCount = 0;

    if(translations.type == json_array)
    {
        // Initialize language entry
        {
            std::lock_guard<std::mutex> lock(m_LocalizeMutex);
            LOCALIZATION_SAFE_STRNCPY(m_aLocalize[pLanguageCode].m_aLanguageName, 
                                     pLanguageCode, 
                                     sizeof(m_aLocalize[pLanguageCode].m_aLanguageName));
        }
        
        // Load translations
        for(unsigned i = 0; i < translations.u.array.length; ++i)
        {
            const json_value &entry = translations[i];
            if(entry.type == json_object)
            {
                const char *pKey = entry["key"];
                const char *pValue = entry["value"];
                
                if(pKey && pValue)
                {
                    SafeAddLocalization(pLanguageCode, pKey, pValue);
                    translationCount++;
                }
            }
        }
    }
    else
    {
        dbg_msg("Localization", "Invalid translation format for language %s", pLanguageCode);
        return false;
    }
    
    dbg_msg("Localization", "Loaded %s: %d translations", pLanguageCode, translationCount);
    return true;
}

void CLocalization::AddNewLocalize(const char *pName, const char *pKey, const char *pValue)
{
    if(!pName || !pKey || !pValue)
        return;
        
    // Use emplace for efficiency (avoids extra copy)
    m_aLocalize[pName].m_aLocalizedTexts.emplace(pKey, pValue);
}

const char *CLocalization::GetLanguageCode(int Country)
{
    // Simple cache for frequently used country codes
    static const std::unordered_map<int, const char*> s_CountryCodeCache = {
        {826, "en"}, // United Kingdom
        {840, "en"}, // United States
        {250, "fr"}, // France
        {276, "de"}, // Germany
        {380, "it"}, // Italy
        {392, "ja"}, // Japan
        {156, "zh-cn"}, // China
        {643, "ru"}, // Russia
        {724, "es"}, // Spain
        {76,  "pt"}, // Brazil
    };
    
    // Check cache first
    auto cached = s_CountryCodeCache.find(Country);
    if(cached != s_CountryCodeCache.end())
        return cached->second;
    
    // Constants from 'data/countryflags/index.txt'
    switch(Country)
    {
		/* ar - Arabic ************************************/
		case 12: //Algeria
		case 48: //Bahrain
		case 262: //Djibouti
		case 818: //Egypt
		case 368: //Iraq
		case 400: //Jordan
		case 414: //Kuwait
		case 422: //Lebanon
		case 434: //Libya
		case 478: //Mauritania
		case 504: //Morocco
		case 512: //Oman
		case 275: //Palestine
		case 634: //Qatar
		case 682: //Saudi Arabia
		case 706: //Somalia
		case 729: //Sudan
		case 760: //Syria
		case 788: //Tunisia
		case 784: //United Arab Emirates
		case 887: //Yemen
			return "ar";
		/* bg - Bosnian *************************************/
		case 100: //Bulgaria
			return "bg";
		/* bs - Bosnian *************************************/
		case 70: //Bosnia and Hercegovina
			return "bs";
		/* cs - Czech *************************************/
		case 203: //Czechia
			return "cs";
		/* de - German ************************************/
		case 40: //Austria
		case 276: //Germany
		case 438: //Liechtenstein
		case 756: //Switzerland
			return "de";
		/* el - Greek ***********************************/
		case 300: //Greece
		case 196: //Cyprus
			return "el";
		/* es - Spanish ***********************************/
		case 32: //Argentina
		case 68: //Bolivia
		case 152: //Chile
		case 170: //Colombia
		case 188: //Costa Rica
		case 192: //Cuba
		case 214: //Dominican Republic
		case 218: //Ecuador
		case 222: //El Salvador
		case 226: //Equatorial Guinea
		case 320: //Guatemala
		case 340: //Honduras
		case 484: //Mexico
		case 558: //Nicaragua
		case 591: //Panama
		case 600: //Paraguay
		case 604: //Peru
		case 630: //Puerto Rico
		case 724: //Spain
		case 858: //Uruguay
		case 862: //Venezuela
			return "es";
		/* fa - Farsi ************************************/
		case 364: //Islamic Republic of Iran
		case 4: //Afghanistan
			return "fa";
		/* fr - French ************************************/
		case 204: //Benin
		case 854: //Burkina Faso
		case 178: //Republic of the Congo
		case 384: //Cote d’Ivoire
		case 266: //Gabon
		case 324: //Ginea
		case 466: //Mali
		case 562: //Niger
		case 686: //Senegal
		case 768: //Togo
		case 250: //France
		case 492: //Monaco
			return "fr";
		/* hr - Croatian **********************************/
		case 191: //Croatia
			return "hr";
		/* hu - Hungarian *********************************/
		case 348: //Hungary
			return "hu";
		/* it - Italian ***********************************/
		case 380: //Italy
			return "it";
		/* ja - Japanese **********************************/
		case 392: //Japan
			return "ja";
		/* la - Latin *************************************/
		case 336: //Vatican
			return "la";
		/* nl - Dutch *************************************/
		case 533: //Aruba
		case 531: //Curaçao
		case 534: //Sint Maarten
		case 528: //Netherland
		case 740: //Suriname
		case 56: //Belgique
			return "nl";
		/* pl - Polish *************************************/
		case 616: //Poland
			return "pl";
		/* pt - Portuguese ********************************/
		case 24: //Angola
		case 76: //Brazil
		case 132: //Cape Verde
		//case 226: //Equatorial Guinea: official language, but not national language
		//case 446: //Macao: official language, but spoken by less than 1% of the population
		case 508: //Mozambique
		case 626: //Timor-Leste
		case 678: //São Tomé and Príncipe
			return "pt";
		/* ru - Russian ***********************************/
		case 112: //Belarus
		case 643: //Russia
		case 398: //Kazakhstan
			return "ru";
		/* sk - Slovak ************************************/
		case 703: //Slovakia
			return "sk";
		/* sr - Serbian ************************************/
		case 688: //Serbia
			return "sr";
		/* tl - Tagalog ************************************/
		case 608: //Philippines
			return "tl";
		/* tr - Turkish ************************************/
		case 31: //Azerbaijan
		case 792: //Turkey
			return "tr";
		/* uk - Ukrainian **********************************/
		case 804: //Ukraine
			return "uk";
		/* zh-cn - Chinese (Simplified) **********************************/
		case 156: //People’s Republic of China
		case 344: //Hong Kong
		case 446: //Macau
			return "zh-cn";
		case 826: // United Kingdom of Great Britain and Northern Ireland
		case 840: // United States of America
			return "en";
		default:
			return "en";
	}
}

const char *CLocalization::Localize(const char *pLanguage, const char *pText)
{
    if(!pLanguage || !pText || !pText[0])
        return pText;
    
    // Wait for initialization if needed
    if(!m_LoadComplete)
    {
        // Quick check without waiting if we're still loading
        static int s_WarningCount = 0;
        if(s_WarningCount++ < 3)  // Only warn first few times
            dbg_msg("Localization", "Localization called before initialization complete");
        return pText;
    }
    
    std::lock_guard<std::mutex> lock(m_LocalizeMutex);
    
    // Find language
    auto languageIt = m_aLocalize.find(pLanguage);
    if(languageIt == m_aLocalize.end())
        return pText;
    
    // Find translation
    auto &texts = languageIt->second.m_aLocalizedTexts;
    auto textIt = texts.find(pText);
    
    if(textIt != texts.end() && !textIt->second.empty())
        return textIt->second.c_str();
    
    return pText;
}

bool CLocalization::IsLanguageLoaded(const char *pLanguage) const
{
    if(!pLanguage)
        return false;
        
    std::lock_guard<std::mutex> lock(m_LocalizeMutex);
    return m_aLocalize.find(pLanguage) != m_aLocalize.end();
}

void CLocalization::WaitForInitialization()
{
    // Simple spin wait - in production you might want a condition variable
    while(!m_LoadComplete)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ILocalization *CreateLocalization(IStorage *pStorage) { return new CLocalization(pStorage); }