#include "encodings.h"
#include "html.h"
#include "css_parser.h"

namespace litehtml
{

    // https://www.w3.org/TR/css-syntax-3/#css-filter-code-points
    void filter_code_points(std::string& input)
    {
        const char* xFFFD = "\xEF\xBF\xBD";

        size_t null_count = std::count(input.begin(), input.end(), 0);

        if(null_count == 0)
        {
            // the output is never longer than the input: filter in place, no second copy of the text
            size_t j = 0;
            for(size_t i = 0; i < input.size(); i++)
            {
                char ch = input[i];
                if(ch == '\r')
                {
                    input[j++] = '\n';
                    if(i + 1 < input.size() && input[i + 1] == '\n')
                    {
                        i++; // skip \n after \r
                    }
                } else
                {
                    input[j++] = ch == '\f' ? '\n' : ch;
                }
            }
            input.resize(j);
            return;
        }

        std::string result(input.size() + 2 * null_count, 0);

        for(int i = 0, j = 0; i < static_cast<int>(input.size()); i++)
        {
            switch(input[i])
            {
            case '\r':
                result[j++] = '\n';
                if(i + 1 < static_cast<int>(input.size()) && input[i + 1] == '\n')
                {
                    i++; // skip \n after \r
                }
                break;
            case '\f':
                result[j++] = '\n';
                break;
            case 0:
                memcpy(&result[j], xFFFD, 3);
                j += 3;
                break;
            default:
                result[j++] = input[i];
            }
        }

        // trim trailing NULs
        result.resize(strlen(result.c_str()));
        input = std::move(result);
    }

    // True if decoding input as UTF-8 and filtering code points would both return it unchanged: no BOM, valid
    // UTF-8 per https://encoding.spec.whatwg.org/#utf-8-decoder, and no CR, FF or NUL.
    static bool is_plain_css_input(const std::string& input)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(input.data());
        size_t      n = input.size();
        if(n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
        {
            return false;
        }
        for(size_t i = 0; i < n;)
        {
            unsigned c = p[i];
            if(c < 0x80)
            {
                if(c == '\r' || c == '\f' || c == 0)
                {
                    return false;
                }
                i++;
                continue;
            }
            size_t   len   = 0;
            unsigned lower = 0x80;
            unsigned upper = 0xBF;
            if(c >= 0xC2 && c <= 0xDF)
            {
                len = 1;
            } else if(c >= 0xE0 && c <= 0xEF)
            {
                len   = 2;
                lower = c == 0xE0 ? 0xA0 : lower;
                upper = c == 0xED ? 0x9F : upper;
            } else if(c >= 0xF0 && c <= 0xF4)
            {
                len   = 3;
                lower = c == 0xF0 ? 0x90 : lower;
                upper = c == 0xF4 ? 0x8F : upper;
            } else
            {
                return false;
            }
            if(i + len >= n || p[i + 1] < lower || p[i + 1] > upper)
            {
                return false;
            }
            for(size_t k = 2; k <= len; k++)
            {
                if(p[i + k] < 0x80 || p[i + k] > 0xBF)
                {
                    return false;
                }
            }
            i += len + 1;
        }
        return true;
    }

    const std::string& prepare_css_input(const std::string& input, std::string& storage)
    {
        if(is_plain_css_input(input))
        {
            return input;
        }
        // decoding potentially broken UTF-8 into valid UTF-8
        storage = decode(input, encoding::utf_8);
        filter_code_points(storage);
        return storage;
    }

    static const size_t kLargeSize = 50;
    static void         remove_whitespace_large(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace);
    static void         remove_whitespace_small(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace);

    void remove_whitespace_large(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace)
    {
        std::vector<int> keep_idx;
        keep_idx.reserve(tokens.size());
        for(int i = 0; i < static_cast<int>(tokens.size()); ++i)
        {
            auto& tok  = tokens[i];
            bool  keep = true;
            if(tok.type == ' ')
            {
                const auto& left  = i > 0 ? tokens[i - 1] : css_token();
                const auto& right = at(tokens, i + 1);
                keep              = keep_whitespace && keep_whitespace(left, right);
            } else if(tok.is_component_value())
            {
                if(tok.value.size() > kLargeSize)
                {
                    remove_whitespace_large(tok.value, keep_whitespace);
                } else
                {
                    remove_whitespace_small(tok.value, keep_whitespace);
                }
            }
            if(keep)
            {
                keep_idx.push_back(i);
            }
        }

        if(keep_idx.size() == tokens.size())
        {
            return;
        }
        css_token_vector tmp;
        tmp.reserve(keep_idx.size());
        for(auto idx : keep_idx)
        {
            tmp.push_back(std::move(tokens[idx]));
        }
        tokens.swap(tmp);
    }

    void remove_whitespace_small(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace)
    {
        for(int i = 0; i < static_cast<int>(tokens.size()); i++)
        {
            auto& tok = tokens[i];
            if(tok.type == ' ')
            {
                const auto& left  = i > 0 ? tokens[i - 1] : css_token();
                const auto& right = at(tokens, i + 1);
                bool        keep  = keep_whitespace && keep_whitespace(left, right);
                if(!keep)
                {
                    remove(tokens, i), i--;
                }
            } else if(tok.is_component_value())
            {
                if(tok.value.size() > kLargeSize)
                {
                    remove_whitespace_large(tok.value, keep_whitespace);
                } else
                {
                    remove_whitespace_small(tok.value, keep_whitespace);
                }
            }
        }
    }

    void remove_whitespace(css_token_vector& tokens, const keep_whitespace_fn& keep_whitespace)
    {
        if(tokens.size() > kLargeSize)
        {
            remove_whitespace_large(tokens, keep_whitespace);
        } else
        {
            remove_whitespace_small(tokens, keep_whitespace);
        }
    }

    void componentize(css_token_vector& tokens)
    {
        css_parser       parser(tokens);
        css_token_vector result;
        while(true)
        {
            css_token tok = parser.consume_component_value();
            if(tok.type == EOF)
            {
                break;
            }
            result.push_back(std::move(tok));
        }
        tokens = std::move(result); // EXP
    }

    // https://www.w3.org/TR/css-syntax-3/#normalize-into-a-token-stream
    css_token_vector normalize(css_token_vector input, int options, const keep_whitespace_fn& keep_whitespace)
    {
        if(options & f_componentize)
        {
            componentize(input);
        }
        if(options & f_remove_whitespace)
        {
            remove_whitespace(input, keep_whitespace);
        }
        return input;
    }
    css_token_vector normalize(std::string input, int options, const keep_whitespace_fn& keep_whitespace)
    {
        filter_code_points(input);
        auto tokens = tokenize(input);
        return normalize(std::move(tokens), options, keep_whitespace); // EXP
    }

    // https://www.w3.org/TR/css-syntax-3/#parse-stylesheet
    // I don't create a stylesheet because its only purpose is to pass a list of rules to
    // parse_css_stylesheet. I just return the list of rules directly instead.
    // NOTE: css::parse_css_stylesheet does not use this; it pulls tokens and handles one rule at a time.
    raw_rule::vector css_parser::parse_stylesheet(const std::string& input, bool top_level)
    {
        // 1. If input is a byte stream for stylesheet, decode bytes from input, and set input to the result.
        // not implemented, utf-8 is always assumed
        // 2. Normalize input, and set input to the result.
        std::string        storage;
        const std::string& str = prepare_css_input(input, storage);
        css_tokenizer      tokenizer(str);

        return css_parser(tokenizer).consume_list_of_rules(top_level);
    }
    raw_rule::vector css_parser::parse_stylesheet(const css_token_vector& input, bool top_level)
    {
        // 3. Create a new stylesheet, with its location set to location.
        // 4. Consume a list of rules from input, with the top-level flag set, and set the stylesheet’s value to the
        // result.
        // 5. Return the stylesheet.
        return css_parser(input).consume_list_of_rules(top_level);
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-the-next-input-token
    static const css_token& eof_token()
    {
        static const css_token eof(_EOF);
        return eof;
    }

    const css_token& css_parser::next_token()
    {
        if(m_tokenizer)
        {
            if(m_reconsume)
            {
                m_reconsume = false;
            } else
            {
                m_current = m_tokenizer->next_token();
            }
            return m_current;
        }
        if(m_index == static_cast<int>(m_tokens->size()))
        {
            return eof_token();
        }
        return (*m_tokens)[m_index++];
    }

    const css_token& css_parser::peek_token()
    {
        if(m_tokenizer)
        {
            next_token();
            m_reconsume = true;
            return m_current;
        }
        if(m_index == static_cast<int>(m_tokens->size()))
        {
            return eof_token();
        }
        return (*m_tokens)[m_index];
    }

    // Only called right after next_token returned a token that is not EOF.
    void css_parser::reconsume()
    {
        if(m_tokenizer)
        {
            m_reconsume = true;
        } else
        {
            m_index--;
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-list-of-rules
    raw_rule::vector css_parser::consume_list_of_rules(bool top_level)
    {
        raw_rule::vector rules;
        raw_rule::ptr    rule;
        while(consume_rule(top_level, rule))
        {
            // If anything is returned, append it to the list of rules.
            if(rule)
            {
                rules.push_back(std::move(rule));
            }
        }
        // Return the list of rules.
        return rules;
    }

    bool css_parser::consume_rule(bool top_level, raw_rule::ptr& rule)
    {
        rule = nullptr;

        // Repeatedly consume the next input token:
        switch(next_token().type)
        {
        case WHITESPACE:
            return true; // Do nothing.

        case EOF:
            return false;

        case CDO:
        case CDC:
            // If the top-level flag is set, do nothing.
            if(top_level)
            {
                return true;
            }

            // Otherwise, reconsume the current input token. Consume a qualified rule.
            reconsume();
            rule = consume_qualified_rule();
            return true;

        case AT_KEYWORD:
            // Reconsume the current input token. Consume an at-rule.
            reconsume();
            rule = consume_at_rule();
            return true;

        default:
            // Reconsume the current input token. Consume a qualified rule.
            reconsume();
            rule = consume_qualified_rule();
            return true;
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-qualified-rule
    raw_rule::ptr css_parser::consume_qualified_rule()
    {
        // Create a new qualified rule with its prelude initially set to an empty list, and its value initially set to
        // nothing.
        raw_rule::ptr rule = std::make_shared<raw_rule>(raw_rule::qualified);

        while(true)
        {
            // Repeatedly consume the next input token:
            const css_token& token = next_token();

            switch(token.type)
            {
            case EOF:
                // This is a parse error. Return nothing.
                css_parse_error("eof in qualified rule");
                return nullptr;
            case '{':
                // Consume a simple block and assign it to the qualified rule’s block. Return the qualified rule.
                rule->block = consume_simple_block('{');
                return rule;
            case CURLY_BLOCK:
                // Assign the block to the qualified rule’s block. Return the qualified rule.
                rule->block = token;
                return rule;
            default:
                // Reconsume the current input token. Consume a component value. Append the returned value to the
                // qualified rule’s prelude.
                reconsume();
                rule->prelude.push_back(consume_component_value());
            }
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-at-rule
    raw_rule::ptr css_parser::consume_at_rule()
    {
        // Consume the next input token. Create a new at-rule with its name set to the value of the current input token,
        // its prelude initially set to an empty list, and its value initially set to nothing.
        raw_rule::ptr rule = std::make_shared<raw_rule>(raw_rule::at, next_token().str());

        while(true)
        {
            // Repeatedly consume the next input token:
            const css_token& token = next_token();

            switch(token.type)
            {
            case ';':
                return rule;
            case EOF:
                // This is a parse error. Return the at-rule.
                css_parse_error("eof in at-rule");
                return rule;
            case '{':
                // Consume a simple block and assign it to the at-rule’s block. Return the at-rule.
                rule->block = consume_simple_block('{');
                return rule;
            case CURLY_BLOCK:
                // Assign the block to the at-rule’s block. Return the at-rule.
                rule->block = token;
                return rule;
            default:
                // Reconsume the current input token. Consume a component value. Append the returned value to the
                // at-rule’s prelude.
                reconsume();
                rule->prelude.push_back(consume_component_value());
            }
        }
    }

    char mirror(char c);

    // https://www.w3.org/TR/css-syntax-3/#consume-simple-block
    css_token css_parser::consume_simple_block(char opening_bracket)
    {
        // Create a simple block with its associated token set to the current input token and with its value initially
        // set to an empty list.
        auto      block_type = css_token_type(-100 - opening_bracket); // see css_token_type
        css_token block(block_type);

        char closing_bracket = mirror(opening_bracket);

        while(true)
        {
            // Repeatedly consume the next input token and process it as follows:
            int type = next_token().type;

            if(type == closing_bracket || type == EOF)
            {
                if(type == EOF)
                {
                    css_parse_error("eof in simple block");
                }
                // drop the growth slack; the token copy constructor used to do this implicitly
                block.value.shrink_to_fit();
                return block;
            }
            // Reconsume the current input token. Consume a component value and append it to the value of the block.
            reconsume();
            block.value.push_back(consume_component_value());
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-component-value
    css_token css_parser::consume_component_value()
    {
        // Consume the next input token.
        const css_token& token = next_token();

        switch(token.type)
        {
            // If the current input token is a <{-token>, <[-token>, or <(-token>, consume a simple block and return it.
        case '{':
        case '[':
        case '(':
            return consume_simple_block(static_cast<char>(token.ch));

            // Otherwise, if the current input token is a <function-token>, consume a function and return it.
            // (consume_function copies the name before it reads the next token, which may overwrite token)
        case FUNCTION:
            return consume_function(token.name());

            // Otherwise, return the current input token.
        default:
            if(m_tokenizer)
            {
                // token is m_current; it is not reconsumed after this, so it can be moved out
                return std::move(m_current);
            }
            return token;
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-function
    css_token css_parser::consume_function(const std::string& name)
    {
        // Create a function with its name equal to the value of the current input token and with its value initially
        // set to an empty list.
        css_token function(CV_FUNCTION, name);

        while(true)
        {
            // Repeatedly consume the next input token and process it as follows:
            switch(next_token().type)
            {
            case ')':
                function.value.shrink_to_fit();
                return function;

            case EOF:
                css_parse_error("eof in function");
                function.value.shrink_to_fit();
                return function;

            default:
                // Reconsume the current input token. Consume a component value and append the returned value to the
                // function’s value.
                reconsume();
                function.value.push_back(consume_component_value());
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void trim_whitespace(css_token_vector& tokens)
    {
        while(at(tokens, 0).type == ' ')
        {
            remove(tokens, 0);
        }
        while(at(tokens, -1).type == ' ')
        {
            remove(tokens, -1);
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-declaration
    // next token is guaranteed to be IDENT
    raw_declaration css_parser::consume_declaration()
    {
        // Consume the next input token. Create a new declaration with its name set to the value of
        // the current input token and its value initially set to an empty list.
        raw_declaration decl  = {next_token().name(), {}};
        auto&           value = decl.value;

        // 1. While the next input token is a <whitespace-token>, consume the next input token.
        while(peek_token().type == ' ')
        {
            next_token();
        }

        // 2. If the next input token is anything other than a <colon-token>, this is a parse error. Return nothing.
        if(peek_token().ch != ':')
        {
            css_parse_error("consume_declaration: ':' not found");
            return {};
        }
        //    Otherwise, consume the next input token.
        next_token();

        // 3. While the next input token is a <whitespace-token>, consume the next input token.
        while(peek_token().type == ' ')
        {
            next_token();
        }

        // 4. As long as the next input token is anything other than an <EOF-token>,
        //    consume a component value and append it to the declaration’s value.
        while(peek_token().type != EOF)
        {
            value.push_back(consume_component_value());
        }

        // 5. If the last two non-<whitespace-token>s in the declaration’s value are a <delim-token> with the value "!"
        //    followed by an <ident-token> with a value that is an ASCII case-insensitive match for "important",
        //    remove them from the declaration’s value and set the declaration’s important flag to true.

        trim_whitespace(value); // deviation from standard: removing leading whitespace as well

        if(at(value, -1).ident() == "important" && at(value, -2).ch == '!')
        {
            remove(value, -2, 2);
            decl.important = true;
        }

        // 6. While the last token in the declaration’s value is a <whitespace-token>, remove that token.
        trim_whitespace(value);

        // 7. Return the declaration.
        return decl;
    }

    // https://www.w3.org/TR/css-syntax-3/#consume-style-block
    void css_parser::consume_style_block_contents(/*out*/ raw_declaration::vector& decls,
                                                  /*out*/ raw_rule::vector&        rules)
    {
        while(true)
        {
            // Repeatedly consume the next input token:
            const css_token& token = next_token();

            switch(token.type)
            {
            case WHITESPACE:
            case ';':
                break; // Do nothing.

            case EOF:
                // "Extend decls with rules, then return decls."
                // NOTE: I just return decls and rules separately
                return;

            case AT_KEYWORD:
                {
                    // Reconsume the current input token. Consume an at-rule, and append the result to rules.
                    reconsume();
                    auto rule = consume_at_rule();
                    if(rule)
                    {
                        rules.push_back(rule);
                    }
                    break;
                }
            case IDENT:
                {
                    // Initialize a temporary list initially filled with the current input token.
                    css_token_vector temp = {token};
                    // As long as the next input token is anything other than a <semicolon-token> or <EOF-token>,
                    // consume a component value and append it to the temporary list.
                    while(!is_one_of(peek_token().type, ';', EOF))
                    {
                        temp.push_back(consume_component_value());
                    }

                    css_parser parser(temp);
                    // Consume a declaration from the temporary list.
                    auto decl = parser.consume_declaration();
                    // If anything was returned, append it to decls.
                    if(decl)
                    {
                        decls.push_back(std::move(decl));
                    }
                    break;
                }
            case '&':
                {
                    // Reconsume the current input token. Consume a qualified rule. If anything was returned, append it
                    // to rules.
                    reconsume();
                    auto rule = consume_qualified_rule();
                    if(rule)
                    {
                        rules.push_back(rule);
                    }
                    break;
                }
            default:
                // This is a parse error. Reconsume the current input token. As long as the next input token is
                // anything other than a <;> or <EOF>, consume a component value and throw away the returned value.
                css_parse_error("unexpected token in a style block");
                reconsume();
                while(!is_one_of(peek_token().type, ';', EOF))
                {
                    consume_component_value();
                }
                break;
            }
        }
    }

    // https://www.w3.org/TR/css-syntax-3/#parse-comma-separated-list-of-component-values
    // Note: result is never empty. If input is empty result is {{}}.
    std::vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens)
    {
        std::vector<css_token_vector> result;

        css_token_vector list;
        for(auto& tok : tokens)
        {
            if(tok.type == ',') // Note: EOF token is not stored in arrays
            {
                result.push_back(list);
                list.clear();
                continue;
            }
            list.push_back(tok);
        }
        result.push_back(list);

        return result;
    }

    // https://drafts.csswg.org/css-syntax-3/#typedef-any-value
    // assumes that tokens have been componentized
    bool is_any_value(const css_token_vector& tokens)
    {
        if(tokens.empty())
        {
            return false;
        }
        for(auto& tok : tokens)
        {
            if(is_one_of(tok.type, BAD_STRING, BAD_URL, ')', ']', '}'))
            {
                return false;
            }
            if(tok.is_component_value() && !is_any_value(tok.value))
            {
                return false;
            }
        }
        return true;
    }

    // https://drafts.csswg.org/css-syntax-3/#typedef-declaration-value
    // assumes that tokens have been componentized
    bool is_declaration_value(const css_token_vector& tokens, int index)
    {
        if(index >= static_cast<int>(tokens.size()))
        {
            return false;
        }
        for(int i = index; i < static_cast<int>(tokens.size()); i++)
        {
            auto& tok = tokens[i];
            if(is_one_of(tok.type, BAD_STRING, BAD_URL, ')', ']', '}', ';', '!'))
            {
                return false;
            }
            // Note: ';' '!' inside component values are allowed, so using is_any_value here.
            if(tok.is_component_value() && !is_any_value(tok.value))
            {
                return false;
            }
        }
        return true;
    }

    // Note: it is possible to have several whitespace tokens in a row: "  /**/  /**/   "
    bool skip_whitespace(const css_token_vector& tokens, int& index)
    {
        int start = index;
        while(at(tokens, index).type == ' ')
        {
            index++;
        }
        return index != start;
    }

} // namespace litehtml
