#include "html.h"
#include "stylesheet.h"
#include "css_parser.h"
#include "document.h"
#include "document_container.h"

namespace litehtml
{

    // ( <declaration> )  https://drafts.csswg.org/css-conditional-3/#typedef-supports-decl
    static bool eval_supports_declaration(const css_token_vector& tokens, document_container* container)
    {
        // a <supports-decl> holds exactly one declaration
        for(const auto& token : tokens)
        {
            if(token.ch == ';')
            {
                return false;
            }
        }
        // the declaration is supported if litehtml was able to parse it
        style st;
        st.add(tokens, "", container);
        return !st.empty();
    }

    // These return false when the syntax is invalid, which is not the same as a condition that is
    // false: an @supports rule with an invalid condition is invalid and its block is never applied.
    static bool parse_supports_condition(const css_token_vector& tokens, int& index, bool& result,
                                         document_container* container);

    // <supports-in-parens> = ( <supports-condition> ) | <supports-feature> | <general-enclosed>
    static bool parse_supports_in_parens(const css_token& token, bool& result, document_container* container)
    {
        // <general-enclosed> = <function-token> <any-value>? )
        // Any such function is unsupported, so it evaluates to false.
        if(token.type == CV_FUNCTION)
        {
            result = false;
            return true;
        }
        // everything but a block is not a <supports-in-parens> at all
        if(token.type != ROUND_BLOCK)
        {
            return false;
        }

        // ( <supports-condition> )
        int  index = 0;
        bool value = false;
        if(parse_supports_condition(token.value, index, value, container))
        {
            skip_whitespace(token.value, index);
            if(index == static_cast<int>(token.value.size()))
            {
                result = value;
                return true;
            }
        }
        // ( <declaration> ), or <general-enclosed> = ( <any-value>? ) which evaluates to false
        result = eval_supports_declaration(token.value, container);
        return true;
    }

    // https://drafts.csswg.org/css-conditional-3/#typedef-supports-condition
    // <supports-condition> = not <supports-in-parens>
    //                      | <supports-in-parens> [ and <supports-in-parens> ]*
    //                      | <supports-in-parens> [ or <supports-in-parens> ]*
    static bool parse_supports_condition(const css_token_vector& tokens, int& index, bool& result,
                                         document_container* container)
    {
        auto parse_operand = [&](bool& value) {
            skip_whitespace(tokens, index);
            if(!parse_supports_in_parens(at(tokens, index), value, container))
            {
                return false;
            }
            index++;
            return true;
        };
        auto next_ident = [&]() {
            skip_whitespace(tokens, index);
            return lowcase(at(tokens, index).ident());
        };

        if(next_ident() == "not")
        {
            index++;
            if(!parse_operand(result))
            {
                return false;
            }
            result = !result;
            return true;
        }

        if(!parse_operand(result))
        {
            return false;
        }

        std::string op = next_ident();
        if(op != "and" && op != "or")
        {
            return true; // a single <supports-in-parens>, the caller checks what follows it
        }
        // mixing `and` and `or` without parentheses is invalid, so the operator cannot change
        while(next_ident() == op)
        {
            index++;
            bool operand = false;
            if(!parse_operand(operand))
            {
                return false;
            }
            result = op == "and" ? result && operand : result || operand;
        }
        return true;
    }

    static bool eval_supports_condition(const css_token_vector& tokens, document_container* container)
    {
        int  index  = 0;
        bool result = false;
        if(!parse_supports_condition(tokens, index, result, container))
        {
            return false;
        }
        skip_whitespace(tokens, index);
        return index == static_cast<int>(tokens.size()) && result;
    }

    // https://www.w3.org/TR/css-syntax-3/#parse-a-css-stylesheet
    void css::parse_css_stylesheet(const std::string& input, const std::string& baseurl,
                                   const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media,
                                   bool top_level)
    {
        if(doc && media)
        {
            doc->add_media_list(media);
        }

        // To parse a CSS stylesheet, first parse a stylesheet (see css_parser::parse_stylesheet).
        // 1. Decode and 2. normalize. Tokens are pulled from the tokenizer as the rules are consumed.
        std::string        storage;
        const std::string& text = prepare_css_input(input, storage);
        css_tokenizer      tokenizer(text);
        css_parser         parser(tokenizer);
        parse_rules(parser, baseurl, doc, media, top_level);
    }

    void css::parse_css_stylesheet(const css_token_vector& input, const std::string& baseurl,
                                   const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media,
                                   bool top_level)
    {
        if(doc && media)
        {
            doc->add_media_list(media);
        }

        css_parser parser(input);
        parse_rules(parser, baseurl, doc, media, top_level);
    }

    void css::parse_rules(css_parser& parser, const std::string& baseurl, const std::shared_ptr<document>& doc,
                          const media_query_list_list::ptr& media, bool top_level)
    {
        bool import_allowed = top_level;

        // Interpret all of the resulting top-level qualified rules as style rules, defined below.
        // If any style rule is invalid, or any at-rule is not recognized or is invalid according
        // to its grammar or context, it's a parse error. Discard that rule.
        // Each rule is consumed (css_parser::consume_rule is one step of "consume a list of rules"),
        // interpreted, and dropped before the next one is consumed.
        // When the input is a token stream, the {} block of an at-rule is left in the stream (block_pending) so that
        // the contents of @media, @supports and @layer are parsed from the stream without being stored first.
        raw_rule::ptr rule;
        while(parser.consume_rule(top_level, rule, /*defer_blocks*/ true))
        {
            if(!rule)
            {
                continue;
            }
            if(rule->type == raw_rule::qualified)
            {
                if(parse_style_rule(rule, baseurl, doc, media))
                {
                    import_allowed = false;
                }
                continue;
            }

            bool pending   = parser.block_pending();
            bool has_block = pending || rule->block.type == CURLY_BLOCK;
            // <stylesheet> block contents, from the stream or from the block's component values
            auto parse_block = [&](const media_query_list_list::ptr& block_media) {
                if(pending)
                {
                    if(doc && block_media)
                    {
                        doc->add_media_list(block_media);
                    }
                    int saved = parser.begin_block();
                    parse_rules(parser, baseurl, doc, block_media, false);
                    parser.end_block(saved);
                } else
                {
                    parse_css_stylesheet(rule->block.value, baseurl, doc, block_media, false);
                }
            };

            // Otherwise: at-rule
            switch(_id(lowcase(rule->name)))
            {
            case _charset_: // ignored  https://www.w3.org/TR/css-syntax-3/#charset-rule
                break;

            case _import_:
                if(import_allowed)
                {
                    parse_import_rule(rule, baseurl, doc, media);
                } else
                {
                    css_parse_error("incorrect placement of @import rule");
                }
                break;

            // https://www.w3.org/TR/css-conditional-3/#at-media
            // @media <media-query-list> { <stylesheet> }
            case _media_:
                {
                    if(!has_block)
                    {
                        break;
                    }
                    auto new_media = media;
                    auto mq_list   = parse_media_query_list(rule->prelude, doc);
                    // An empty media query list evaluates to true.
                    // https://drafts.csswg.org/mediaqueries-5/#example-6f06ee45
                    if(!mq_list.empty())
                    {
                        new_media = std::make_shared<media_query_list_list>(media ? *media : media_query_list_list());
                        new_media->add(mq_list);
                    }
                    parse_block(new_media);
                    import_allowed = false;
                    break;
                }

            // https://drafts.csswg.org/css-conditional-3/#at-supports
            // @supports <supports-condition> { <stylesheet> }
            case _supports_:
                {
                    if(!has_block)
                    {
                        break;
                    }
                    auto condition = normalize(rule->prelude, f_componentize);
                    if(eval_supports_condition(condition, doc->container()))
                    {
                        parse_block(media);
                    }
                    import_allowed = false;
                    break;
                }

            // https://drafts.csswg.org/css-cascade-5/#at-layer
            // @layer <layer-name># ;
            // @layer <layer-name>? { <stylesheet> }
            case _layer_:
                {
                    // Cascade layers are not implemented: a layer statement only declares layer order, so it
                    // is ignored, and the rules of a layer block are used as if they were not layered.
                    if(has_block)
                    {
                        parse_block(media);
                        import_allowed = false;
                    }
                    // a layer statement rule is allowed before @import and does not disallow it
                    break;
                }

            default:
                css_parse_error("unrecognized rule @" + rule->name);
            }

            // a block that was not parsed above (unrecognized at-rule, false @supports condition, a block after
            // @import or @charset) is consumed and dropped
            if(parser.block_pending())
            {
                parser.skip_block();
            }
        }
    }

    // https://drafts.csswg.org/css-cascade-5/#at-import
    // `layer` and `supports` are not supported
    // @import [ <url> | <string> ] <media-query-list>?
    void css::parse_import_rule(const raw_rule::ptr& rule, const std::string& baseurl,
                                const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media)
    {
        auto tokens = rule->prelude;
        int  index  = 0;
        skip_whitespace(tokens, index);
        auto        tok = at(tokens, index);
        std::string url;
        auto        parse_string = [](const css_token& tok, std::string& str) {
            if(tok.type != STRING)
            {
                return false;
            }
            str = tok.str();
            return true;
        };
        bool ok = parse_url(tok, url) || parse_string(tok, url);
        if(!ok)
        {
            css_parse_error("invalid @import rule");
            return;
        }
        document_container* container = doc->container();
        std::string         css_text;
        std::string         css_baseurl = baseurl;
        container->import_css(css_text, url, css_baseurl);

        auto new_media = media;
        tokens         = slice(tokens, index + 1);
        auto mq_list   = parse_media_query_list(tokens, doc);
        if(!mq_list.empty())
        {
            new_media = std::make_shared<media_query_list_list>(media ? *media : media_query_list_list());
            new_media->add(mq_list);
        }

        parse_css_stylesheet(css_text, css_baseurl, doc, new_media, true);
    }

    // https://www.w3.org/TR/css-syntax-3/#style-rules
    bool css::parse_style_rule(const raw_rule::ptr& rule, const std::string& baseurl,
                               const std::shared_ptr<document>& doc, const media_query_list_list::ptr& media)
    {
        // The prelude of the qualified rule is parsed as a <selector-list>. If this returns failure, the entire style
        // rule is invalid.
        auto list = parse_selector_list(rule->prelude, strict_mode, doc->mode());
        if(list.empty())
        {
            css_parse_error("invalid selector");
            return false;
        }

        style::ptr style = std::make_shared<litehtml::style>(); // style block
        // The content of the qualified rule's block is parsed as a style block's contents.
        style->add(rule->block.value, baseurl, doc->container());

        for(const auto& sel : list)
        {
            sel->m_style       = style;
            sel->m_media_query = media;
            sel->calc_specificity();
            add_selector(sel);
        }
        return true;
    }

    void css::sort_selectors()
    {
        std::sort(m_selectors.begin(), m_selectors.end(),
                  [](const css_selector::ptr& v1, const css_selector::ptr& v2) { return (*v1) < (*v2); });

        m_index.clear();
        m_index.reserve(m_selectors.size());
        for(int i = 0; i < static_cast<int>(m_selectors.size()); i++)
        {
            const css_element_selector& r   = m_selectors[i]->m_right;
            uint32_t                    key = index_key(3, empty_id);
            const auto id = std::find_if(r.m_attrs.begin(), r.m_attrs.end(), [](const css_attribute_selector& a) {
                return a.type == select_id;
            });
            const auto cls = std::find_if(r.m_attrs.begin(), r.m_attrs.end(), [](const css_attribute_selector& a) {
                return a.type == select_class;
            });
            if(id != r.m_attrs.end())
            {
                key = index_key(0, id->name);
            } else if(cls != r.m_attrs.end())
            {
                key = index_key(1, cls->name);
            } else if(r.m_tag != star_id)
            {
                key = index_key(2, r.m_tag);
            }
            m_index.push_back({key, i});
        }
        std::sort(m_index.begin(), m_index.end());
        m_indexed_count = m_selectors.size();
    }

    bool css::candidates(string_id id, const std::vector<string_id>& classes, string_id tag,
                         std::vector<int>& out) const
    {
        if(m_indexed_count != m_selectors.size())
        {
            return false;
        }
        auto add = [&](uint32_t key) {
            auto it = std::lower_bound(m_index.begin(), m_index.end(), index_entry{key, -1});
            for(; it != m_index.end() && it->key == key; ++it)
            {
                out.push_back(it->pos);
            }
        };
        if(id != empty_id)
        {
            add(index_key(0, id));
        }
        for(string_id cls : classes)
        {
            add(index_key(1, cls));
        }
        add(index_key(2, tag));
        add(index_key(3, empty_id));
        // the lists are each ascending; merge them back into stylesheet order (a repeated class adds a
        // list twice)
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return true;
    }

} // namespace litehtml
