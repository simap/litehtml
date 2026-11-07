#ifndef LITEHTML_RENDER_ITEM_H
#define LITEHTML_RENDER_ITEM_H

#include <memory>
#include <list>
#include <tuple>
#include "html.h"
#include "pixel_type.h"
#include "types.h"
#include "line_box.h"
#include "table.h"
#include "formatting_context.h"
#include "element.h"
#include "scroll_view.h"
#include "render_item_list.h"

namespace litehtml
{
    class element;
    class render_item;

    // Members that few render items use. Allocated on first use. Text items and most inline items have all-zero
    // margins, padding and borders and never allocate it.
    struct render_item_extra
    {
        margins                                   margins_box;
        margins                                   padding_box;
        margins                                   borders_box;
        std::vector<std::shared_ptr<render_item>> positioned;
        std::shared_ptr<scroll_view>              scroll;
    };

    class render_item : public std::enable_shared_from_this<render_item>
    {
      protected:
        std::shared_ptr<element>                  m_element;
        // Not owning. The parent owns this item through its children list, so it outlives it while the item is in
        // the tree. Items dropped from the tree in init() are not used afterwards.
        render_item*                              m_parent = nullptr;
        render_item_list                          m_children;
        std::unique_ptr<render_item_extra>        m_extra;

        scroll_view* scroller() const
        {
            return m_extra ? m_extra->scroll.get() : nullptr;
        }
        const std::vector<std::shared_ptr<render_item>>& positioned() const
        {
            static const std::vector<std::shared_ptr<render_item>> empty;
            return m_extra ? m_extra->positioned : empty;
        }
        render_item_extra& extra()
        {
            if(!m_extra)
            {
                m_extra = std::make_unique<render_item_extra>();
            }
            return *m_extra;
        }
        static const margins& zero_margins()
        {
            static const margins zero{};
            return zero;
        }

      public:
        // Read access to margins, padding and borders; all zero when the extra block is absent
        const margins& box_margins() const
        {
            return m_extra ? m_extra->margins_box : zero_margins();
        }
        const margins& box_padding() const
        {
            return m_extra ? m_extra->padding_box : zero_margins();
        }
        const margins& box_borders() const
        {
            return m_extra ? m_extra->borders_box : zero_margins();
        }
        // Write access; allocates the extra block
        margins& edit_margins()
        {
            return extra().margins_box;
        }
        margins& edit_padding()
        {
            return extra().padding_box;
        }
        margins& edit_borders()
        {
            return extra().borders_box;
        }
        static bool is_zero(const margins& m)
        {
            // exact comparison: pixel_t == uses an epsilon
            return m.left.value() == 0.0f && m.right.value() == 0.0f && m.top.value() == 0.0f &&
                   m.bottom.value() == 0.0f;
        }
        // Store all three; allocates only when a value is non-zero or the block exists
        void set_box(const margins& mrg, const margins& pad, const margins& brd)
        {
            if(m_extra || !is_zero(mrg) || !is_zero(pad) || !is_zero(brd))
            {
                auto& ex       = extra();
                ex.margins_box = mrg;
                ex.padding_box = pad;
                ex.borders_box = brd;
            }
        }
        void set_margins(const margins& mrg)
        {
            if(m_extra || !is_zero(mrg))
            {
                extra().margins_box = mrg;
            }
        }

      protected:
        position m_pos;
        size     m_intrinsic_min_size;
        size     m_intrinsic_max_size;
        position m_bounds; // bounding box of the element
        bool     m_skip = false;

        containing_block_context calculate_containing_block_context(const containing_block_context& cb_context) const;
        void                     calc_cb_length(const css_length& len, pixel_t percent_base,
                                                containing_block_context::typed_pixel& out_value) const;
        virtual void             calc_intrinsic_size() = 0;
        virtual pixel_t _render(pixel_t /*x*/, pixel_t /*y*/, const containing_block_context& /*containing_block_size*/,
                                formatting_context* /*fmt_ctx*/, bool /*second_pass = false*/)
        {
            return 0_px;
        }

      public:
        explicit render_item(std::shared_ptr<element> src_el);

        virtual ~render_item() = default;

        pixel_t get_scroll_left() const
        {
            auto sv = scroller();
            return sv ? sv->get_left() : 0_px;
        }

        pixel_t get_scroll_top() const
        {
            auto sv = scroller();
            return sv ? sv->get_top() : 0_px;
        }

        void scroll_box(position& box) const
        {
            if(auto sv = scroller())
            {
                box.x -= sv->get_left();
                box.y -= sv->get_top();
            }
        }

        const size& get_intrinsic_max_size() const
        {
            return m_intrinsic_max_size;
        }

        const size& get_intrinsic_min_size() const
        {
            return m_intrinsic_min_size;
        }

        void update_intrinsic_size();

        pixel_t h_scroll(pixel_t dx)
        {
            auto sv = scroller();
            return sv ? sv->h_scroll(dx) : 0_px;
        }

        pixel_t v_scroll(pixel_t dy)
        {
            auto sv = scroller();
            return sv ? sv->v_scroll(dy) : 0_px;
        }

        bool is_h_scrollable(const pixel_t dx) const
        {
            auto sv = scroller();
            return sv ? sv->is_h_scrollable(dx) : false;
        }

        bool is_v_scrollable(const pixel_t dy) const
        {
            auto sv = scroller();
            return sv ? sv->is_v_scrollable(dy) : false;
        }

        render_item_list& children()
        {
            return m_children;
        }

        // Access to the m_pos
        position& pos()
        {
            return m_pos;
        }

        // Calculates the position of the element in the document
        // This position is relative to the x and y arguments
        // Scroll shifts are applied to the position
        position calc_placement(int x = 0, int y = 0) const
        {
            position pos  = m_pos;
            pos.x        += pixel_t(x) - get_scroll_left();
            pos.y        += pixel_t(y) - get_scroll_top();
            return pos;
        }

        bool skip() const
        {
            return m_skip;
        }

        void skip(bool val)
        {
            m_skip = val;
        }

        // Element right (including margins, padding, and borders)
        pixel_t right() const
        {
            return left() + width();
        }

        // Element right (excluding margins, but including padding and borders)
        pixel_t content_right() const
        {
            return content_left() + content_width();
        }

        // Element left (including margins, padding, and borders)
        pixel_t left() const
        {
            return m_pos.left() - box_margins().left - box_padding().left - box_borders().left;
        }

        // Element left (excluding margins, but including padding and borders)
        pixel_t content_left() const
        {
            return m_pos.left() - box_padding().left - box_borders().left;
        }

        // Element top (including margins, padding, and borders)
        pixel_t top() const
        {
            return m_pos.top() - box_margins().top - box_padding().top - box_borders().top;
        }

        // Element top (excluding margins, but including padding and borders)
        pixel_t content_top() const
        {
            return m_pos.top() - box_padding().top - box_borders().top;
        }

        // Element bottom (including margins, padding, and borders)
        pixel_t bottom() const
        {
            return top() + height();
        }

        // Element bottom (excluding margins, but including padding and borders)
        pixel_t content_bottom() const
        {
            return content_top() + content_height();
        }

        // Element height (including margins, padding, and borders)
        pixel_t height() const
        {
            return m_pos.height + box_margins().height() + box_padding().height() + box_borders().height();
        }

        // Element height (excluding margins, but including padding and borders)
        pixel_t content_height() const
        {
            return m_pos.height + box_padding().height() + box_borders().height();
        }

        // Element width (including margins, padding, and borders)
        pixel_t width() const
        {
            return m_pos.width + box_margins().width() + box_padding().width() + box_borders().width();
        }

        // Element width (excluding margins, but including padding and borders)
        pixel_t content_width() const
        {
            return m_pos.width + box_padding().width() + box_borders().width();
        }

        pixel_t padding_top() const
        {
            return box_padding().top;
        }

        pixel_t padding_bottom() const
        {
            return box_padding().bottom;
        }

        pixel_t padding_left() const
        {
            return box_padding().left;
        }

        pixel_t padding_right() const
        {
            return box_padding().right;
        }

        pixel_t border_top() const
        {
            return box_borders().top;
        }

        pixel_t border_bottom() const
        {
            return box_borders().bottom;
        }

        pixel_t border_left() const
        {
            return box_borders().left;
        }

        pixel_t border_right() const
        {
            return box_borders().right;
        }

        pixel_t margin_top() const
        {
            return box_margins().top;
        }

        pixel_t margin_bottom() const
        {
            return box_margins().bottom;
        }

        pixel_t margin_left() const
        {
            return box_margins().left;
        }

        pixel_t margin_right() const
        {
            return box_margins().right;
        }

        std::shared_ptr<render_item> parent() const
        {
            return m_parent ? m_parent->shared_from_this() : nullptr;
        }

        const margins& get_margins() const
        {
            return box_margins();
        }

        const margins& get_paddings() const
        {
            return box_padding();
        }

        void set_paddings(const margins& val)
        {
            if(m_extra || !is_zero(val))
            {
                edit_padding() = val;
            }
        }

        const margins& get_borders() const
        {
            return box_borders();
        }

        /**
         * Top offset to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_top() const
        {
            return box_margins().top + box_padding().top + box_borders().top;
        }

        /**
         * Bottom offset to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_bottom() const
        {
            return box_margins().bottom + box_padding().bottom + box_borders().bottom;
        }

        /**
         * Left offset to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_left() const
        {
            return box_margins().left + box_padding().left + box_borders().left;
        }

        /**
         * Right offset to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_right() const
        {
            return box_margins().right + box_padding().right + box_borders().right;
        }

        /**
         * Sum of left and right offsets to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_width() const
        {
            return content_offset_left() + content_offset_right();
        }

        /**
         * Sum of top and bottom offsets to the element content. Includes paddings, margins and borders.
         */
        pixel_t content_offset_height() const
        {
            return content_offset_top() + content_offset_bottom();
        }

        pixel_t render_offset_left() const
        {
            if(css().get_box_sizing() == box_sizing_content_box)
            {
                return box_margins().left + box_borders().left + box_padding().left;
            }
            return box_margins().left;
        }

        pixel_t render_offset_right() const
        {
            if(css().get_box_sizing() == box_sizing_content_box)
            {
                return box_margins().right + box_borders().right + box_padding().right;
            }
            return box_margins().right;
        }

        pixel_t render_offset_width() const
        {
            return render_offset_left() + render_offset_right();
        }

        pixel_t render_offset_top() const
        {
            if(css().get_box_sizing() == box_sizing_content_box)
            {
                return box_margins().top + box_borders().top + box_padding().top;
            }
            return box_margins().top;
        }

        pixel_t render_offset_bottom() const
        {
            if(css().get_box_sizing() == box_sizing_content_box)
            {
                return box_margins().bottom + box_borders().bottom + box_padding().bottom;
            }
            return box_margins().bottom;
        }

        pixel_t render_offset_height() const
        {
            return render_offset_top() + render_offset_bottom();
        }

        pixel_t box_sizing_left() const
        {
            if(css().get_box_sizing() == box_sizing_border_box)
            {
                return box_padding().left + box_borders().left;
            }
            return 0_px;
        }

        pixel_t box_sizing_right() const
        {
            if(css().get_box_sizing() == box_sizing_border_box)
            {
                return box_padding().right + box_borders().right;
            }
            return 0_px;
        }

        pixel_t box_sizing_width() const
        {
            return box_sizing_left() + box_sizing_right();
        }

        pixel_t box_sizing_top() const
        {
            if(css().get_box_sizing() == box_sizing_border_box)
            {
                return box_padding().top + box_borders().top;
            }
            return 0_px;
        }

        pixel_t box_sizing_bottom() const
        {
            if(css().get_box_sizing() == box_sizing_border_box)
            {
                return box_padding().bottom + box_borders().bottom;
            }
            return 0_px;
        }

        pixel_t box_sizing_height() const
        {
            return box_sizing_top() + box_sizing_bottom();
        }

        void parent(const std::shared_ptr<render_item>& par)
        {
            m_parent = par.get();
        }

        const std::shared_ptr<element>& src_el() const
        {
            return m_element;
        }

        const css_properties& css() const
        {
            return m_element->css();
        }

        void add_child(const std::shared_ptr<render_item>& ri)
        {
            m_children.push_back(ri);
            ri->parent(shared_from_this());
        }

        bool is_root() const
        {
            return m_parent == nullptr;
        }

        bool collapse_top_margin() const
        {
            return box_borders().top == 0_px && box_padding().top == 0_px && m_element->in_normal_flow() &&
                   m_element->css().get_float() == float_none && box_margins().top >= 0_px && !is_flex_item() &&
                   !is_root() && !is_one_of(css().get_overflow(), overflow_hidden, overflow_scroll, overflow_auto);
        }

        bool collapse_bottom_margin() const
        {
            return box_borders().bottom == 0_px && box_padding().bottom == 0_px && m_element->in_normal_flow() &&
                   m_element->css().get_float() == float_none && box_margins().bottom >= 0_px && !is_root() &&
                   !is_one_of(css().get_overflow(), overflow_hidden, overflow_scroll, overflow_auto);
        }

        bool is_visible() const
        {
            return !m_skip && src_el()->css().get_display() != display_none &&
                   src_el()->css().get_visibility() == visibility_visible;
        }

        bool is_flex_item() const
        {
            auto par = parent();
            return par && (par->css().get_display() == display_inline_flex || par->css().get_display() == display_flex);
        }

        pixel_t render(pixel_t x, pixel_t y, const containing_block_context& containing_block_size,
                       formatting_context* fmt_ctx, bool second_pass = false);
        void    apply_relative_shift(const containing_block_context& containing_block_size);
        // How far position: relative moves this element (0, 0 when it isn't relatively positioned)
        position relative_shift(const containing_block_context& containing_block_size) const;
        void     calc_outlines(pixel_t parent_width);
        pixel_t  calc_auto_margins(pixel_t parent_width); // returns left margin

        virtual std::shared_ptr<render_item> init();
        virtual void                         apply_vertical_align() {}
        /**
         * Get first baseline position. Default position is element bottom without bottom margin.
         * @returns offset of the first baseline from element top
         */
        virtual pixel_t get_first_baseline()
        {
            return height() - margin_bottom();
        }
        /**
         * Get the last baseline position.  The default position is element bottom without bottom margin.
         * @returns offset of the last baseline from element top
         */
        virtual pixel_t get_last_baseline()
        {
            return height() - margin_bottom();
        }

        virtual std::shared_ptr<render_item> clone() = 0;

        std::tuple<std::shared_ptr<litehtml::render_item>, std::shared_ptr<litehtml::render_item>,
                   std::shared_ptr<litehtml::render_item>>
             split_inlines();
        bool fetch_positioned();
        void render_positioned(render_type rt = render_all);
        // returns element offset related to the containing block
        std::tuple<pixel_t, pixel_t> element_static_offset(const std::shared_ptr<litehtml::render_item>& el);

        void add_positioned(const std::shared_ptr<litehtml::render_item>& el);
        // Calculate the bounding box of the element, including its children.
        void calc_bounds();
        void calc_document_size(litehtml::size& sz, pixel_t x = 0_px, pixel_t y = 0_px);

        /**
         * @brief Call func for all inline boxes
         * @param process function to call for boxes. "first "argument is true for the first box, "last" argument is
         * true for the last box. Function should return true to continue.
         * @return returns true if supported, else returns false
         */
        virtual bool for_inline_boxes(
            [[maybe_unused]] const std::function<bool(const position& box, bool first, bool last)>& process) const
        {
            return false;
        }

        virtual void set_inline_boxes(position::vector& /*boxes*/) {};
        virtual void add_inline_box(const position& /*box*/) {};
        virtual void clear_inline_boxes() {};
        void draw_stacking_context(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, bool with_positioned);
        virtual void    draw_children(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip, draw_flag flag,
                                      int zindex);
        virtual pixel_t get_draw_vertical_offset()
        {
            return 0_px;
        }
        virtual std::shared_ptr<element> get_child_by_point(
            pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y, draw_flag flag, int zindex,
            const std::function<bool(const std::shared_ptr<render_item>&)>& check);
        std::shared_ptr<element> get_element_by_point(
            pixel_t x, pixel_t y, pixel_t client_x, pixel_t client_y,
            const std::function<bool(const std::shared_ptr<render_item>&)>& check);
        virtual bool is_point_inside(pixel_t x, pixel_t y) const;
        void         dump(litehtml::dumper& cout);
        position     get_placement() const;
        virtual void y_shift(pixel_t delta);
        /**
         * Returns the boxes of rendering element. All coordinates are absolute
         *
         * @param redraw_box function to be called on every rendering box
         * @return
         */
        void get_rendering_boxes(const std::function<void(const position&)>& redraw_box) const;
    };
} // namespace litehtml

#endif // LITEHTML_RENDER_ITEM_H
