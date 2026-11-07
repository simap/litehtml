#include "html.h"
#include "document.h"
#include "el_space.h"

litehtml::el_space::el_space(const char* text, const std::shared_ptr<document>& doc) :
    el_text(text, doc)
{
}

bool litehtml::el_space::is_white_space() const
{
    white_space ws = css().get_white_space();
    return ws == white_space_normal || ws == white_space_nowrap || ws == white_space_pre_line;
}

bool litehtml::el_space::is_break() const
{
    white_space ws = css().get_white_space();
    if(ws == white_space_pre || ws == white_space_pre_line || ws == white_space_pre_wrap)
    {
        if(m_text == "\n")
        {
            return true;
        }
    }
    return false;
}

bool litehtml::el_space::is_space() const
{
    return true;
}

void litehtml::split_space_runs(elements_list& list, white_space ws, const std::shared_ptr<document>& doc,
                                element* parent)
{
    if(ws != white_space_pre && ws != white_space_pre_wrap && ws != white_space_pre_line)
    {
        return;
    }
    std::string text;
    for(auto it = list.begin(); it != list.end();)
    {
        if(!(*it)->is_space())
        {
            ++it;
            continue;
        }
        text.clear();
        (*it)->get_text(text);
        if(text.length() < 2)
        {
            ++it;
            continue;
        }
        for(char ch : text)
        {
            const char c[2] = {ch, 0};
            auto       sp   = std::make_shared<el_space>(c, doc);
            if(parent)
            {
                sp->parent(parent->shared_from_this());
            }
            list.insert(it, sp);
        }
        it = list.erase(it);
    }
}

std::string litehtml::el_space::dump_get_name()
{
    return "space: \"" + get_escaped_string(m_text) + "\"";
}
