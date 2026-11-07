#ifndef LITEHTML_RENDER_ITEM_LIST_H
#define LITEHTML_RENDER_ITEM_LIST_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <new>
#include <utility>

namespace litehtml
{
    class render_item;

    // Children of a render item. A vector of shared_ptr<render_item> whose size, capacity and elements live in one
    // heap block, so the handle is one pointer and an empty list allocates nothing. It replaces a std::list, which
    // cost one heap node per child. Iterators are pointers: insert and erase invalidate them like std::vector does,
    // and both return a valid iterator.
    class render_item_list
    {
      public:
        using value_type             = std::shared_ptr<render_item>;
        using size_type              = std::size_t;
        using difference_type        = std::ptrdiff_t;
        using reference              = value_type&;
        using const_reference        = const value_type&;
        using iterator               = value_type*;
        using const_iterator         = const value_type*;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

      private:
        struct header
        {
            uint32_t size;
            uint32_t cap;
        };
        static_assert(sizeof(header) % alignof(value_type) == 0, "elements follow the header");

        header* m_h = nullptr;

        value_type* data() const
        {
            return m_h ? reinterpret_cast<value_type*>(m_h + 1) : nullptr;
        }

        void reallocate(size_type new_cap)
        {
            size_type sz = size();
            header*   nh = nullptr;
            if(new_cap)
            {
                nh       = static_cast<header*>(::operator new(sizeof(header) + new_cap * sizeof(value_type)));
                nh->size = static_cast<uint32_t>(sz);
                nh->cap  = static_cast<uint32_t>(new_cap);
                auto* nd = reinterpret_cast<value_type*>(nh + 1);
                auto* od = data();
                for(size_type i = 0; i < sz; i++)
                {
                    new(nd + i) value_type(std::move(od[i]));
                    od[i].~value_type();
                }
            }
            ::operator delete(m_h);
            m_h = nh;
        }

        void grow()
        {
            size_type cap = capacity();
            reallocate(cap ? cap * 2 : 1);
        }

      public:
        render_item_list() = default;

        render_item_list(const render_item_list& other)
        {
            *this = other;
        }

        render_item_list(render_item_list&& other) noexcept :
            m_h(other.m_h)
        {
            other.m_h = nullptr;
        }

        ~render_item_list()
        {
            clear();
        }

        render_item_list& operator=(const render_item_list& other)
        {
            if(this != &other)
            {
                clear();
                if(!other.empty())
                {
                    reallocate(other.size());
                    for(size_type i = 0; i < other.size(); i++)
                    {
                        new(data() + i) value_type(other.data()[i]);
                    }
                    m_h->size = static_cast<uint32_t>(other.size());
                }
            }
            return *this;
        }

        render_item_list& operator=(render_item_list&& other) noexcept
        {
            if(this != &other)
            {
                clear();
                m_h       = other.m_h;
                other.m_h = nullptr;
            }
            return *this;
        }

        size_type size() const
        {
            return m_h ? m_h->size : 0;
        }
        size_type capacity() const
        {
            return m_h ? m_h->cap : 0;
        }
        bool empty() const
        {
            return size() == 0;
        }

        iterator begin()
        {
            return data();
        }
        iterator end()
        {
            return data() + size();
        }
        const_iterator begin() const
        {
            return data();
        }
        const_iterator end() const
        {
            return data() + size();
        }
        const_iterator cbegin() const
        {
            return begin();
        }
        const_iterator cend() const
        {
            return end();
        }
        reverse_iterator rbegin()
        {
            return reverse_iterator(end());
        }
        reverse_iterator rend()
        {
            return reverse_iterator(begin());
        }
        const_reverse_iterator rbegin() const
        {
            return const_reverse_iterator(end());
        }
        const_reverse_iterator rend() const
        {
            return const_reverse_iterator(begin());
        }
        const_reverse_iterator crbegin() const
        {
            return rbegin();
        }
        const_reverse_iterator crend() const
        {
            return rend();
        }

        reference front()
        {
            return data()[0];
        }
        const_reference front() const
        {
            return data()[0];
        }
        reference back()
        {
            return data()[size() - 1];
        }
        const_reference back() const
        {
            return data()[size() - 1];
        }

        void push_back(value_type val)
        {
            if(size() == capacity())
            {
                grow();
            }
            new(data() + m_h->size) value_type(std::move(val));
            m_h->size++;
        }

        iterator insert(const_iterator pos, value_type val)
        {
            size_type idx = static_cast<size_type>(pos - cbegin());
            push_back(std::move(val));
            // rotate the new last element into place
            for(size_type i = size() - 1; i > idx; i--)
            {
                std::swap(data()[i], data()[i - 1]);
            }
            return begin() + idx;
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            size_type f = static_cast<size_type>(first - cbegin());
            size_type l = static_cast<size_type>(last - cbegin());
            if(f == l)
            {
                return begin() + f;
            }
            iterator d = begin();
            std::move(d + l, d + size(), d + f);
            size_type new_size = size() - (l - f);
            for(size_type i = new_size; i < size(); i++)
            {
                d[i].~value_type();
            }
            m_h->size = static_cast<uint32_t>(new_size);
            return begin() + f;
        }

        iterator erase(const_iterator pos)
        {
            return erase(pos, pos + 1);
        }

        void clear()
        {
            if(m_h)
            {
                for(size_type i = 0; i < size(); i++)
                {
                    data()[i].~value_type();
                }
                ::operator delete(m_h);
                m_h = nullptr;
            }
        }

        void shrink_to_fit()
        {
            if(capacity() != size())
            {
                reallocate(size());
            }
        }
    };
} // namespace litehtml

#endif // LITEHTML_RENDER_ITEM_LIST_H
