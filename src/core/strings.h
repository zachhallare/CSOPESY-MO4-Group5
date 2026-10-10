// Small string helpers shared across the project.
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

inline std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

inline std::string Trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

inline bool EqualsNoCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() && ToLower(a) == ToLower(b);
}

inline bool ContainsNoCase(const std::string& haystack, const std::string& needle) {
    return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

inline bool StartsWithNoCase(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && EqualsNoCase(text.substr(0, prefix.size()), prefix);
}

// Lowercase extension including the dot (".txt"), or "" when there is none.
inline std::string ExtensionOf(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return "";
    return ToLower(name.substr(dot));
}
