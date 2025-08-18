#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <algorithm>
#include "Logger.h"

using json = nlohmann::json;

class LanguageManager {
public:
    LanguageManager(const std::string _default_language, const std::string& jsonFilePath = "/home/x_user/my_camera_project/langs.json"): default_language(_default_language) {
        std::ifstream file(jsonFilePath);
        if (!file.is_open()) {
            LOG_ERROR("Could not open language file: " + jsonFilePath);
            throw std::runtime_error("Could not open language file: " + jsonFilePath);
        }
        
        try {
            langs = json::parse(file);
            
            // Parse language mappings
            for (const auto& lang_obj : langs["languagesList"]) {
                for (auto& [display_name, lang_code] : lang_obj.items()) {
                    language_keys[display_name] = lang_code;
                }
            }
            
            setLanguage(default_language);
            LOG_INFO("LanguageManager Constructor");
        } catch (const json::exception& e) {
            LOG_ERROR("JSON parsing error: " + std::string(e.what()));
            throw std::runtime_error("JSON parsing error");
        }
    }

    void setLanguage(const std::string& lang) {
        if (language_keys.find(lang) == language_keys.end()) {
            LOG_ERROR("Language not supported: " + lang);
            throw std::invalid_argument("Language not supported: " + lang);
        }
        translations = langs[language_keys[lang]];
        default_language = lang;
        LOG_INFO("Language set to: " + lang);
    }

    std::string getDefaultLanguage() const {
        return default_language;
    }

    std::string getText(const std::string& section, const std::string& key) {
        try {
            return translations[section][key].get<std::string>();
        } catch (const json::exception& e) {
            LOG_ERROR("Failed to get text for section: " + section + ", key: " + key + ", error: " + std::string(e.what()));
            return key; // Fallback to key if translation is missing
        }
    }

     std::string getVosk() {
        try {
            return translations["vosk_model"];
        } catch (const json::exception& e) {
            LOG_ERROR("Failed to get VOSK, error: " + std::string(e.what()));
            return "";
        }

     }

    std::string getGrammar() {
        try {
            const json& grammar = translations["grammar"];
            if (!grammar.is_array()) {
                LOG_ERROR("Grammar section is not an array");
                return "[]";
            }
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < grammar.size(); ++i) {
                if (i > 0) oss << ",";
                std::string word = grammar[i].get<std::string>();
                oss << "\"" << word << "\"";
            }
            oss << "]";
            return oss.str();
        } catch (const json::exception& e) {
            LOG_ERROR("Failed to get grammar: " + std::string(e.what()));
            return "[]";
        }
    }

    std::string getSection(const std::string& section) {
        try {
            return translations[section].dump();
        } catch (const json::exception& e) {
            LOG_ERROR("Failed to get section: " + section + ", error: " + std::string(e.what()));
            return "{}";
        }
    }

private:
    std::string default_language;
    json langs;
    json translations;
    std::map<std::string, std::string> language_keys;    
};
#endif // LANGUAGEMANAGER_H