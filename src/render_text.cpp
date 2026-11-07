#include "render_text.h"

litehtml::size litehtml::render_text::get_intrinsic_max_size() const
{
    litehtml::size sz;
    if(!src_el())
    {
        return sz;
    }
    src_el()->get_content_size(sz, 0_px);
    if(src_el()->css().line_height().computed_value > 0_px)
    {
        sz.height = src_el()->css().line_height().computed_value;
    }
    return sz;
}
