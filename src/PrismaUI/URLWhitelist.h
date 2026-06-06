#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace PrismaUI::URLWhitelist {

/// Trusted domains that can load resources (images, styles, fonts, etc.)
/// Add entries here to whitelist external CDNs, wikis, or asset servers
/// Subdomain matching supported: "example.com" allows "api.example.com", "cdn.example.com", etc.
static constexpr std::array<std::string_view, 6> WHITELISTED_DOMAINS = {
    "static.wikia.nocookie.net",    // Fallout wiki images
    "cdn.jsdelivr.net",              // JavaScript CDN (Tailwind, React, etc.)
    "fonts.googleapis.com",           // Google Fonts
    "youtube.com",                   // YouTube videos (youtube.com, www.youtube.com, youtu.be)
    "youtu.be",                      // YouTube short links
    "nexusmods.com",                 // Nexus Mods (any game: fallout4.nexusmods.com, skyrim.nexusmods.com, etc.)
};

/// Check if a domain is whitelisted
/// @param domain The domain to check (e.g., "example.com")
/// @return true if domain is in whitelist, false otherwise
inline bool IsWhitelisted(std::string_view domain) {
    return std::any_of(
        WHITELISTED_DOMAINS.begin(),
        WHITELISTED_DOMAINS.end(),
        [domain](std::string_view whitelisted) {
            // Exact match or subdomain match
            return domain == whitelisted ||
                   (domain.size() > whitelisted.size() &&
                    domain.substr(domain.size() - whitelisted.size()) == whitelisted &&
                    domain[domain.size() - whitelisted.size() - 1] == '.');
        }
    );
}

/// Generate CSP img-src directive with whitelisted domains
/// @return CSP directive string: "img-src 'self' data: blob: file: https://domain1 https://domain2 ..."
inline std::string GenerateImgSrcDirective() {
    std::string directive = "img-src 'self' data: blob: file:";
    for (auto domain : WHITELISTED_DOMAINS) {
        directive += " https://";
        directive += domain;
    }
    directive += ";";
    return directive;
}

/// Generate CSP script-src directive with whitelisted domains
inline std::string GenerateScriptSrcDirective() {
    std::string directive = "script-src 'self' 'unsafe-inline' 'unsafe-eval' file: data:";
    for (auto domain : WHITELISTED_DOMAINS) {
        directive += " https://";
        directive += domain;
    }
    directive += ";";
    return directive;
}

/// Generate CSP style-src directive with whitelisted domains
inline std::string GenerateStyleSrcDirective() {
    std::string directive = "style-src 'self' 'unsafe-inline' file: data:";
    for (auto domain : WHITELISTED_DOMAINS) {
        directive += " https://";
        directive += domain;
    }
    directive += ";";
    return directive;
}

} // namespace PrismaUI::URLWhitelist
