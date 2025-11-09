#pragma once

#include <hb.h>
#include <hb-ft.h>


struct ShapedGlyph {
    unsigned int glyph_index;
    float        x_advance, y_advance;
    float        x_offset, y_offset;
    unsigned int cluster;
};

class HbShaper {
  public:
      HbShaper(const std::string& fontPath, int pixelHeight) {
        if (FT_Init_FreeType(&ft_))
            throw std::runtime_error("Could not init FreeType library");
        if (FT_New_Face(ft_, fontPath.c_str(), 0, &face_)) 
            throw std::runtime_error("Could not open font: " + fontPath);

        // Set pixel         size; this is your raster scale.HarfBuzz uses this via hb_font.
        FT_Set_Pixel_Sizes(face_, 0, pixelHeight);

        hb_face_ = hb_ft_face_create_referenced(face_);
        hb_font_ = hb_ft_font_create_referenced(face_);

        // Enable scalable metrics in font space if you want subpixel positions:
        hb_ft_font_set_funcs(hb_font_);
    }

      ~HbShaper() {
        if (hb_font_)
            hb_font_destroy(hb_font_);
        if (hb_face_)
            hb_face_destroy(hb_face_);
        if (face_)
            FT_Done_Face(face_);
        if (ft_)
            FT_Done_FreeType(ft_);
      }

     std::vector<ShapedGlyph> shape_utf8(const std::string& textUtf8,
                                        const char*        lang   = "en",
                                        const char*        script = nullptr, // e.g., "Latn", "Arab"...
                                        hb_direction_t     dir    = HB_DIRECTION_LTR) {
        hb_buffer_t* buf = hb_buffer_create();
        hb_buffer_add_utf8(buf, textUtf8.c_str(), (int)textUtf8.size(), 0, (int)textUtf8.size());
        hb_buffer_set_direction(buf, dir);
        if (script)
            hb_buffer_set_script(buf, hb_script_from_string(script, -1));
        if (lang)
            hb_buffer_set_language(buf, hb_language_from_string(lang, -1));

        // Features: enable ligatures, kerning, etc. by default
        hb_feature_t features[] = {
            {HB_TAG('k', 'e', 'r', 'n'), 1, 0, (unsigned int)-1},
            {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1},
        };

        hb_shape(hb_font_, buf, features, sizeof(features) / sizeof(features[0]));

        unsigned int         count = 0;
        hb_glyph_info_t*     infos = hb_buffer_get_glyph_infos(buf, &count);
        hb_glyph_position_t* pos   = hb_buffer_get_glyph_positions(buf, &count);

        std::vector<ShapedGlyph> out;
        out.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            // HarfBuzz positions are in 26.6 fixed point or font units scaled; hb-ft maps to 26.6 pixel subunits.
            ShapedGlyph g;
            g.glyph_index = infos[i].codepoint;
            g.cluster     = infos[i].cluster;
            g.x_advance   = pos[i].x_advance / 64.0f;
            g.y_advance   = pos[i].y_advance / 64.0f;
            g.x_offset    = pos[i].x_offset / 64.0f;
            g.y_offset    = pos[i].y_offset / 64.0f;
            out.push_back(g);
        }

        hb_buffer_destroy(buf);
        return out;
    }

private:
    FT_Library ft_{};
    FT_Face    face_{};
    hb_face_t* hb_face_{};
    hb_font_t* hb_font_{};
};