#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace soulbound::detail {

inline std::string readJsonFile(const std::filesystem::path& path, std::string_view label) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open " + std::string(label) + " file: " + path.string());
    const std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad()) throw std::runtime_error("Cannot read " + std::string(label) + " file: " + path.string());
    return text;
}

inline nlohmann::json parseJson(std::string_view text) {
    // nlohmann normally keeps the last duplicate key. Preserve the core's
    // strict schema contract by detecting duplicates before building the DOM.
    std::vector<std::unordered_set<std::string>> objectKeys;
    auto callback = [&](int, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
        using Event = nlohmann::json::parse_event_t;
        if (event == Event::object_start) objectKeys.emplace_back();
        else if (event == Event::object_end) objectKeys.pop_back();
        else if (event == Event::key && !objectKeys.back().insert(parsed.get<std::string>()).second)
            throw std::runtime_error("Duplicate JSON field: " + parsed.get<std::string>());
        return true;
    };
    try {
        return nlohmann::json::parse(text.begin(), text.end(), callback);
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error(std::string("Invalid JSON: ") + error.what());
    }
}

inline void requireFields(const nlohmann::json& object,
                          std::initializer_list<std::string_view> fields,
                          std::string_view label) {
    if (!object.is_object()) throw std::runtime_error("Expected " + std::string(label) + " object");
    for (const auto& [key, value] : object.items()) {
        bool known = false;
        for (const auto field : fields) if (key == field) { known = true; break; }
        if (!known) throw std::runtime_error("unknown " + std::string(label) + " field: " + key);
    }
    for (const auto field : fields) {
        if (!object.contains(std::string(field)))
            throw std::runtime_error("Missing " + std::string(label) + " field: " + std::string(field));
    }
}

template<typename Integer>
Integer jsonInteger(const nlohmann::json& value) {
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (std::in_range<Integer>(number)) return static_cast<Integer>(number);
    } else if (value.is_number_integer()) {
        const auto number = value.get<std::int64_t>();
        if (std::in_range<Integer>(number)) return static_cast<Integer>(number);
    }
    throw std::runtime_error("Expected integer within the supported range");
}

} // namespace soulbound::detail
