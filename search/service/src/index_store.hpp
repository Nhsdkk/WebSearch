#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_INDEX_STORE_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_INDEX_STORE_HPP_

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "index.hpp"

namespace search_web {

class IndexSnapshot {
   public:
    IndexSnapshot(indexing::Index index, std::string name, std::size_t bytes, double load_ms);

    indexing::DocList Search(const std::string& query) const;
    const indexing::Index& index() const { return index_; }
    const std::string& name() const { return name_; }
    std::size_t bytes() const { return bytes_; }
    double load_ms() const { return load_ms_; }

   private:
    indexing::Index index_;
    std::string name_;
    std::size_t bytes_;
    double load_ms_;
};

class IndexStore {
   public:
    explicit IndexStore(const std::string& path);

    std::shared_ptr<const IndexSnapshot> Snapshot() const;
    std::shared_ptr<const IndexSnapshot> LoadUpload(std::string_view bytes, std::string name);

   private:
    static std::shared_ptr<const IndexSnapshot> LoadFile(const std::string& path, std::string name);

    std::shared_ptr<const IndexSnapshot> current_;
    mutable std::mutex snapshot_mutex_;
    std::mutex loading_;
};

}  // namespace search_web

#endif  // WEBSEARCH_SEARCH_SERVICE_SRC_INDEX_STORE_HPP_
