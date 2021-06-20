/**************************************************************************
A class for containing and working on bodies of text.

Copyright (C) 2021 Chris Morrison (gnosticist@protonmail.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef _TEXT_EXTRACTOR_H_
#define _TEXT_EXTRACTOR_H_

#include <cstdio>
#include <stack>
#include <cstring>
#include <sstream>
#include <iostream>
#include <codecvt>
#include <string>
#include <algorithm>
#include <filesystem>

#include "stringUtils.hpp"
#include "textCorpusItem.hpp"

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

namespace fsl::text
{
    class textCorpus
    {
    private:
        std::vector<textCorpusItem> _items;
        bool _splitSentences;
        bool _splitParagraphs;
        bool _removeHtmlTags;
    public:

        textCorpus()
        {
            _splitSentences = false;
            _splitParagraphs = false;
            _removeHtmlTags = true;
        }

        ~textCorpus() = default;

        bool empty() const noexcept
        {
            return _items.empty();
        }

        bool splitSentences() const
        {
            return _splitSentences;
        }

        void setSplitSentences(bool splitSentances)
        {
            _splitSentences = splitSentances;
        }

        bool splitParagraphs() const
        {
            return _splitParagraphs;
        }

        void setSplitParagraphs(bool splitParagraphs)
        {
            _splitParagraphs = splitParagraphs;
        }

        const std::vector<textCorpusItem>& parts() const
        {
            return _items;
        }

        void parseString(const std::string& input, bool append)
        {

        }

        void parseString(const std::wstring& input, bool append)
        {
            OutputDebugString(input.c_str());
            std::vector<std::wstring> paragraphs;
            std::vector<std::wstring> temp;
            if (append)
            {
                if (_splitParagraphs && !_items.empty() && !_items.back().empty()) _items.emplace_back();
            }
            else
            {
                _items.clear();
            }

            // The parsing of of a string will be carried out in X discrete phases.
            // ---------------------------------------------------------------------------------------------------------
            // Phase 1 - Obtain a copy of the string with all leading and trailing newlines and whitespaces removed.
            // ---------------------------------------------------------------------------------------------------------
            std::wstring trimmed = boost::algorithm::trim_copy_if(input, [](wchar_t wc){ if (wc == 0x000D) return true; return fsl::_private::_wspc_pred(wc); });

            // ---------------------------------------------------------------------------------------------------------
            // Phase 2 - Call _prep_string() which will:-
            //
            // - Replace all '\r' and unicode line breaks with '\n'
            // - Replace all unicode paragraph breaks with '\n\n'
            // - Replace all white space characters with ASCII 32.
            // - Ensure that there are no sequences of two or more consecutive white spaces (32) in the string.
            // - Ensure that there are no sequences of more that two consecutive line breaks (\n) in the string.
            // - Convert decorative unicode characters such as curly quotation marks to their ASCII equivalents.
            // ---------------------------------------------------------------------------------------------------------

            std::wstring copy;
            fsl::_private::_prep_string(trimmed, copy);

            // ---------------------------------------------------------------------------------------------------------
            // Phase 3 - If the caller has requested it, remove all the HTML/XML tabs. Replace paragraph ends
            // with '\n\n' and line breaks with '\n'
            // ---------------------------------------------------------------------------------------------------------
            if (_removeHtmlTags)
            {
                boost::ireplace_all(copy, L"</p>", L"\n\n");
                boost::ireplace_all(copy, L"<br>", "\n");
                boost::erase_all_regex(copy, boost::wregex(L"<[^<>]+>"));
            }

            // ---------------------------------------------------------------------------------------------------------
            // Phase 4 - Split the string into paragraphs.
            // ---------------------------------------------------------------------------------------------------------
            if (_splitSentences)
            {
                // Remove all single line breaks.
                boost::wregex wrx(L"[^\\n]\\n {1,}");
                boost::regex_replace(copy, wrx, L" ");
                wrx.assign(L" \\n{1}");
                boost::regex_replace(copy, wrx, L" ");

                boost::wregex p1(L"!(\\s{1})");
                boost::regex_replace(copy, p1, L"!\n");
                boost::wregex p2(L"\\?(\\s{1})");
                boost::regex_replace(copy, p2, L"!\n");
                boost::wregex p3(L"!\"(\\s{1})");
                boost::regex_replace(copy, p3, L"!\"\n");
                boost::wregex p4(L"\\?\"(\\s{1})");
                boost::regex_replace(copy, p4, L"?\"\n");
                boost::wregex p5(L"\\.\"(\\s{1})");
                boost::regex_replace(copy, p5, L".\"\n");
                boost::wregex p6(L"!'(\\s{1})");
                boost::regex_replace(copy, p6, L"!'\n");
                boost::wregex p7(L"\\?'(\\s{1})");
                boost::regex_replace(copy, p7, L"?'\n");
                boost::wregex p8(L"\\.'(\\s{1})");
                boost::regex_replace(copy, p8, L".'\n");
                boost::wregex p9(L"(?<term>([a-zA-Z]{2,}|\\d{3,})(\\)|\\}|\\]{0,1})\\.)( {1})");
                copy = boost::regex_replace(copy, p9, [](const boost::wsmatch& m)->std::wstring
                    {
                        return m["term"] + L"\n";
                    });
            }

            boost::split_regex(paragraphs, copy, boost::wregex(L"\\n{2,}"));
            for (auto& p : paragraphs)
            {
                copy = p;
                
                
                boost::split_regex(temp, copy, boost::wregex(L"\\n{1}"));
                for (const auto& s : temp)
                {
                    if (s.empty()) continue;
                    textCorpusItem itm(s, textCorpusItem::itemType::paragraph);
                    _items.push_back(std::move(itm));
                }

                // If paragraphs are to be split, then add an empty item to delimit them.
                if (_splitParagraphs) _items.emplace_back();
            }
        }

        bool removeHtmlTags() const
        {
            return _removeHtmlTags;
        }

        void setRemoveHtmlTags(bool removeHtmlTags)
        {
            _removeHtmlTags = removeHtmlTags;
        }
    };
}

#endif // _TEXT_EXTRACTOR_H_
