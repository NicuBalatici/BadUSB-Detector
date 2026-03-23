#ifndef GEMINI_ANALYZER_H
#define GEMINI_ANALYZER_H

#include <string>

class GeminiAnalyzer {
private:
    std::string api_key;

public:
    GeminiAnalyzer(const std::string& key);
    void generateThreatReport(const std::string& log_filepath);
};

#endif