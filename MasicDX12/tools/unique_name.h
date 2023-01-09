#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

class UniqueName {
public:
    using ID = uint32_t;
    static const ID INVALID_ID = UINT32_MAX;

    UniqueName();
    UniqueName(const std::string& string);
    UniqueName(const char* c_str);
    explicit UniqueName(ID id);
    ~UniqueName();

    UniqueName(const UniqueName& other);
    UniqueName(UniqueName&& other) noexcept;

    UniqueName& operator=(const UniqueName& other);
    UniqueName& operator=(UniqueName&& other);

    bool operator==(const UniqueName& other) const;
    bool operator<(const UniqueName& other) const;

    const std::string& ToString() const;
    ID ToId() const;

    bool IsValid() const;

    static ID MakeID(const std::string& string);

private:
    static std::unordered_map<std::string, uint32_t> m_name_id_map;
    static std::vector<std::string> m_names;
    ID m_id;
};

namespace std {
    template<>
    struct hash<UniqueName> {
        size_t operator()(const UniqueName& key) const {
            return key.ToId();
        }
    };
}