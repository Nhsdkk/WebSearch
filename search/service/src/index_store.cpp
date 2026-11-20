#include "index_store.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <mutex>
#include <ratio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/crypto/hash.hpp>
#include <userver/fs/blocking/temp_file.hpp>
#include <utility>

#include "boolean_search/query.hpp"
#include "index_loader.hpp"
#include "tokenizer.hpp"

namespace search_web {

IndexSnapshot::IndexSnapshot(indexing::Index index, std::string name, std::size_t bytes, double load_ms,
                             std::string sha256)
    : index_(std::move(index)), name_(std::move(name)), bytes_(bytes), load_ms_(load_ms), sha256_(std::move(sha256)) {
    const auto start = std::chrono::steady_clock::now();
    engine_ = std::make_unique<boolsearch::BooleanSearch>(index_);
    zipf_ = analysis::ZipfAnalysis{}.Run(index_);
    load_ms_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

indexing::DocList IndexSnapshot::Search(const std::string& query, boolsearch::MatchMode mode) const {
    return engine_->Search(query, mode);
}

IndexStore::IndexStore(const std::string& path)
    : current_(LoadFile(path, std::filesystem::path(path).filename().string())) {}

std::shared_ptr<const IndexSnapshot> IndexStore::Snapshot() const {
    std::lock_guard lock(snapshot_mutex_);
    return current_;
}

std::shared_ptr<const IndexSnapshot> IndexStore::LoadUpload(std::string_view bytes, std::string name) {
    std::lock_guard lock(loading_);
    if (bytes.empty()) {
        throw std::invalid_argument("Choose a non-empty BIDX index file");
    }

    try {
        indexing::Tokenizer{}.Utf8Length(name);
    } catch (const std::runtime_error& error) {
        throw std::invalid_argument(error.what());
    }

    const auto temporary = userver::fs::blocking::TempFile::Create();
    std::ofstream output(temporary.GetPath(), std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Cannot write temporary index file");
    }

    std::shared_ptr<const IndexSnapshot> candidate;
    try {
        candidate = LoadFile(temporary.GetPath(), std::move(name));
    } catch (const std::runtime_error& error) {
        throw std::invalid_argument(error.what());
    }

    {
        std::lock_guard lock(snapshot_mutex_);
        current_.swap(candidate);
    }
    return Snapshot();
}

std::shared_ptr<const IndexSnapshot> IndexStore::LoadFile(const std::string& path, std::string name) {
    const auto start = std::chrono::steady_clock::now();
    auto index = indexing::IndexLoader{}.Load(path);

    const auto bytes = std::filesystem::file_size(path);
    std::ifstream source(path, std::ios::binary);
    const std::string raw{std::istreambuf_iterator<char>(source), {}};
    if (source.bad() || raw.size() != bytes) {
        throw std::runtime_error("Cannot fingerprint complete index");
    }
    auto sha256 = userver::crypto::hash::Sha256(raw);

    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return std::make_shared<const IndexSnapshot>(std::move(index), std::move(name), bytes, elapsed, std::move(sha256));
}

}  // namespace search_web
