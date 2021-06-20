/**************************************************************************
Various utilities for working with images.

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

#include <tiffio.h>
#include <tiffio.hxx>
#include <sstream>
#include <cstring>
#include <png.h>
#include <jpeglib.h>

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
#else
#pragma comment(lib, "libpng16.lib")
#pragma comment(lib, "jpeg62.lib")
#pragma comment(lib, "tiff.lib")
#pragma comment(lib, "tiffxx.lib")
#endif
#endif

namespace fsl::_private
{
    inline void _pngWriteCallback(png_structp  png_ptr, png_bytep data, png_size_t length)
    {
        std::vector<uint8_t>* p = (std::vector<uint8_t>*)png_get_io_ptr(png_ptr);
        p->insert(p->end(), data, data + length);
    }

    struct _pngDestructor
    {
        png_struct* _p;
        _pngDestructor(png_struct* p)
        {
            _p = p;
        }
        ~_pngDestructor()
        {
            if (_p) png_destroy_write_struct(&_p, nullptr);
        }
    };

    struct _deallocator
    {
        void* _ptr;

        _deallocator(void* ptr)
        {
            _ptr = ptr;
        }
        ~_deallocator()
        {
            if (_ptr) std::free(_ptr);
        }
    };

    inline std::vector<uint8_t>& _make_jpeg(const uint8_t* input, std::vector<uint8_t>& output, unsigned int width, unsigned int height, bool compress = true)
    {
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        JSAMPROW row_pointer[1];      // Pointer to JSAMPLE row(s).
        int row_stride;               // Physical row width in image buffer.
        unsigned long jpegBufSize = 0;
        uint8_t* jpegBuf = nullptr;  // Get the library to allocate a buffer.

        // Step 1: allocate and initialize JPEG compression object.
        cinfo.err = jpeg_std_error(&jerr);
        // Now we can initialize the JPEG compression object.
        jpeg_create_compress(&cinfo);

        // Step 2: specify data destination.
        jpeg_mem_dest(&cinfo, &jpegBuf, &jpegBufSize);

        // Step 3: set parameters for compression.
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 4;
        cinfo.in_color_space = JCS_EXT_RGBA;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, 100, TRUE);

        // Step 4: Start compressor.
        jpeg_start_compress(&cinfo, TRUE);

        // Step 5: Write scanlines.
        row_stride = width * 4;

        while (cinfo.next_scanline < cinfo.image_height)
        {
            // TODO: More scanlines per pass.
            row_pointer[0] = const_cast<unsigned char*>(&input[cinfo.next_scanline * row_stride]);
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        // Step 6: Finish compression.
        jpeg_finish_compress(&cinfo);

        // Step 7: release JPEG compression object.
        jpeg_destroy_compress(&cinfo);

        // Copy buffer out.
        output.clear();
        output.assign(jpegBuf, jpegBuf + jpegBufSize);
        free(jpegBuf);

        return output;
    }

    inline std::vector<uint8_t>& _make_png(const uint8_t* input, std::vector<uint8_t>& output, unsigned int width, unsigned int height, bool compress = true)
    {
        png_structp png_ptr = nullptr;
        png_infop info_ptr = nullptr;
        png_bytep row = nullptr;
        output.clear();

        // Initialize write structure
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (png_ptr == nullptr) return output;
        _pngDestructor destroyPng(png_ptr);

        // Initialize info structure
        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == nullptr) return output;

        // Set up the header.
        setjmp(png_jmpbuf(png_ptr));
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        if (compress)
        {
            png_set_compression_level(png_ptr, 9);
        }
        else
        {
            png_set_compression_level(png_ptr, 0);
        }

        std::vector<uint8_t*> rows(height);
        for (size_t y = 0; y < height; ++y)
        {
            rows[y] = (uint8_t*)input + y * width * 4;
        }
        png_set_rows(png_ptr, info_ptr, &rows[0]);
        png_set_write_fn(png_ptr, &output, _pngWriteCallback, nullptr);
        png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

        return output;
    }

    inline std::vector<uint8_t>& _make_tiff(const uint8_t* input, std::vector<uint8_t>& output, unsigned int width, unsigned int height, bool compress = true)
    {
        std::ostringstream output_TIFF_stream;
        TIFF* out = TIFFStreamOpen("MemTIFF", &output_TIFF_stream);
        TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        uint16 es[1] = { EXTRASAMPLE_ASSOCALPHA };
        TIFFSetField(out, TIFFTAG_EXTRASAMPLES, 1, &es);
        if (compress) TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);

        tsize_t bytes_per_line = width * 4; // Length in memory of one row of pixel in the image.
        unsigned char* buf = nullptr;  // Buffer used to store the row of pixel information for writing to file
        // Allocate memory to store the pixels of current row
        if (TIFFScanlineSize(out) == bytes_per_line)
        {
            buf = static_cast<unsigned char*>(_TIFFmalloc(bytes_per_line));
        }
        else
        {
            buf = static_cast<unsigned char*>(_TIFFmalloc(TIFFScanlineSize(out)));
        }

        // We set the strip size of the file to be size of one row of pixels
        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, width * 4));

        //Now writing image to the file one strip at a time
        output.clear();
        bool failed = false;
        for (uint32 row = 0; row < height; row++)
        {
            std::memcpy(buf, &input[row * bytes_per_line], bytes_per_line);
            //std::memcpy(buf, &input[(height - row - 1) * bytes_per_line], bytes_per_line);
            if (TIFFWriteScanline(out, buf, row, 0) < 0)
            {
                failed = true;
                break;;
            }
        }

        TIFFClose(out);
        if (buf) _TIFFfree(buf);

        if (!failed)
        {
            const std::string& str = output_TIFF_stream.str();
            output.insert(output.end(), str.begin(), str.end());
        }

        return output;
    }
}
