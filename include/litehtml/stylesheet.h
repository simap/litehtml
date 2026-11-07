#ifndef LITEHTML_STYLESHEET_H
#define LITEHTML_STYLESHEET_H

#include <cstdint>
#include <utility>
#include <vector>

#include "css_selector.h"
#include "css_tokenizer.h"

namespace litehtml
{

    // https://www.w3.org/TR/cssom-1/#css-declarations
    struct raw_declaration
    {
        using vector = std::vector<raw_declaration>;

        // property name
        std::string name;
        // default value is specified here to get rid of gcc warning "missing initializer for member"
        css_token_vector value;

        bool important = false;

        operator bool() const
        {
            return name != "";
        }
    };

    // intermediate half-parsed rule that is used internally by the parser
    class raw_rule
    {
      public:
        using ptr    = std::shared_ptr<raw_rule>;
        using vector = std::vector<ptr>;

        enum rule_type
        {
            qualified,
            at
        };

        raw_rule(rule_type _type, std::string _name = {}) :
            type(_type),
            name(std::move(_name))
        {
        }

        rule_type type;
        // An at-rule has a name, a prelude consisting of a list of component values, and an optional block consisting
        // of a simple {} block.
        std::string name;
        // https://www.w3.org/TR/css-syntax-3/#qualified-rule
        // A qualified rule has a prelude consisting of a list of component values, and a block consisting of a simple
        // {} block. Note: Most qualified rules will be style rules, where the prelude is a selector and the block a
        // list of declarations.
        css_token_vector prelude;
        css_token        block;
    };

    class css_parser;

    class css
    {
        css_selector::vector m_selectors;

      public:
        const css_selector::vector& selectors() const
        {
            return m_selectors;
        }

        // Input is either the stylesheet text, which is tokenized while it is parsed, or the component values
        // of a block (@media, @supports, @layer). Each top-level rule is interpreted as soon as it is consumed and
        // then dropped, so neither the token stream nor the rule list of a whole stylesheet is held at once.
        void parse_css_stylesheet(const std::string& input, const std::string& baseurl,
                                  const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media = nullptr,
                                  bool top_level = true);
        void parse_css_stylesheet(const css_token_vector& input, const std::string& baseurl,
                                  const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media = nullptr,
                                  bool top_level = true);

        // Sorts the selectors and builds the rightmost-compound index (see candidates).
        void sort_selectors();

        // Appends to out, in ascending order, the positions in selectors() of the selectors that can match an
        // element with this id, classes and tag. Each selector is filed under one key of its rightmost
        // compound: its first id, else its first class, else its tag, else it is universal; a selector not
        // returned cannot match. Returns false when the index does not cover the current selectors (no
        // sort_selectors since the last add); the caller must then test every selector.
        bool candidates(string_id id, const std::vector<string_id>& classes, string_id tag,
                        std::vector<int>& out) const;

      private:
        struct index_entry
        {
            uint32_t key; // kind in the top two bits (0 id, 1 class, 2 tag, 3 universal), string_id below
            int      pos;
            bool     operator<(const index_entry& o) const { return key != o.key ? key < o.key : pos < o.pos; }
        };
        static uint32_t          index_key(uint32_t kind, string_id name) { return kind << 30 | uint32_t(name); }
        std::vector<index_entry> m_index;           // sorted by key, then position
        size_t                   m_indexed_count = 0; // selectors().size() when m_index was built

        void parse_rules(css_parser& parser, const std::string& baseurl, const std::shared_ptr<document>& doc,
                         const media_query_list_list::ptr& media, bool top_level);
        bool parse_style_rule(const raw_rule::ptr& rule, const std::string& baseurl,
                              const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media);
        void parse_import_rule(const raw_rule::ptr& rule, const std::string& baseurl,
                               const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media);
        void add_selector(const css_selector::ptr& selector);
    };

    inline void css::add_selector(const css_selector::ptr& selector)
    {
        selector->m_order = static_cast<int>(m_selectors.size());
        m_selectors.push_back(selector);
    }

} // namespace litehtml

#endif // LITEHTML_STYLESHEET_H
