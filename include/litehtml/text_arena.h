#ifndef LITEHTML_TEXT_ARENA_H
#define LITEHTML_TEXT_ARENA_H

#include <cstddef>
#include <memory>
#include <vector>

namespace litehtml
{
    // Append-only storage for the text of el_text and el_space nodes, owned by the document.
    // Strings are NUL-terminated, because document_container::text_width and draw_text take a const char*.
    // They never move and are freed only with the document, so elements hold a plain const char*.
    // Strings are read-only and can be shared: the empty string and every one-byte ASCII string point into a
    // static table, and white-space runs are stored once each (up to max_runs distinct runs).
    class text_arena
    {
      public:
        // Returns a NUL-terminated copy of the n bytes at s, or a shared string equal to it.
        const char* add(const char* s, size_t n);
        // Copies a, a NUL, b and a NUL into one block. Returns the copy of a; the copy of b follows its NUL.
        const char* add_pair(const char* a, size_t na, const char* b, size_t nb);

      private:
        struct block
        {
            std::unique_ptr<char[]> data;
            size_t                  size;
        };
        static constexpr size_t chunk_size = 2048;  // shared blocks
        static constexpr size_t own_block  = 512;   // strings this long or longer get a block of their own
        static constexpr size_t max_runs   = 64;    // distinct white-space runs that are shared
        static constexpr size_t max_run    = 64;    // longest white-space run that is shared

        char* alloc(size_t n);

        std::vector<block>       m_blocks;
        std::vector<const char*> m_runs; // shared white-space runs
        char*              m_cur     = nullptr; // free space in the current shared block
        size_t             m_left    = 0;
        size_t             m_used    = 0; // bytes handed out, for statistics
        size_t             m_strings = 0; // strings handed out, for statistics
    };
} // namespace litehtml

#endif // LITEHTML_TEXT_ARENA_H
