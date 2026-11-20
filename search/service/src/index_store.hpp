#ifndef WEBSEARCH_SEARCH_SERVICE_SRC_INDEX_STORE_HPP_
#define WEBSEARCH_SEARCH_SERVICE_SRC_INDEX_STORE_HPP_

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "boolean_search/query.hpp"
#include "index.hpp"
#include "zipf_analysis.hpp"

namespace search_web {

class IndexSnapshot {
   public:
    IndexSnapshot(indexing::Index index, std::string name, std::size_t bytes, double load_ms, std::string sha256);
    IndexSnapshot(const IndexSnapshot&) = delete;
    IndexSnapshot& operator=(const IndexSnapshot&) = delete;
    IndexSnapshot(IndexSnapshot&&) = delete;
    IndexSnapshot& operator=(IndexSnapshot&&) = delete;

    indexing::DocList Search(const std::string& query, boolsearch::MatchMode mode = boolsearch::MatchMode::kStem) const;
    const indexing::Index& index() const { return index_; }
    const std::string& name() const { return name_; }
    std::size_t bytes() const { return bytes_; }
    double load_ms() const { return load_ms_; }
    const std::string& sha256() const { return sha256_; }
    const boolsearch::BooleanSearch& engine() const { return *engine_; }
    const analysis::ZipfAnalysis::Report& zipf() const { return zipf_; }

   private:
    indexing::Index index_;
    std::string name_;
    std::size_t bytes_;
    double load_ms_;
    std::string sha256_;
    std::unique_ptr<boolsearch::BooleanSearch> engine_;
    analysis::ZipfAnalysis::Report zipf_;
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
