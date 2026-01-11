#pragma once

/**
 * @file line_reader.h
 * @brief Buffered line reader with support for various input sources
 */

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace wikilib::core {

// ============================================================================
// Abstract line reader interface
// ============================================================================

/**
 * @brief Abstract interface for reading lines from a source
 */
class LineReader {
public:
    virtual ~LineReader() = default;

    /**
     * @brief Read next line from source
     * @param line Output string to store the line
     * @return true if line was read, false if EOF or error
     */
    virtual bool read_line(std::string& line) = 0;

    /**
     * @brief Check if at end of stream
     */
    [[nodiscard]] virtual bool eof() const noexcept = 0;
};

// ============================================================================
// Buffered line reader
// ============================================================================

/**
 * @brief Buffered line reader with support for lines spanning buffer boundaries
 *
 * This reader efficiently handles:
 * - Lines longer than buffer size
 * - Lines split across buffer boundaries
 * - Multiple line ending styles (\n, \r, \r\n)
 * - Large block reads for performance
 */
class BufferedLineReader : public LineReader {
public:
    /**
     * @brief Create buffered reader with specified buffer size
     * @param buffer_size Size of internal buffer (default 512KB)
     */
    explicit BufferedLineReader(size_t buffer_size = 512 * 1024);

    ~BufferedLineReader() override;

    // Non-copyable, movable
    BufferedLineReader(const BufferedLineReader&) = delete;
    BufferedLineReader& operator=(const BufferedLineReader&) = delete;
    BufferedLineReader(BufferedLineReader&&) noexcept;
    BufferedLineReader& operator=(BufferedLineReader&&) noexcept;

    /**
     * @brief Read next line from source
     */
    bool read_line(std::string& line) override;

    /**
     * @brief Check if at end of stream
     */
    [[nodiscard]] bool eof() const noexcept override;

protected:
    /**
     * @brief Read next buffer from source
     *
     * Derived classes must implement this to read from their specific source.
     * Should set bytes_read_ to number of bytes read and eof_ flag if at end.
     */
    virtual void fill_buffer() = 0;

    // Buffer state
    char* buffer_ = nullptr;
    size_t buffer_size_;
    size_t bytes_read_ = 0;     // Bytes read in last fill_buffer()
    bool eof_ = false;          // At end of stream
    bool initialized_ = false;  // First buffer loaded

    // Line parsing state
    size_t line_start_ = 0;     // Start of current line in buffer
    size_t eol_pos_ = 0;        // End of line position
    std::string result_;        // Accumulated line data for long lines

private:
    bool find_eol_in_buffer();
    void find_next_eol();
    void set_start_after_eol();
    void skip_eol();
    static bool is_eol(char c) noexcept;
};

// ============================================================================
// Stream-based line reader (for testing with regular files)
// ============================================================================

/**
 * @brief Simple line reader using std::istream
 *
 * Uses standard getline() - efficient for regular files,
 * but BufferedLineReader is better for compressed streams.
 */
class StreamLineReader : public LineReader {
public:
    explicit StreamLineReader(std::istream& stream);

    bool read_line(std::string& line) override;
    [[nodiscard]] bool eof() const noexcept override;

private:
    std::istream& stream_;
    bool eof_ = false;
};

} // namespace wikilib::core
