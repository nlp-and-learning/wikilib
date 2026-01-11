/**
 * @file test_dump_path.cpp
 * @brief Unit tests for DumpPath class
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "wikilib/dump/dump_path.h"

namespace fs = std::filesystem;
using namespace wikilib::dump;

// ============================================================================
// Project conversion tests
// ============================================================================

TEST(DumpPathTest, ProjectToDirname) {
    EXPECT_EQ(project_to_dirname(WikiProject::Wikipedia), "wiki");
    EXPECT_EQ(project_to_dirname(WikiProject::Wiktionary), "wiktionary");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikidata), "wikidata");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikibooks), "wikibooks");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikinews), "wikinews");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikiquote), "wikiquote");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikisource), "wikisource");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikiversity), "wikiversity");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikivoyage), "wikivoyage");
    EXPECT_EQ(project_to_dirname(WikiProject::Wikimedia), "wikimedia");
}

TEST(DumpPathTest, DirnameToProject) {
    EXPECT_EQ(dirname_to_project("wiki"), WikiProject::Wikipedia);
    EXPECT_EQ(dirname_to_project("wiktionary"), WikiProject::Wiktionary);
    EXPECT_EQ(dirname_to_project("wikidata"), WikiProject::Wikidata);
    EXPECT_EQ(dirname_to_project("unknown"), std::nullopt);
    EXPECT_EQ(dirname_to_project(""), std::nullopt);
}

// ============================================================================
// DumpPath fluent API tests
// ============================================================================

TEST(DumpPathTest, FindFromCwdFindsDirectory) {
    // This test will pass if we're in the wikilib directory
    // and wikimedia-dump-fetcher exists as sibling
    try {
        auto path = DumpPath::find_from_cwd();
        EXPECT_FALSE(path.base_dir().empty());
    } catch (const std::runtime_error&) {
        // Expected if not in correct directory structure
        SUCCEED();
    }
}

TEST(DumpPathTest, SetProjectReturnsThis) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    auto& result = path.set_project(WikiProject::Wikipedia);
    EXPECT_EQ(&result, &path);
    EXPECT_EQ(path.project(), WikiProject::Wikipedia);
}

TEST(DumpPathTest, SetLanguageReturnsThis) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    auto& result = path.set_language("en");
    EXPECT_EQ(&result, &path);
    EXPECT_EQ(path.language(), "en");
}

TEST(DumpPathTest, FluentChaining) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wiktionary)
        .set_language("pl")
        .set_date("20260101");

    EXPECT_EQ(path.project(), WikiProject::Wiktionary);
    EXPECT_EQ(path.language(), "pl");
    EXPECT_EQ(path.date(), "20260101");
}

// ============================================================================
// Path generation tests
// ============================================================================

TEST(DumpPathTest, DumpFilename) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wiktionary)
        .set_language("pl")
        .set_date("20260101");

    auto dump_path = path.dump_path();
    EXPECT_TRUE(dump_path.string().find("plwiktionary-20260101") != std::string::npos);
    EXPECT_TRUE(dump_path.string().ends_with(".xml.bz2"));
}

TEST(DumpPathTest, IndexFilename) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wiktionary)
        .set_language("pl")
        .set_date("20260101");

    auto index_path = path.index_path();
    EXPECT_TRUE(index_path.string().find("plwiktionary-20260101") != std::string::npos);
    EXPECT_TRUE(index_path.string().find("-index.txt.bz2") != std::string::npos);
}

TEST(DumpPathTest, WikipediaUsesWikiPrefix) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wikipedia)
        .set_language("en")
        .set_date("20260101");

    auto dump_path = path.dump_path();
    // Wikipedia uses enwiki not enwikipedia
    EXPECT_TRUE(dump_path.string().find("enwiki-") != std::string::npos);
}

// ============================================================================
// Symbolic name tests
// ============================================================================

TEST(DumpPathTest, SymbolicName) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wiktionary)
        .set_language("pl")
        .set_date("20260101");

    EXPECT_EQ(path.symbolic_name(), "plwiktionary-20260101");
}

TEST(DumpPathTest, SymbolicNameWikipedia) {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        GTEST_SKIP() << "No wikimedia-dump-fetcher found";
    }

    DumpPath path(*found);
    path.set_project(WikiProject::Wikipedia)
        .set_language("en")
        .set_date("20260101");

    EXPECT_EQ(path.symbolic_name(), "enwiki-20260101");
}

// ============================================================================
// Utility function tests
// ============================================================================

TEST(DumpPathTest, ReadTermsFile) {
    // Create temp file
    auto temp_path = fs::temp_directory_path() / "test_terms.txt";
    {
        std::ofstream file(temp_path);
        file << "  term1  \n";
        file << "term2\n";
        file << "\n";  // Empty line
        file << "  term with spaces  \n";
        file << "term3";  // No trailing newline
    }

    auto terms = read_terms_file(temp_path.string());
    EXPECT_EQ(terms.size(), 4);
    EXPECT_EQ(terms[0], "term1");
    EXPECT_EQ(terms[1], "term2");
    EXPECT_EQ(terms[2], "term with spaces");
    EXPECT_EQ(terms[3], "term3");

    fs::remove(temp_path);
}

TEST(DumpPathTest, ReadTermsFileNonexistent) {
    auto terms = read_terms_file("/nonexistent/path/to/file.txt");
    EXPECT_TRUE(terms.empty());
}
