#pragma once
#include "pch.h"
#include "FileNavigator.h"   // ImageID

// ============================================================================
// FavoritesStore - user bookmarked ("starred") images
// ----------------------------------------------------------------------------
// Favorites are stored as an ordered path list in the same QuickView.ini
// that holds the rest of the application settings ([Favorites] section), so
// the feature follows both portable-mode and AppData persistence rules.
// All public methods are safe to call from the UI thread; the internal list
// is guarded by a mutex for future background-reader use.
// ============================================================================
class FavoritesStore {
public:
    FavoritesStore() = default;
    ~FavoritesStore() = default;
    FavoritesStore(const FavoritesStore&) = delete;
    FavoritesStore& operator=(const FavoritesStore&) = delete;

    // Load the persisted favorites list from the INI (called once at startup,
    // after LoadConfig). Safe to call again to re-read the file.
    void Initialize();

    // Cache-only lookup.
    bool Contains(const std::wstring& path) const;
    bool ContainsId(ImageID id) const;

    // Toggle membership. Returns true if the image is favorited after the call.
    bool Toggle(const std::wstring& path);

    void Add(const std::wstring& path);
    void Remove(const std::wstring& path);

    // Ordered snapshot of favorited paths (normalized on disk form).
    const std::vector<std::wstring>& List() const { return m_paths; }
    int Count() const;

    // Normalize a path the same way the ImageID hash does (case-insensitive on
    // Windows) so lookups and dedup are consistent.
    static std::wstring Normalize(const std::wstring& path);

private:
    void Persist();

    mutable std::mutex m_mutex;
    std::vector<std::wstring> m_paths;        // insertion order, normalized
    std::unordered_set<ImageID> m_ids;
};

// Global instance (defined in FavoritesStore.cpp, like g_ratingStore).
extern FavoritesStore g_favorites;
