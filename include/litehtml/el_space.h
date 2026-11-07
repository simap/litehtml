#ifndef LITEHTML_EL_SPACE_H
#define LITEHTML_EL_SPACE_H

#include "el_text.h"

namespace litehtml
{
    class el_space : public el_text
    {
      public:
        el_space(const char* text, const std::shared_ptr<document>& doc);

        bool is_white_space() const override;
        bool is_break() const override;
        bool is_space() const override;

        std::string dump_get_name() override;
    };

    // Whitespace-only text is parsed as one el_space per run (document::create_node). Where ws preserves
    // white space (pre, pre-wrap, pre-line), replace each run in list by one el_space per character, as
    // upstream parsed it, so newlines break lines (is_break) and tabs and spaces stay apart. parent, if
    // not null, becomes the parent of the new nodes (a raw pointer because html_tag can compute its
    // style inside its constructor, where shared_from_this is not yet available; it has no children then).
    void split_space_runs(elements_list& list, white_space ws, const std::shared_ptr<document>& doc,
                          element* parent);
} // namespace litehtml

#endif // LITEHTML_EL_SPACE_H
