#ifndef LITEHTML_CSS_PARSER_H
#define LITEHTML_CSS_PARSER_H

#include "css_tokenizer.h"
#include "stylesheet.h"
#include <functional>

namespace litehtml
{

    // Decode (UTF-8) and filter code points. Returns input itself when both steps are no-ops, which is the
    // common case, otherwise fills storage and returns it.
    // https://www.w3.org/TR/css-syntax-3/#parse-stylesheet steps 1 and 2 (before tokenization)
    const std::string& prepare_css_input(const std::string& input, std::string& storage);

    class css_parser
    {
        // Token source: either a token vector (m_tokens) or a tokenizer that is pulled one token at a time
        // (m_tokenizer), so that the token stream of a whole stylesheet never exists at once.
        const css_token_vector* m_tokens    = nullptr;
        int                     m_index     = 0;
        css_tokenizer*          m_tokenizer = nullptr;
        css_token               m_current;           // stream mode: the current input token
        bool                    m_reconsume = false; // stream mode: next_token returns m_current again

        // The returned reference is valid until the next call of next_token or peek_token.
        const css_token& next_token();
        const css_token& peek_token();
        // https://www.w3.org/TR/css-syntax-3/#reconsume-the-current-input-token
        void reconsume();

      public:
        css_parser(const css_token_vector& tokens) :
            m_tokens(&tokens)
        {
        }
        css_parser(css_tokenizer& tokenizer) :
            m_tokenizer(&tokenizer)
        {
        }

        static raw_rule::vector parse_stylesheet(const std::string& input, bool top_level);
        static raw_rule::vector parse_stylesheet(const css_token_vector& input, bool top_level);
        raw_rule::vector        consume_list_of_rules(bool top_level);
        // One step of consume_list_of_rules. Returns false at EOF. Otherwise rule is the consumed rule, or null
        // when the input produced none (whitespace, CDO/CDC at top level, an invalid qualified rule).
        bool                    consume_rule(bool top_level, raw_rule::ptr& rule);
        raw_rule::ptr           consume_qualified_rule();
        raw_rule::ptr           consume_at_rule();
        css_token               consume_simple_block(char opening_bracket);
        css_token               consume_component_value();
        css_token               consume_function(const std::string& name);

        raw_declaration consume_declaration();
        void consume_style_block_contents(/*out*/ raw_declaration::vector& decls, /*out*/ raw_rule::vector& rules);
    };

    using keep_whitespace_fn = std::function<bool(const css_token& left, const css_token& right)>;
    void remove_whitespace(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace = nullptr);

    enum
    {
        f_componentize      = 1,
        f_remove_whitespace = 2
    };
    css_token_vector normalize(css_token_vector input, int options = 0, const keep_whitespace_fn& keep_whitespace = nullptr);
    css_token_vector normalize(std::string input, int options = 0, const keep_whitespace_fn& keep_whitespace = nullptr);

    std::vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens);
    bool                          is_declaration_value(const css_token_vector& tokens, int index = 0);
    bool                          is_any_value(const css_token_vector& tokens);
    bool                          skip_whitespace(const css_token_vector& tokens, int& index);

} // namespace litehtml

#endif // LITEHTML_CSS_PARSER_H
