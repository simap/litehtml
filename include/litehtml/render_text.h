#ifndef LITEHTML_RENDER_TEXT_H
#define LITEHTML_RENDER_TEXT_H

#include "render_inline.h"

namespace litehtml
{
    // One word or whitespace run. Derives from render_item directly: a text item never has inline boxes, so it does
    // not carry render_item_inline::m_boxes. The overrides keep the behavior it had as a render_item_inline.
    // It does not store intrinsic sizes or a bounding box (render_item_box does): both are functions of the element
    // and the position, since a text item has no children and zero margins, padding and borders.
    class render_text : public render_item
    {
      protected:
        void calc_intrinsic_size() override {}

      public:
        explicit render_text(std::shared_ptr<element> src_el) :
            render_item(std::move(src_el))
        {
        }

        // min and max are equal: the text size, with the line height as height when it is positive
        size get_intrinsic_max_size() const override;
        size get_intrinsic_min_size() const override
        {
            return get_intrinsic_max_size();
        }
        void update_intrinsic_size() override {}
        // what calc_document_size and calc_bounds store for an item without children or inline boxes
        position get_bounds() const override
        {
            return {content_left(), content_top(), content_width(), content_height()};
        }
        void set_bounds(const position& /*bounds*/) override {}

        bool for_inline_boxes([[maybe_unused]] const std::function<bool(const position& box, bool first, bool last)>&
                                  process) const override
        {
            return false;
        }
        pixel_t get_first_baseline() override
        {
            return src_el()->css().get_font_metrics().height - src_el()->css().get_font_metrics().base_line();
        }
        pixel_t get_last_baseline() override
        {
            return src_el()->css().get_font_metrics().height - src_el()->css().get_font_metrics().base_line();
        }
        std::shared_ptr<render_item> clone() override
        {
            return std::make_shared<render_item_inline>(src_el());
        }
        void y_shift(pixel_t shift) override
        {
            if(css().get_display() == display_inline_text)
            {
                render_item::y_shift(shift);
            }
        }
        bool is_point_inside(pixel_t /*x*/, pixel_t /*y*/) const override
        {
            // as render_item_inline with no boxes
            return false;
        }
    };
} // namespace litehtml

#endif // LITEHTML_RENDER_TEXT_H
