#include "pch.h"
#include "FavoritesStore.h"
#include "EditState.h"   // GetConfigPath

FavoritesStore g_favorites;

namespace {
    constexpr const wchar_t* kSection = L"Favorites";
    constexpr int kMaxEntries = 4096;

    std::wstring MakeKey(int index) {
        wchar_t buf[24];
        swprintf_s(buf, L"Fav%d", index);
        return buf;
    }
}

std::wstring FavoritesStore::Normalize(const std::wstring& path) {
    std::wstring n = path;
    std::transform(n.begin(), n.end(), n.begin(), ::towlower);
    return n;
}

void FavoritesStore::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paths.clear();
    m_ids.clear();

    const std::wstring ini = GetConfigPath(false);
    if (ini.empty()) return;

    wchar_t buf[32768];
    for (int i = 0; i < kMaxEntries; ++i) {
        const std::wstring key = MakeKey(i);
        DWORD len = GetPrivateProfileStringW(kSection, key.c_str(), L"",
                                              buf, static_cast<DWORD>(std::size(buf)), ini.c_str());
        if (len == 0) break;
        std::wstring path(buf, len);
        if (path.empty()) continue;
        m_paths.push_back(path);
        m_ids.insert(FileNavigator::PathToImageID(Normalize(path)));
    }
}

int FavoritesStore::Count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_paths.size());
}

bool FavoritesStore::Contains(const std::wstring& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ids.count(FileNavigator::PathToImageID(Normalize(path))) != 0;
}

bool FavoritesStore::ContainsId(ImageID id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ids.count(id) != 0;
}

bool FavoritesStore::Toggle(const std::wstring& path) {
    if (path.empty()) return false;
    bool present;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ImageID id = FileNavigator::PathToImageID(Normalize(path));
        present = m_ids.count(id) != 0;
    }
    // Add/Remove acquire the mutex themselves; do not call them while locked.
    if (present) {
        Remove(path);
        return false;
    }
    Add(path);
    return true;
}

void FavoritesStore::Add(const std::wstring& path) {
    if (path.empty()) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ImageID id = FileNavigator::PathToImageID(Normalize(path));
        if (m_ids.count(id) != 0) return;   // already present
        m_paths.push_back(path);
        m_ids.insert(id);
    }
    Persist();
}

void FavoritesStore::Remove(const std::wstring& path) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ImageID id = FileNavigator::PathToImageID(Normalize(path));
        if (m_ids.count(id) == 0) return;
        m_ids.erase(id);
        const std::wstring norm = Normalize(path);
        m_paths.erase(std::remove_if(m_paths.begin(), m_paths.end(),
            [&](const std::wstring& p) { return Normalize(p) == norm; }),
            m_paths.end());
    }
    Persist();
}

void FavoritesStore::Persist() {
    const std::wstring ini = GetConfigPath(false);
    if (ini.empty()) return;

    // Build the [Favorites] section body: "Fav0=path\0Fav1=path\0" plus a
    // trailing double null. WritePrivateProfileSectionW replaces the whole
    // section, which also drops entries removed since last save.
    std::wstring section;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_paths.size(); ++i) {
            section += MakeKey(static_cast<int>(i));
            section += L"=";
            section += m_paths[i];
            section.push_back(L'\0');
        }
    }
    section.push_back(L'\0');
    if (section.size() == 1) section.push_back(L'\0'); // empty section needs a double null
    WritePrivateProfileSectionW(kSection, section.data(), ini.c_str());
}
