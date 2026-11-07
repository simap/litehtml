#ifndef LITEHTML_EL_TEXT_H
#define LITEHTML_EL_TEXT_H

#include "element.h"
#include "document.h"

namespace litehtml
{
    class el_text : public element
    {
      protected:
        std::string m_text;
        // text-transform result, allocated only when a transform applies
        std::unique_ptr<std::string> m_transformed_text;
        size                         m_size;
        // whitespace drawn instead of the text: 0 none, 1 " ", 2 "    " (tab), 3 "" (line break)
        uint8_t m_replacement = 0;
        bool    m_draw_spaces;

        // the text that is measured and drawn
        const char* display_text() const;

      public:
        el_text(const char* text, const document::ptr& doc);

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
