#include "unique_name.h"

#include <cassert>

std::unordered_map<std::string, uint32_t> UniqueName::m_name_id_map = {};
std::vector<std::string> UniqueName::m_names = {};

UniqueName::UniqueName() : m_id(UniqueName::INVALID_ID) {}

UniqueName::UniqueName(const std::string& string) : m_id(UniqueName::INVALID_ID) {
	m_id = MakeID(string);
}

UniqueName::UniqueName(const char* c_str) : m_id(UniqueName::INVALID_ID) {
	m_id = MakeID(c_str);
}

UniqueName::UniqueName(ID id) : m_id(id) {}

UniqueName::~UniqueName() {}

UniqueName::UniqueName(const UniqueName& other) : m_id(other.m_id) {}

UniqueName::UniqueName(UniqueName&& other) noexcept : m_id(other.m_id) {}

UniqueName& UniqueName::operator=(const UniqueName& other) {
	m_id = other.m_id;
	return *this;
}

UniqueName& UniqueName::operator=(UniqueName&& other) {
	m_id = other.m_id;
	return *this;
}

bool UniqueName::operator==(const UniqueName& other) const {
	return m_id == other.m_id;
}

bool UniqueName::operator<(const UniqueName& other) const {
	return m_id < other.m_id;
}

const std::string& UniqueName::ToString() const {
	assert(m_id != UniqueName::INVALID_ID);
	return m_names[m_id];
}

UniqueName::ID UniqueName::ToId() const {
	return m_id;
}

bool UniqueName::IsValid() const {
	return m_id != UniqueName::INVALID_ID;
}

UniqueName::ID UniqueName::MakeID(const std::string& string) {
	auto found = m_name_id_map.find(string);
	if (found != m_name_id_map.end()) {
		return found->second;
	}

	m_names.push_back(string);
	ID id = m_names.size() - 1;
	m_name_id_map.insert({ string, id });

	return (uint32_t)id;
}
