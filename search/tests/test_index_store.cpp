#include <gtest/gtest.h>

#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "index_saver.hpp"
#include "index_store.hpp"
#include "indexer.hpp"
#include "test_files.hpp"

namespace {

class IndexStoreTest : public ::testing::Test {
   protected:
    void SetUp() override {
        const auto first =
            indexing::Indexer{}.Build(files_.Write("first.tsv", "First\thttps://example.test/0\tcat\n").string());
        const auto second =
            indexing::Indexer{}.Build(files_.Write("second.tsv", "Second\thttps://example.test/1\tdog\n").string());
        indexing::IndexSaver{}.Save(first, files_.Path("first.bin").string());
        indexing::IndexSaver{}.Save(second, files_.Path("second.bin").string(), 1000000);
    }

    search_test::TestFiles files_;
};

TEST_F(IndexStoreTest, ActivatesValidatedUploadAndPreservesExistingReader) {
    search_web::IndexStore store(files_.Path("first.bin").string());
    const auto previous = store.Snapshot();
    EXPECT_FALSE(previous->index().statistics().indexing.has_value());
    const auto loaded = store.LoadUpload(files_.Read("second.bin"), "Другой индекс.bin");
    EXPECT_EQ(loaded, store.Snapshot());
    EXPECT_EQ(loaded->name(), "Другой индекс.bin");
    EXPECT_EQ(loaded->bytes(), files_.Read("second.bin").size());
    EXPECT_TRUE(loaded->index().statistics().indexing.has_value());
    EXPECT_EQ(loaded->Search("dog"), indexing::DocList{0});
    EXPECT_TRUE(loaded->Search("cat").empty());
    EXPECT_EQ(previous->Search("cat"), indexing::DocList{0});
    EXPECT_EQ(previous->index().documents()[0].title, "First");
}

TEST_F(IndexStoreTest, FailedUploadPreservesActiveIndex) {
    search_web::IndexStore store(files_.Path("first.bin").string());
    const auto previous = store.Snapshot();
    for (const auto& bytes : {std::string{}, std::string{"not an index"}, files_.Read("second.bin").substr(0, 64)}) {
        EXPECT_THROW(store.LoadUpload(bytes, "broken.bin"), std::invalid_argument);
        EXPECT_EQ(store.Snapshot(), previous);
    }
    EXPECT_THROW(store.LoadUpload(files_.Read("second.bin"), std::string("bad\xff")), std::invalid_argument);
    EXPECT_EQ(store.Snapshot(), previous);
}

TEST_F(IndexStoreTest, SnapshotOutlivesStoreAndRestartUsesConfiguredFile) {
    std::shared_ptr<const search_web::IndexSnapshot> retained;
    {
        search_web::IndexStore store(files_.Path("first.bin").string());
        retained = store.LoadUpload(files_.Read("second.bin"), "uploaded.bin");
    }
    EXPECT_EQ(retained->Search("dog"), indexing::DocList{0});
    const search_web::IndexStore restarted(files_.Path("first.bin").string());
    EXPECT_EQ(restarted.Snapshot()->name(), "first.bin");
    EXPECT_EQ(restarted.Snapshot()->Search("cat"), indexing::DocList{0});
}

TEST_F(IndexStoreTest, ConcurrentReadersObserveCompleteSnapshots) {
    search_web::IndexStore store(files_.Path("first.bin").string());
    std::vector<std::future<bool>> readers;
    for (int i = 0; i < 4; ++i) {
        readers.push_back(std::async(std::launch::async, [&store] {
            for (int j = 0; j < 1000; ++j) {
                const auto snapshot = store.Snapshot();
                const auto& title = snapshot->index().documents()[0].title;
                const auto matches = snapshot->Search("cat");
                if ((title == "First") != (matches == indexing::DocList{0})) {
                    return false;
                }
                std::this_thread::yield();
            }
            return true;
        }));
    }
    for (int i = 0; i < 10; ++i) {
        store.LoadUpload(files_.Read(i % 2 == 0 ? "second.bin" : "first.bin"), "replacement.bin");
    }
    for (auto& reader : readers) {
        EXPECT_TRUE(reader.get());
    }
}

TEST_F(IndexStoreTest, RetainsStemLookupAndAnalysisWithItsOriginalSnapshot) {
    const auto index =
        indexing::Indexer{}.Build(files_.Write("forms.tsv", "Forms\thttps://test/forms\tмашины\n").string());
    indexing::IndexSaver{}.Save(index, files_.Path("forms.bin").string());
    search_web::IndexStore store(files_.Path("forms.bin").string());
    const auto previous = store.Snapshot();
    EXPECT_EQ(previous->Search("машина"), (indexing::DocList{0}));
    EXPECT_TRUE(previous->Search("машина", boolsearch::MatchMode::kExact).empty());
    EXPECT_TRUE(previous->zipf().available);
    const auto sha256 = previous->sha256();
    store.LoadUpload(files_.Read("second.bin"), "other.bin");
    EXPECT_EQ(previous->sha256(), sha256);
    EXPECT_NE(store.Snapshot()->sha256(), sha256);
    EXPECT_EQ(previous->Search("машина"), (indexing::DocList{0}));
    EXPECT_TRUE(store.Snapshot()->Search("машина").empty());
}

}  // namespace
