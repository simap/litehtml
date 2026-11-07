#ifndef LITEHTML_EL_TEXT_H
#define LITEHTML_EL_TEXT_H

#include "element.h"
#include "document.h"

namespace litehtml
{
    class el_text : public element
    {
      protected:
        // Which string is measured and drawn (see shown_text).
        enum shown_kind : uint8_t
        {
            shown_original,    // m_text
            shown_space,       // " ": collapsible white space
            shown_tab,         // four spaces: a tab that is not collapsed
            shown_empty,       // "": a newline or carriage return that is not collapsed
            shown_transformed, // text-transform result, stored in the text arena after the NUL of m_text
        };

        // NUL-terminated text in the document's text_arena. Lives as long as the document.
        const char* m_text;
        // Width of the shown text. The height and draw_spaces come from css().get_font_metrics(), which
        // compute_styles copies from the parent.
        pixel_t m_width;
        uint8_t m_shown;

        // false for a break or a node without a font: zero content size
        bool has_size() const;

      public:
        el_text(const char* text, const document::ptr& doc);

        // The text as parsed.
        const char* text() const
        {
            return m_text;
        }
        // The string that is measured and drawn.
        const char* shown_text() const;

        void get_text(std::string& text) const override;
        void compute_styles(bool recursive) override;
        bool is_text() const override
        {
            return true;
        }

        void        draw(uint_ptr hdc, pixel_t x, pixel_t y, const position* clip,
                         const std::shared_ptr<render_item>& ri) override;
        std::string dump_get_name() override;

        std::vector<std::tuple<std::string, std::string>> dump_get_attrs() override;

      protected:
        void get_content_size(size& sz, pixel_t max_width) override;
    };
} // namespace litehtml

#endif // LITEHTML_EL_TEXT_H
