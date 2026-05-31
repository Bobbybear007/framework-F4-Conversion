#include "Translations.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>

namespace PrismaUI::Translations {

    std::string DetectGameLanguage() {
        logger::debug("Translations::DetectGameLanguage: scanning INI files");
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        // Walk up from the exe to find the game root, then append the INI path
        std::string path(exePath);
        auto slashPos = path.rfind('\\');
        if (slashPos == std::string::npos) {
            return FALLBACK_LANG;
        }
        std::string gameDir = path.substr(0, slashPos);

        // Priority: Custom.ini (user override) > Prefs.ini > Fallout4.ini (game default).
        // Return on first hit so higher-priority files win.
        std::vector<std::string> candidates;

        char appData[MAX_PATH] = {};
        if (GetEnvironmentVariableA("USERPROFILE", appData, MAX_PATH)) {
            std::string myGames = std::string(appData) + "\\Documents\\My Games\\Fallout4\\";
            candidates.push_back(myGames + "Fallout4Custom.ini");
            candidates.push_back(myGames + "Fallout4Prefs.ini");
        }
        candidates.push_back(gameDir + "\\Fallout4.ini");

        for (const auto& iniPath : candidates) {
            char langBuf[64] = {};
            DWORD result = GetPrivateProfileStringA("General", "sLanguage", "", langBuf, sizeof(langBuf),
                                                    iniPath.c_str());
            if (result > 0) {
                std::string lang(langBuf);
                for (auto& c : lang) {
                    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                }
                if (!lang.empty()) {
                    logger::info("Translations::DetectGameLanguage: '{}' from {}", lang, iniPath);
                    return lang;
                }
            } else {
                logger::debug("Translations::DetectGameLanguage: no sLanguage in {}", iniPath);
            }
        }

        logger::warn("Translations::DetectGameLanguage: no language found in any INI, falling back to '{}'",
                     FALLBACK_LANG);
        return FALLBACK_LANG;
    }

    std::unordered_map<std::string, std::string> ParseTranslationFile(const std::string& pluginName,
                                                                       const std::string& lang) {
        std::unordered_map<std::string, std::string> result;

        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path(exePath);
        auto slashPos = path.rfind('\\');
        if (slashPos == std::string::npos) {
            logger::error("Translations::ParseTranslationFile: cannot determine game directory from '{}'", exePath);
            return result;
        }
        std::string dataDir = path.substr(0, slashPos) + "\\Data";
        std::string filePath = dataDir + "\\Interface\\Translations\\" + pluginName + "_" + lang + ".txt";
        logger::debug("Translations::ParseTranslationFile: trying '{}'", filePath);

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            if (lang != FALLBACK_LANG) {
                std::string fallbackPath =
                    dataDir + "\\Interface\\Translations\\" + pluginName + "_" + FALLBACK_LANG + ".txt";
                logger::warn("Translations::ParseTranslationFile: '{}' not found, trying fallback '{}'",
                             filePath, fallbackPath);
                file.open(fallbackPath, std::ios::binary);
                if (!file.is_open()) {
                    logger::warn("Translations::ParseTranslationFile: fallback '{}' not found either, no translations loaded",
                                 fallbackPath);
                    return result;
                }
                logger::info("Translations::ParseTranslationFile: using fallback '{}'", fallbackPath);
            } else {
                logger::warn("Translations::ParseTranslationFile: '{}' not found, no translations loaded", filePath);
                return result;
            }
        } else {
            logger::info("Translations::ParseTranslationFile: opened '{}'", filePath);
        }

        // Read entire file into memory
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size < 2) {
            return result;
        }

        std::vector<char> raw(static_cast<size_t>(size));
        file.read(raw.data(), size);
        file.close();

        // Detect and strip UTF-16 LE BOM (FF FE)
        const wchar_t* wStart = nullptr;
        size_t wLen = 0;

        if (static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
            // UTF-16 LE with BOM
            wStart = reinterpret_cast<const wchar_t*>(raw.data() + 2);
            wLen = (static_cast<size_t>(size) - 2) / sizeof(wchar_t);
        } else {
            // Try to treat as UTF-8 plain text (no BOM)
            // Parse line-by-line directly
            std::istringstream ss(std::string(raw.data(), static_cast<size_t>(size)));
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty() || line[0] != '$') {
                    continue;
                }
                // Strip trailing \r
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                auto tabPos = line.find('\t');
                if (tabPos == std::string::npos) {
                    continue;
                }
                std::string key = line.substr(0, tabPos);
                std::string value = line.substr(tabPos + 1);
                result[key] = value;
            }
            logger::info("Translations::ParseTranslationFile: loaded {} entries (UTF-8) for '{}'",
                         result.size(), pluginName);
            return result;
        }

        // Convert UTF-16 LE wide string to std::wstring, then each line to UTF-8
        std::wstring wide(wStart, wLen);
        std::wistringstream wss(wide);
        std::wstring wline;

        while (std::getline(wss, wline)) {
            if (wline.empty() || wline[0] != L'$') {
                continue;
            }
            // Strip trailing \r
            if (!wline.empty() && wline.back() == L'\r') {
                wline.pop_back();
            }
            auto tabPos = wline.find(L'\t');
            if (tabPos == std::wstring::npos) {
                continue;
            }
            std::wstring wkey = wline.substr(0, tabPos);
            std::wstring wval = wline.substr(tabPos + 1);

            // Convert key and value to UTF-8
            auto toUtf8 = [](const std::wstring& ws) -> std::string {
                if (ws.empty()) return "";
                int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr,
                                             nullptr);
                if (len <= 0) return "";
                std::string out(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), len, nullptr,
                                    nullptr);
                return out;
            };

            std::string key = toUtf8(wkey);
            std::string val = toUtf8(wval);
            if (!key.empty()) {
                result[key] = val;
            }
        }

        logger::info("Translations::ParseTranslationFile: loaded {} entries (UTF-16 LE) for '{}'",
                     result.size(), pluginName);
        return result;
    }

    // Escape a UTF-8 string for embedding inside a JS double-quoted string literal
    static std::string jsEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (unsigned char c : s) {
            if (c == '\\') {
                out += "\\\\";
            } else if (c == '"') {
                out += "\\\"";
            } else if (c == '\n') {
                out += "\\n";
            } else if (c == '\r') {
                out += "\\r";
            } else {
                out += static_cast<char>(c);
            }
        }
        return out;
    }

    std::string BuildL10NScript(const std::unordered_map<std::string, std::string>& translations) {
        if (translations.empty()) {
            return "";
        }

        std::string script;
        script.reserve(translations.size() * 64);
        script += "window.L10N={";

        bool first = true;
        for (const auto& kv : translations) {
            if (!first) {
                script += ',';
            }
            first = false;
            script += '"';
            script += jsEscape(kv.first);
            script += "\":\"";
            script += jsEscape(kv.second);
            script += '"';
        }

        script += "};window.t=function(k){return window.L10N[k]!==undefined?window.L10N[k]:k;};";
        return script;
    }
}
