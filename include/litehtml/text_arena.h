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
    class text_arena
    {
      public:
        // Copies n bytes of s and a NUL. Returns the copy.
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

        char* alloc(size_t n);

        std::vector<block> m_blocks;
        char*              m_cur     = nullptr; // free space in the current shared block
        size_t             m_left    = 0;
        size_t             m_used    = 0; // bytes handed out, for statistics
        size_t             m_strings = 0; // strings handed out, for statistics
    };
} // namespace litehtml

#endif // LITEHTML_TEXT_ARENA_H
