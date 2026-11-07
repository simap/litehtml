#ifndef LITEHTML_ELEMENT_H
#define LITEHTML_ELEMENT_H

#include "html.h"
#include "types.h"
#include "css_properties.h"
#include "stylesheet.h"
#include <functional>
#include <list>
#include <memory>
#include <tuple>

namespace litehtml
{
    class line_box;
    class dumper;
    class render_item;

    // The render items of an element, in insertion order. Almost every element has zero or one, so the first is
    // stored inline and further ones in a vector allocated on demand. It replaces a std::list, which cost one heap
    // node per render item.
    class render_item_refs
    {
        using ref = std::weak_ptr<render_item>;

        ref                               m_first;
        std::unique_ptr<std::vector<ref>> m_more;

      public:
        class const_iterator
        {
            const render_item_refs* m_refs;
            size_t                  m_idx;

          public:
            const_iterator(const render_item_refs* refs, size_t idx) :
                m_refs(refs),
                m_idx(idx)
            {
            }
            const ref& operator*() const
            {
                return m_idx == 0 ? m_refs->m_first : (*m_refs->m_more)[m_idx - 1];
            }
            const_iterator& operator++()
            {
                ++m_idx;
                return *this;
            }
            bool operator!=(const const_iterator& other) const
            {
                return m_idx != other.m_idx;
            }
        };

        bool empty() const
        {
            // true when m_first was never assigned (shares ownership with nothing)
            return !m_first.owner_before(ref{}) && !ref{}.owner_before(m_first);
        }
        size_t size() const
        {
            return empty() ? 0 : 1 + (m_more ? m_more->size() : 0);
        }
        const ref& front() const
        {
            return m_first;
        }
        void push_back(const std::shared_ptr<render_item>& ri)
        {
            if(empty())
            {
                m_first = ri;
            } else
            {
                if(!m_more)
                {
                    m_more = std::make_unique<std::vector<ref>>();
                }
                m_more->emplace_back(ri);
            }
        }
        const_iterator begin() const
        {
            return {this, 0};
        }
        const_iterator end() const
        {
            return {this, size()};
        }
    };

    class element : public std::enable_shared_from_this<element>
    {
        friend class line_box;
        friend class html_tag;
        friend class el_table;
        friend class document;

      public:
        using ptr       = std::shared_ptr<element>;
        using const_ptr = std::shared_ptr<const element>;
        using weak_ptr  = std::weak_ptr<element>;

      protected:
        std::weak_ptr<element>                m_parent;
        std::weak_ptr<document>               m_doc;
        elements_list                         m_children;
        const css_properties*                 m_css; // interned, owned by the document (see set_css)
        // render items of this element in creation order: the first inline, later ones (rare) in a vector
        render_item_refs                      m_renders;

        virtual void select_all(const css_selector& selector, elements_list& res);
        element::ptr _add_before_after(int type, const style& style);

        // Calls f(const std::shared_ptr<render_item>&) for every live render item in creation order until f
        // returns false.
        template <class F> void for_each_render(F&& f) const
        {
            for(const auto& weak_ri : m_renders)
            {
                if(auto ri = weak_ri.lock())
                {
                    if(!f(ri))
                    {
                        return;
                    }
                }
            }
        }

      private:
        // null until a counter is reset or incremented on this element (few elements have one)
        std::unique_ptr<std::map<string_id, int>> m_counter_values;
        std::map<string_id, int>&                 counter_values();

      public:
        explicit element(const std::shared_ptr<document>& doc);
        virtual ~element() = default;

        const css_properties& css() const;
        // Replaces this element's style with the interned instance equal to style.
        void set_css(const css_properties& style);
        // Copy-on-write: copies the current style, applies f to the copy, and interns the result.
        template <class F> void modify_css(F&& f)
        {
            css_properties tmp = *m_css;
            f(tmp);
            set_css(tmp);
        }

        bool     in_normal_flow() const;
        bool     is_inline() const;     // returns true if element is inline
        bool     is_inline_box() const; // returns true if element is inline box (inline-table, inline-box, inline-flex)
        bool     is_block_box() const;
        position get_placement() const;
        bool     is_positioned() const;
        bool     is_float() const;
        bool     is_block_formatting_context() const;
        bool     is_flex_item() const;

        pixel_t v_scroll(pixel_t dy) const;
        pixel_t h_scroll(pixel_t dx) const;

        bool         is_root() const;
        element::ptr parent() const;
        void         parent(const element::ptr& par);
        // returns true for elements inside a table (but outside cells) that don't participate in table rendering
        bool is_table_skip() const;

        std::shared_ptr<document>                  get_document() const;
        const elements_list&                       children() const;

        std::shared_ptr<render_item> get_render_item();

        virtual elements_list select_all(const std::string& selector);
        virtual elements_list select_all(const css_selector& selector);

        virtual element::ptr select_one(const std::string& selector);
        virtual element::ptr select_one(const css_selector& selector);

        virtual bool appendChild(const ptr& el);
        virtual bool removeChild(const ptr& el);
        virtual void clearRecursive();

        virtual string_id   id() const;
        virtual string_id   tag() const;
        virtual const char* get_tagName() const;
        virtual void        set_tagName(const char* tag);
        virtual void        set_data(const char* data);

        virtual void        set_attr(const char* name, const char* val);
        virtual void        remove_attr(const char* name);
        virtual const char* get_attr(const char* name, const char* def = nullptr) const;
        virtual void        apply_stylesheet(const litehtml::css& stylesheet);
        // For each element in document order: apply author, apply user, compute the style. Same result as
        // apply_stylesheet(author) and apply_stylesheet(user) over the tree followed by compute_styles(), as long
        // as the user stylesheet has no counter properties, but an element's full declared style only lives until
        // its own compute instead of until the whole tree is styled.
        virtual void apply_stylesheets_and_compute(const litehtml::css& author, const litehtml::css& user);
        virtual void        refresh_styles();
        virtual bool        is_white_space() const;
        virtual bool        is_space() const;
        virtual bool        is_comment() const;
        virtual bool        is_body() const;
        virtual bool        is_break() const;
        virtual bool        is_text() const;

        virtual bool on_mouse_over();
        virtual bool on_mouse_leave();
        virtual bool on_lbutton_down();
        virtual bool on_lbutton_up(bool is_click = true);
        virtual void on_click();
        virtual bool set_pseudo_class(string_id cls, bool add);
        virtual bool set_class(const char* pclass, bool add);
        virtual bool is_replaced() const;
        virtual void compute_styles(bool recursive = true);
        virtual void draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                          const std::shared_ptr<render_item>& ri);
        virtual void draw_background(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                                     const std::shared_ptr<render_item>& ri);

        virtual void              get_text(std::string& text) const;
        virtual void              parse_attributes();
        virtual int               select(const css_selector::vector& selector_list, bool apply_pseudo = true);
        virtual int               select(const std::string& selector);
        virtual int               select(const css_selector& selector, bool apply_pseudo = true);
        virtual int               select(const css_element_selector& selector, bool apply_pseudo = true);
        virtual element::ptr      find_ancestor(const css_selector& selector, bool apply_pseudo = true,
                                                bool* is_pseudo = nullptr);
        virtual bool              is_ancestor(const ptr& el) const;
        virtual element::ptr      find_adjacent_sibling(const element::ptr& el, const css_selector& selector,
                                                        bool apply_pseudo = true, bool* is_pseudo = nullptr);
        virtual element::ptr      find_sibling(const element::ptr& el, const css_selector& selector,
                                               bool apply_pseudo = true, bool* is_pseudo = nullptr);
        virtual void              get_content_size(size& sz, pixel_t max_width);
        virtual bool              is_nth_child(const element::ptr& el, int num, int off, bool of_type,
                                               const css_selector::vector& selector_list = {}) const;
        virtual bool              is_nth_last_child(const element::ptr& el, int num, int off, bool of_type,
                                                    const css_selector::vector& selector_list = {}) const;
        virtual bool              is_only_child(const element::ptr& el, bool of_type) const;
        virtual void              add_style(const style& style);
        virtual const background* get_background(bool own_only = false);

        virtual std::string                                       dump_get_name();
        virtual std::vector<std::tuple<std::string, std::string>> dump_get_attrs();
        void                                                      dump(litehtml::dumper& cout);

        std::tuple<element::ptr, element::ptr, element::ptr> split_inlines();
        virtual std::shared_ptr<render_item> create_render_item(const std::shared_ptr<render_item>& parent_ri);

        // true if a selector this element matched (or did not) changes result, see html_tag::m_used_styles
        virtual bool requires_styles_update();
        void         add_render(const std::shared_ptr<render_item>& ri);
        bool find_styles_changes(const std::function<void(const position&)>& redraw_box);

        element::ptr add_pseudo_before(const style& style)
        {
            return _add_before_after(0, style);
        }
        element::ptr add_pseudo_after(const style& style)
        {
            return _add_before_after(1, style);
        }

        std::string get_counter_value(const std::string& counter_name);
        std::string get_counters_value(const std::vector<std::string>& parameters);
        void        increment_counter(const string_id& counter_name_id, int increment = 1);
        void        reset_counter(const string_id& counter_name_id, int value = 0);

        void run_on_renderers(const std::function<bool(const std::shared_ptr<render_item>&)>& func);

      private:
        std::vector<element::ptr> get_siblings_before() const;
        bool find_counter(string_id counter_name_id, std::map<string_id, int>::iterator& map_iterator);
        void parse_counter_tokens(const std::vector<std::string>& tokens, int default_value,
                                  const std::function<void(string_id, int)>& handler) const;
    };

    //////////////////////////////////////////////////////////////////////////
    //							INLINE FUNCTIONS							//
    //////////////////////////////////////////////////////////////////////////

    inline bool litehtml::element::in_normal_flow() const
    {
        return css().get_position() != element_position_absolute && css().get_display() != display_none &&
               css().get_position() != element_position_fixed;
    }

    inline bool litehtml::element::is_root() const
    {
        return m_parent.expired();
    }

    inline element::ptr litehtml::element::parent() const
    {
        return m_parent.lock();
    }

    inline void litehtml::element::parent(const element::ptr& par)
    {
        m_parent = par;
    }

    inline bool litehtml::element::is_positioned() const
    {
        return (css().get_position() > element_position_static);
    }

    inline bool litehtml::element::is_float() const
    {
        return (css().get_float() != float_none);
    }

    inline bool litehtml::element::is_flex_item() const
    {
        auto par = parent();
        if(par)
        {
            return is_one_of(par->css().get_display(), display_flex, display_inline_flex);
        }
        return false;
    }

    inline std::shared_ptr<document> element::get_document() const
    {
        return m_doc.lock();
    }

    inline const css_properties& element::css() const
    {
        return *m_css;
    }

    inline bool element::is_block_box() const
    {
        return css().get_display() == display_block || css().get_display() == display_flex ||
               css().get_display() == display_table || css().get_display() == display_list_item;
    }

    inline const elements_list& element::children() const
    {
        return m_children;
    }

    inline std::shared_ptr<render_item> element::get_render_item()
    {
        return m_renders.front().lock();
    }
} // namespace litehtml

#endif // LITEHTML_ELEMENT_H
