#include "html.h"
#include "el_text.h"
#include "render_item.h"
#include "document_container.h"

litehtml::el_text::el_text(const char* text, const document::ptr& doc) :
    element(doc)
{
    if(text)
    {
        m_text = text;
    }
    m_draw_spaces = true;
    modify_css([](css_properties& c) { c.set_display(display_inline_text); });
}

const char* litehtml::el_text::display_text() const
{
    switch(m_replacement)
    {
    case 1:
        return " ";
    case 2:
        return "    ";
    case 3:
        return "";
    default:
        return m_transformed_text ? m_transformed_text->c_str() : m_text.c_str();
    }
}

void litehtml::el_text::get_content_size(size& sz, pixel_t /*max_width*/)
{
    sz = m_size;
}

void litehtml::el_text::get_text(std::string& text) const
{
    text += m_text;
}

void litehtml::el_text::compute_styles(bool /*recursive*/)
{
    // compute into a scratch copy, then share the interned instance
    css_properties c       = *m_css;
    element::ptr el_parent = parent();
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

    if(c.get_text_transform() != text_transform_none)
    {
        m_transformed_text = std::make_unique<std::string>(m_text);
        get_document()->container()->transform_text(*m_transformed_text, c.get_text_transform());
    } else
    {
        m_transformed_text.reset();
    }

    element::ptr p = parent();
    while(p && p->css().get_display() == display_inline)
    {
        if(p->css().get_position() == element_position_relative)
        {
            c.set_offsets(p->css().get_offsets());
            c.set_position(element_position_relative);
            break;
        }
        p = p->parent();
    }
    if(p)
    {
        c.set_position(element_position_static);
    }
    set_css(c);

    if(is_white_space())
    {
        m_replacement = 1;
    } else if(m_text == "\t")
    {
        m_replacement = 2;
    } else if(m_text == "\n" || m_text == "\r")
    {
        m_replacement = 3;
    }

    font_metrics fm;
    uint_ptr     font = 0;
    if(el_parent)
    {
        font = el_parent->css().get_font();
        fm   = el_parent->css().get_font_metrics();
    }
    if(is_break() || !font)
    {
        m_size.height = 0;
        m_size.width  = 0;
    } else
    {
        m_size.height = fm.height;
        m_size.width  = get_document()->container()->text_width(
            display_text(), font);
    }
    m_draw_spaces = fm.draw_spaces;
}

void litehtml::el_text::draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                             const std::shared_ptr<render_item>& ri)
{
    if(is_white_space() && !m_draw_spaces)
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
                doc->container()->draw_text(hdc, display_text(), font,
                                            color, pos);
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
