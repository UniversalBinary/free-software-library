/**************************************************************************
A class to convert the pages of a document into rasterised bitmaps.

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

#ifndef _DOCUMENT_FRACTIONATOR_HPP
#define _DOCUMENT_FRACTIONATOR_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <poppler/cpp/poppler-image.h>
#include <iostream>
#include <cmath>
#include <sstream>
#include <cstring>
#include <podofo/podofo.h>

#include "imageUtils.hpp"
#include "textCorpus.hpp"

using namespace fsl::_private;

#ifdef _MSC_VER
//#include <windows.h>
//#include <atlbase.h>
//#include <comdef.h>
//#include <comutil.h>
//#include <strsafe.h>
#ifdef DEBUG
#pragma comment(lib, "libpng16d.lib")
#pragma comment(lib, "jpegd.lib")
#pragma comment(lib, "tiffd.lib")
#pragma comment(lib, "tiffxxd.lib")
#pragma comment(lib, "poppler.lib")
#pragma comment(lib, "poppler-cpp.lib")
#pragma comment(lib, "podofo.lib")
#else
#pragma comment(lib, "libpng16.lib")
#pragma comment(lib, "jpeg62.lib")
#pragma comment(lib, "tiff.lib")
#pragma comment(lib, "tiffxx.lib")
#pragma comment(lib, "poppler.lib")
#pragma comment(lib, "poppler-cpp.lib")
#pragma comment(lib, "podofo.lib")
#endif
#endif

namespace fsl::text
{
	enum class imageFormat
	{
		png,
		tiff,
		jpeg,
	};

	enum class _documentType
	{
		_none,
		_pdf,
		_doc,
		_docx,
		_odt,
		_text
	};

	class documentFractionator
	{
	private:
		poppler::document* _pdfDoc;
		PoDoFo::PdfMemDocument _pdf;
		std::vector<uint8_t> _data;
		std::vector<textCorpus> _text;
		bool _valid;
		unsigned int _dpiX;
		unsigned int _dpiY;
		unsigned int _numberOfPages;
		unsigned int _currentPage;
		_documentType _docType;
	public:
		documentFractionator() = delete;

		explicit documentFractionator(unsigned int dpiX = 96, unsigned int dpiY = 96)
		{
			_numberOfPages = 0;
			_pdfDoc = nullptr;
			_valid = false;
			_dpiX = dpiX;
			_dpiY = dpiY;
			_currentPage = 1;
			_docType = _documentType::_none;
		}

		~documentFractionator()
		{

		}

		[[nodiscard]] bool valid() const
		{
			return _valid;
		}

		[[nodiscard]] int numberOfPages() const
		{
			return _numberOfPages;
		}

		[[nodiscard]] int currentPage() const
		{
			return _currentPage;
		}

		void setCurrentPage(unsigned int newPage)
		{
			if ((newPage == 0) || (newPage > _numberOfPages)) throw std::invalid_argument("newPage out of range.");
			_currentPage = newPage;
		}

		explicit operator bool() const
		{
			return _valid;
		}

		void loadPdfFile(const std::filesystem::path& pdfFile, const std::string& owner_password = std::string(), const std::string& user_password = std::string())
		{
			if (!std::filesystem::exists(pdfFile)) throw std::runtime_error("pdfFile does not exist.");

			_valid = false;
			_numberOfPages = 0;

			_pdfDoc = poppler::document::load_from_file(pdfFile.string(), owner_password, user_password);
			if (_pdfDoc)
			{
				_numberOfPages = _pdfDoc->pages();
				_currentPage = 1;
			}
			else
			{
				throw std::runtime_error("The given PDF file could not be opened, it may be damaged or invalid.");
			}

			if (!owner_password.empty()) _pdf.SetPassword(owner_password);
			if (!user_password.empty()) _pdf.SetPassword(user_password);
			_pdf.Load(pdfFile.wstring().c_str());

			_valid = true;
			_docType = _documentType::_pdf;
			_text.clear();
			_text.resize(_numberOfPages);
		}

		void loadWordFile(const std::filesystem::path& wordFile, const std::string& documentPassword = std::string(), const std::string& templatePassword = std::string(), const std::string& documentWritePassword = std::string(), const std::string& templateWritePassword = std::string())
		{
			if (!std::filesystem::exists(wordFile)) throw std::runtime_error("wordFile does not exist.");

			_valid = false;
			_numberOfPages = 0;


			_valid = true;
			_docType = _documentType::_pdf;
			_text.clear();
			_text.resize(_numberOfPages);
		}

		void loadOdtFile(const std::filesystem::path& odtFile, const std::string& documentPassword = std::string())
		{
			if (!std::filesystem::exists(odtFile)) throw std::runtime_error("odtFile does not exist.");

			_valid = false;
			_numberOfPages = 0;


			_valid = true;
			_docType = _documentType::_pdf;
			_text.clear();
			_text.resize(_numberOfPages);
		}

		const textCorpus& getText(bool splitSentences = true, bool splitParagraphs = true)
		{
			if ((_currentPage == 0) || (_currentPage > _numberOfPages)) throw std::invalid_argument("page out of range.");

			if (_valid && (_docType == _documentType::_pdf)) return _getPdfText(splitSentences, splitParagraphs);

			throw std::runtime_error("Invalid object state!");
		}

	private:
		std::wstring _adjustTextCursor(double old_tx, double old_ty, double tx, double ty, double tr, double sw)
		{
			std::wstringstream wss;
			// If the old coordinates are zero we have just entered the document and there is nothing to compare with.

			// Check if a space is required.
			if ((old_tx != 0) && (sw > 0))
			{
				auto diff = fabs(old_tx - tx);
				if (diff >= sw) wss << L' ';
			}
			
			// See if a newline is required.
			if ((old_ty != 0) && (tr == 0))
			{
				auto diff = fabs(old_ty - ty);
				if (diff > 5.00) wss << L'\n';
			}

			return wss.str();
		}

		const textCorpus& _getPdfText(bool splitSentences, bool splitParagraphs)
		{
			textCorpus& tcref = _text[_currentPage - 1];
			if (!tcref.empty()) return tcref;

			tcref.setSplitSentences(splitSentences);
			tcref.setSplitParagraphs(splitParagraphs);
			std::wstring rawstring;
			std::stack<PoDoFo::PdfVariant> stack;
			PoDoFo::PdfFont* pCurFont = nullptr;
			double old_tx = 0;
			double old_ty = 0;
			double tx = 0;
			double ty = 0;
			double textRise = 0;
			double spaceWidth = 0;

			PoDoFo::PdfPage* page = _pdf.GetPage(_currentPage - 1);
			PoDoFo::PdfContentsTokenizer tok(page);
			const char* token = nullptr;
			PoDoFo::PdfVariant var;
			PoDoFo::EPdfContentsType type;
			bool inTextObject = false;
			while (tok.ReadNext(type, token, var))
			{
				// BT*, ET*, Td*, TD*, Ts, T, Tm*, Tf*, ", ', Tj and TJ
				switch (type)
				{
				case PoDoFo::ePdfContentsType_Keyword:
					std::cout << token << "\n";
					if (!token) throw std::runtime_error("Error parsing PDF file!"); // Should not happen, but always check.
					if (std::strcmp(token, "BT") == 0) inTextObject = true;
					if (std::strcmp(token, "ET") == 0) inTextObject = false;
					if (std::strcmp(token, "Tc") == 0)
					{
						if (!inTextObject || (stack.size() != 1)) throw std::runtime_error("Error parsing PDF file!");
					}
					if (std::strcmp(token, "Tw") == 0)
					{
						if (!inTextObject || (stack.size() != 1)) throw std::runtime_error("Error parsing PDF file!");
					}
					if (std::strcmp(token, "Ts") == 0)
					{
						// Changes the text rise should not trigger a newline.
						if (!inTextObject || (stack.size() != 1)) throw std::runtime_error("Error parsing PDF file!");
						textRise = stack.top().GetReal();
					}
					if (std::strcmp(token, "Tf") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						stack.pop();
						PoDoFo::PdfName fontName = stack.top().GetName();
						PoDoFo::PdfObject* pFont = page->GetFromResources(PoDoFo::PdfName("Font"), fontName);
						pCurFont = _pdf.GetFont(pFont);
						if (pCurFont)
						{
							spaceWidth = pCurFont->GetFontMetrics()->GetWordSpace();
							if (spaceWidth == 0) spaceWidth = pCurFont->GetFontMetrics()->UnicodeCharWidth(' ');
							if (spaceWidth == 0) spaceWidth = pCurFont->GetFontMetrics()->UnicodeCharWidth('W'); // Last resort.
						}
					}
					if (std::strcmp(token, "TD") == 0)
					{
						if (!inTextObject || (stack.size() != 2)) throw std::runtime_error("Error parsing PDF file!");
						old_tx = tx;
						old_ty = ty;
						ty = stack.top().GetReal();
						stack.pop();
						tx = stack.top().GetReal();
						rawstring.append(_adjustTextCursor(old_tx, old_ty, tx, ty, textRise, spaceWidth));
					}
					if (std::strcmp(token, "Tm") == 0)
					{
						if (!inTextObject || (stack.size() != 6)) throw std::runtime_error("Error parsing PDF file!");
						old_tx = tx;
						old_ty = ty;
						ty = stack.top().GetReal();
						stack.pop();
						tx = stack.top().GetReal();
						rawstring.append(_adjustTextCursor(old_tx, old_ty, tx, ty, textRise, spaceWidth));
					}
					if (std::strcmp(token, "Td") == 0)
					{
						if (!inTextObject || (stack.size() != 2)) throw std::runtime_error("Error parsing PDF file!");
						old_tx = tx;
						old_ty = ty;
						ty = stack.top().GetReal();
						stack.pop();
						tx = stack.top().GetReal();
						rawstring.append(_adjustTextCursor(old_tx, old_ty, tx, ty, textRise, spaceWidth));
					}
					if (std::strcmp(token, "T*") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						rawstring.push_back('\n');
					}

					if (std::strcmp(token, "'") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						rawstring.push_back('\n');
						if (!stack.top().IsString()) throw std::runtime_error("Error parsing PDF file!");
						if (pCurFont)
						{
							PoDoFo::PdfString unicode = pCurFont->GetEncoding()->ConvertToUnicode(stack.top().GetString(), pCurFont);
							rawstring.append(unicode.GetStringW());
							tx = tx + pCurFont->GetFontMetrics()->StringWidth(unicode);
						}
						else
						{
							rawstring.append(stack.top().GetString().GetStringW());
						}
						//_cwprintf(L"Append: %c x = %f y = %f space = %f\n", rawstring.end(), tx, ty, spaceWidth);
					}
					if (std::strcmp(token, "\"") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						rawstring.push_back('\n');
						if (!stack.top().IsString()) throw std::runtime_error("Error parsing PDF file!");
						auto str = stack.top().GetString().GetStringW();
						if (pCurFont)
						{
							PoDoFo::PdfString unicode = pCurFont->GetEncoding()->ConvertToUnicode(stack.top().GetString(), pCurFont);
							rawstring.append(unicode.GetStringW());
							tx = tx + pCurFont->GetFontMetrics()->StringWidth(unicode);
						}
						else
						{
							rawstring.append(stack.top().GetString().GetStringW());
						}
						//_cwprintf(L"Append: %c x = %f y = %f space = %f\n", rawstring.end(), tx, ty, spaceWidth);
					}
					if (std::strcmp(token, "Tj") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						if (!stack.top().IsString()) throw std::runtime_error("Error parsing PDF file!");
						auto str = stack.top().GetString().GetStringW();
						if (pCurFont)
						{
							PoDoFo::PdfString unicode = pCurFont->GetEncoding()->ConvertToUnicode(stack.top().GetString(), pCurFont);
							rawstring.append(unicode.GetStringW());
							tx = tx + pCurFont->GetFontMetrics()->StringWidth(unicode);
						}
						else
						{
							rawstring.append(stack.top().GetString().GetStringW());
						}
						//_cwprintf(L"Append: %c x = %f y = %f space = %f\n", rawstring.end(), tx, ty, spaceWidth);
					}
					if (std::strcmp(token, "TJ") == 0)
					{
						if (!inTextObject) throw std::runtime_error("Error parsing PDF file!");
						// Get the array.
						if (!stack.top().IsArray()) throw std::runtime_error("Error parsing PDF file!");
						PoDoFo::PdfArray& a = stack.top().GetArray();
						for (size_t i = 0; i < a.GetSize(); ++i)
						{
							if (a[i].IsString() || a[i].IsHexString())
							{
								if (pCurFont)
								{
									PoDoFo::PdfString unicode = pCurFont->GetEncoding()->ConvertToUnicode(a[i].GetString(), pCurFont);
									rawstring.append(unicode.GetStringW());
									tx = tx + pCurFont->GetFontMetrics()->StringWidth(unicode);
								}
								else
								{
									rawstring.append(stack.top().GetString().GetStringW());
								}
							}
							//_cwprintf(L"Append: %c x = %f y = %f space = %f\n", rawstring.back(), tx, ty, spaceWidth);
						}
					}

					// Make sure the stack has been purged, even if we did not process the command.
					while (!stack.empty())
					{
						stack.pop();
					}
					break;
				case PoDoFo::ePdfContentsType_Variant:
					stack.push(var);
					break;
				default:
					throw std::runtime_error("Error parsing PDF file!");
					// Should not happen!
					break;
				}
			}

			tcref.parseString(rawstring, true);
			return tcref;
		}

	public:

		const std::vector<uint8_t>& renderPage(double dpi, imageFormat format, bool compress = true)
		{
			if ((_currentPage == 0) || (_currentPage > _numberOfPages)) throw std::invalid_argument("page out of range.");
			_data.clear();

			if (_valid && (_docType == _documentType::_pdf) && _pdfDoc)
			{
				auto pageRef = _pdfDoc->create_page(_currentPage - 1);
				if (pageRef)
				{
					poppler::page_renderer pageRenderer;
					pageRenderer.set_image_format(poppler::image::format_rgb24);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::antialiasing);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::text_antialiasing);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::text_hinting);
					auto data = pageRenderer.render_page(pageRef, dpi, dpi);
					if (data.is_valid())
					{
						if (format == imageFormat::png)
						{
							_make_png(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
						else if (format == imageFormat::tiff)
						{
							_make_tiff(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
						else if (format == imageFormat::jpeg)
						{
							_make_jpeg(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
					}
				}
				return _data;
			}

			throw std::runtime_error("Invalid object state!");
		}

		const std::vector<uint8_t>& renderPageFitted(unsigned int viewportWidth, unsigned int viewportHeight, imageFormat format, bool compress = true)
		{
			if ((_currentPage == 0) || (_currentPage > _numberOfPages)) throw std::invalid_argument("page out of range.");
			_data.clear();

			if ((_docType == _documentType::_pdf) && _pdfDoc)
			{
				auto pageRef = _pdfDoc->create_page(_currentPage - 1);
				if (pageRef)
				{
					poppler::page_renderer pageRenderer;
					pageRenderer.set_image_format(poppler::image::format_argb32);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::antialiasing);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::text_antialiasing);
					pageRenderer.set_render_hint(poppler::page_renderer::render_hint::text_hinting);
					// Fit image to given viewport.
					auto w = pageRef->page_rect(poppler::page_box_enum::media_box).width();
					auto h = pageRef->page_rect(poppler::page_box_enum::media_box).height();
					double widthInches = w / 72.00;
					double heightInches = h / 72.00;
					double dpi = 0;

					if (pageRef->orientation() == poppler::page::landscape)
					{
						dpi = static_cast<double>(viewportWidth) / widthInches;
					}
					else if (pageRef->orientation() == poppler::page::portrait)
					{
						dpi = static_cast<double>(viewportHeight) / heightInches;
					}
					else
					{
						return _data;
					}

					auto data = pageRenderer.render_page(pageRef, dpi, dpi);
					if (data.is_valid())
					{
						if (format == imageFormat::png)
						{
							_make_png(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
						else if (format == imageFormat::tiff)
						{
							_make_tiff(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
						else if (format == imageFormat::jpeg)
						{
							_make_jpeg(reinterpret_cast<const uint8_t*>(data.const_data()), _data, data.width(), data.height(), compress);
						}
					}
				}
				return _data;
			}
			
			throw std::runtime_error("Invalid object state!");
		}
	};
}

#endif //_DOCUMENT_FRACTIONATOR_HPP
