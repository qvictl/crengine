// CoolReader font lifecycle regression tests.
// Distributed under the GNU General Public License, like the font manager.
#include "lvfntman.h"

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// Observe actual FreeType destruction, including FT_Done_FreeType cleanup.
// No production counters or font-manager internals are needed by this test.
static int live_faces;
static int created_faces;

static void face_finalized(void *) { --live_faces; }

static void track_face(FT_Face face) {
    face->generic.finalizer = face_finalized;
    ++live_faces;
    ++created_faces;
}

extern "C" FT_Error FT_New_Face(FT_Library library, const char *path,
                               FT_Long index, FT_Face *face) {
    using NewFace = FT_Error (*)(FT_Library, const char *, FT_Long, FT_Face *);
    static NewFace original = reinterpret_cast<NewFace>(dlsym(RTLD_NEXT, "FT_New_Face"));
    FT_Error result = original(library, path, index, face);
    if (!result) track_face(*face);
    return result;
}

extern "C" FT_Error FT_New_Memory_Face(FT_Library library, const FT_Byte *data,
                                      FT_Long length, FT_Long index, FT_Face *face) {
    using NewFace = FT_Error (*)(FT_Library, const FT_Byte *, FT_Long, FT_Long, FT_Face *);
    static NewFace original = reinterpret_cast<NewFace>(dlsym(RTLD_NEXT, "FT_New_Memory_Face"));
    FT_Error result = original(library, data, length, index, face);
    if (!result) track_face(*face);
    return result;
}

static void check(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "font_gc_test: %s\n", message);
        std::exit(1);
    }
}

int main(int argc, char **argv) {
    check(argc == 2, "provide a TrueType font fixture path");
    check(InitFontManager(lString8()), "font manager initialization failed");
    check(fontMan->RegisterFont(lString8(argv[1])), "font fixture registration failed");
    lString32Collection families;
    fontMan->getFaceList(families);
    check(families.length() > 0, "font fixture has no family");
    lString8 family = UnicodeToUtf8(families[0]);
    LVFontRef first = fontMan->GetFont(20, 400, false, css_ff_serif, family);
    LVFontRef second = fontMan->GetFont(24, 400, false, css_ff_serif, family);
    check(!first.isNull() && !second.isNull(), "font instances could not be loaded");
    check(live_faces >= 2, "the observer must track the loaded font instances");
    check(first.getRefCount() == 2 && second.getRefCount() == 2,
          "each fresh instance must be owned only by its caller and the cache");

    LVFontRef third = fontMan->GetFont(28, 400, false, css_ff_serif, family);
    check(!third.isNull(), "third font instance could not be loaded");
    second.Clear();
    fontMan->gc();
    check(third.getRefCount() == 2,
          "erasing a middle entry must not leave a strong reference in unused storage");
    third.Clear();
    fontMan->gc();
    second = fontMan->GetFont(24, 400, false, css_ff_serif, family);

    first->setFallbackFont(second);
    second->setFallbackFont(first);
    check(first.getRefCount() == 3 && second.getRefCount() == 3,
          "the test must construct a fallback-reference cycle");
    fontMan->gc();
    check(first.getRefCount() == 2 && second.getRefCount() == 2,
          "font collection must break cached fallback-reference cycles");

    // Decimal markers in non-HarfBuzz mode retain the font itself.
    first->setKerningMode(KERNING_MODE_DISABLED);
    LVFontRef marker = first->getDecimalListItemFont();
    check(marker.get() == first.get(), "decimal marker must use this font");
    marker.Clear();
    check(first.getRefCount() == 3, "the test must construct a list-marker self-reference");
    fontMan->gc();
    check(first.getRefCount() == 2, "font collection must break list-marker self-references");

    check(fontMan->SetFallbackFontFaces(family), "fallback font registration failed");
    LVFontRef caps = fontMan->GetFont(20, 400, false, css_ff_serif, family, LFNT_OT_FEATURES_P_SMCP);
    check(!caps.isNull(), "small-caps font could not be loaded");
    LVFont::glyph_info_t missing;
    caps->getGlyphInfo(0x10FFFF, &missing);
    check(first.getRefCount() > 2, "the missing glyph must retain a fallback from the base font");
    fontMan->gc();
    check(first.getRefCount() == 2, "synthetic fonts must forward collection to their wrapped fonts");

    LVFont::glyph_info_t glyph;
    check(first->getGlyphInfo('A', &glyph), "collection must preserve fonts held by callers");
    caps.Clear();
    first.Clear();
    second.Clear();
    fontMan->gc();
    check(created_faces > 0, "the FreeType destruction observer must be active");
    check(live_faces == 0, "collection must destroy every unreferenced FreeType face before shutdown");
    ShutdownFontManager();
    check(live_faces == 0, "font-manager shutdown must not leave faces alive");
    std::puts("font_gc_test: fallback cycles, marker references and live callers passed");
    return 0;
}
