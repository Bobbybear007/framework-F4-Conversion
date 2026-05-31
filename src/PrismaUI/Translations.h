#pragma once

#include <string>
#include <unordered_map>

namespace PrismaUI::Translations {
    // Language codes used in Fallout 4 translation filenames
    // e.g. Data\Interface\Translations\MyPlugin_en.txt
    constexpr const char* FALLBACK_LANG = "en";

    // Detect the game language from Fallout4Prefs.ini (sLanguage under [General]).
    // Returns lowercase language code such as "en", "de", "fr", etc.
    // Falls back to "en" if the INI cannot be found or parsed.
    std::string DetectGameLanguage();

    // Parse a UTF-16 LE translation file for the given plugin and language.
    // pluginName should be the bare plugin name without extension (e.g. "MyPlugin").
    // lang should be the lowercase language code (e.g. "en").
    // Returns a map of $KEY -> translated string, or an empty map on failure.
    std::unordered_map<std::string, std::string> ParseTranslationFile(const std::string& pluginName,
                                                                       const std::string& lang);

    // Build a JavaScript snippet that assigns window.L10N for the given translation map.
    // The snippet looks like:
    //   window.L10N = { "$KEY": "Value", ... };
    //   window.t = function(k) { return window.L10N[k] || k; };
    // Returns an empty string if the map is empty.
    std::string BuildL10NScript(const std::unordered_map<std::string, std::string>& translations);
}
