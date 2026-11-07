#include "text_arena.h"
#include <cstring>

namespace litehtml
{
    namespace
    {
        // "\0\0" "\x01\0" ... "\x7f\0": every one-byte ASCII string, and the empty string at offset 0
        struct single_bytes
        {
            char c[256];
            constexpr single_bytes() :
                c()
            {
                for(int i = 0; i < 128; i++)
                {
                    c[2 * i] = static_cast<char>(i);
                }
            }
        };
        constexpr single_bytes k_single;

        bool is_space_run(const char* s, size_t n)
        {
            for(size_t i = 0; i < n; i++)
            {
                if(s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r' && s[i] != '\f')
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

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
        if(n == 0)
        {
            return k_single.c;
        }
        if(n == 1 && static_cast<unsigned char>(s[0]) < 128)
        {
            return k_single.c + 2 * s[0];
        }
        bool run = n <= max_run && is_space_run(s, n);
        if(run)
        {
            for(const char* r : m_runs)
            {
                if(!strncmp(r, s, n) && !r[n])
                {
                    return r;
                }
            }
        }
        char* dst = alloc(n + 1);
        memcpy(dst, s, n);
        dst[n] = 0;
        if(run && m_runs.size() < max_runs)
        {
            m_runs.push_back(dst);
        }
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
