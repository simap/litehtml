#include "html.h"
#include "el_text.h"
#include "render_item.h"
#include "document_container.h"

litehtml::el_text::el_text(const char* text, const document::ptr& doc) :
    element(doc)
{
    if(!text)
    {
        text = "";
    }
    m_text        = doc->texts().add(text, strlen(text));
    m_width = 0;
    m_shown = shown_original;
    // the default style with display inline_text, interned once per document
    m_css = doc->text_style();
}

litehtml::el_text::~el_text()
{
    // While the document lives, give back a long text that has a block of its own. When the document is being
    // destroyed, get_document() is null and the arena goes with it.
    if(document::ptr doc = get_document())
    {
        doc->texts().release(m_text);
    }
}

const char* litehtml::el_text::shown_text() const
{
    switch(m_shown)
    {
    case shown_space:
        return " ";
    case shown_tab:
        return "    ";
    case shown_empty:
        return "";
    case shown_transformed:
        return m_text + strlen(m_text) + 1;
    default:
        return m_text;
    }
}

void litehtml::el_text::get_content_size(size& sz, pixel_t /*max_width*/)
{
    sz.width  = m_width;
    sz.height = has_size() ? css().get_font_metrics().height : pixel_t(0);
}

void litehtml::el_text::get_text(std::string& text) const
{
    text += m_text;
}

void litehtml::el_text::compute_styles(bool /*recursive*/)
{
    // The computed style depends only on the node's own style, the parent's style, the nearest relative
    // ancestor in the inline chain and whether that chain ends at a non-inline ancestor. All are interned, so
    // most text nodes reuse the result of a sibling or cousin through the document's memo (98 percent on
    // Wikipedia) instead of building and interning a new copy.
    element::ptr el_parent = parent();
    element::ptr p         = el_parent;
    element::ptr relative;
    while(p && p->css().get_display() == display_inline)
    {
        if(p->css().get_position() == element_position_relative)
        {
            relative = p;
            break;
        }
        p = p->parent();
    }
    document::ptr doc = get_document();
    if(doc)
    {
        // the last slot only marks that the chain ends at an ancestor; any non-null pointer would do
        document::text_style_key key = {m_css, el_parent ? &el_parent->css() : nullptr,
                                        relative ? &relative->css() : nullptr, p ? doc->default_style() : nullptr};
        auto                     it  = doc->m_text_style_memo.find(key);
        if(it != doc->m_text_style_memo.end())
        {
            m_css = it->second;
        } else
        {
            // compute into a scratch copy, then share the interned instance
            css_properties c = *m_css;
            if(el_parent)
            {
                c.line_height() = el_parent->css().line_height();
                c.set_font(el_parent->css().get_font());
                c.set_font_metrics(el_parent->css().get_font_metrics());
                c.set_white_space(el_parent->css().get_white_space());
                c.set_text_transform(el_parent->css().get_text_transform());
            }
            c.set_display(display_inline_text);
            c.set_float(float_none);
            if(relative)
            {
                c.set_offsets(relative->css().get_offsets());
                c.set_position(element_position_relative);
            }
            if(p)
            {
                c.set_position(element_position_static);
            }
            set_css(c);
            doc->m_text_style_memo.emplace(key, m_css);
        }
    }
    const css_properties& c = css();

    // The transformed text is stored only when it is shown and differs from the text.
    if(is_white_space())
    {
        m_shown = shown_space;
    } else if(!strcmp(m_text, "\t"))
    {
        m_shown = shown_tab;
    } else if(!strcmp(m_text, "\n") || !strcmp(m_text, "\r"))
    {
        m_shown = shown_empty;
    } else if(c.get_text_transform() != text_transform_none)
    {
        std::string tt = m_text;
        get_document()->container()->transform_text(tt, c.get_text_transform());
        if(tt == m_text)
        {
            m_shown = shown_original;
        } else if(m_shown != shown_transformed || tt != shown_text())
        {
            const char* old = m_text;
            m_text          = get_document()->texts().add_pair(m_text, strlen(m_text), tt.c_str(), tt.length());
            m_shown         = shown_transformed;
            get_document()->texts().release(old);
        }
    } else
    {
        m_shown = shown_original;
    }

    // css() now holds the parent's font and font metrics; get_content_size and draw read the height and
    // draw_spaces from there
    if(has_size())
    {
        m_width = get_document()->container()->text_width(shown_text(), css().get_font());
    } else
    {
        m_width = 0;
    }
}

bool litehtml::el_text::has_size() const
{
    return !is_break() && css().get_font();
}

void litehtml::el_text::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                             const std::shared_ptr<render_item>& ri)
{
    if(is_white_space() && !css().get_font_metrics().draw_spaces)
    {
        return;
    }

    position pos  = ri->pos();
    pos.x        += x;
    pos.y        += y;
    pos.round();

    if(pos.does_intersect(clip))
    {
        element::ptr el_parent = parent();
        if(el_parent)
        {
            document::ptr doc = get_document();

            uint_ptr font = el_parent->css().get_font();
            if(font)
            {
                web_color color = el_parent->css().get_color();
                doc->container()->draw_text(hdc, shown_text(), font, color, pos);
            }
        }
    }
}

std::string litehtml::el_text::dump_get_name()
{
    return "text: \"" + get_escaped_string(m_text) + "\"";
}

std::vector<std::tuple<std::string, std::string>> litehtml::el_text::dump_get_attrs()
{
    return {};
}
