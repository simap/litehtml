#include "text_arena.h"
#include <cstring>

namespace litehtml
{
    char* text_arena::alloc(size_t n)
    {
        m_used += n;
        m_strings++;
        if(n >= own_block)
        {
            m_blocks.push_back({std::unique_ptr<char[]>(new char[n]), n});
            return m_blocks.back().data.get();
        }
        if(n > m_left)
        {
            m_blocks.push_back({std::unique_ptr<char[]>(new char[chunk_size]), chunk_size});
            m_cur  = m_blocks.back().data.get();
            m_left = chunk_size;
        }
        char* ret  = m_cur;
        m_cur     += n;
        m_left    -= n;
        return ret;
    }

    const char* text_arena::add(const char* s, size_t n)
    {
        char* dst = alloc(n + 1);
        memcpy(dst, s, n);
        dst[n] = 0;
        return dst;
    }

    const char* text_arena::add_pair(const char* a, size_t na, const char* b, size_t nb)
    {
        char* dst = alloc(na + nb + 2);
        memcpy(dst, a, na);
        dst[na] = 0;
        memcpy(dst + na + 1, b, nb);
        dst[na + 1 + nb] = 0;
        return dst;
    }
} // namespace litehtml
