/*
 * A simple panel for color swatches
 *
 * Authors:
 *   Jon A. Cruz
 *
 * Copyright (C) 2005 Jon A. Cruz
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "swatches.h"

#include <gtk/gtk.h>

#include <dialogs/dialog-events.h>
#include <gtk/gtkdialog.h> //for GTK_RESPONSE* types
#include <glibmm/i18n.h>
#include "interface.h"
#include "verbs.h"
#include "prefs-utils.h"
#include "inkscape.h"
#include "macros.h"
#include "document.h"
#include "desktop-handles.h"
#include "selection.h"
#include "display/nr-arena.h"
#include <glib.h>
#include <gtkmm/table.h>
#include "extension/db.h"
#include "desktop.h"
#include "inkscape.h"
#include "svg/svg.h"
#include "desktop-style.h"
#include "ui/previewable.h"

namespace Inkscape {
namespace UI {
namespace Dialogs {

SwatchesPanel* SwatchesPanel::instance = 0;

// This struct and hardcoded array are just temporary.
typedef struct {
    unsigned int r;
    unsigned int g;
    unsigned int b;
    char const * name;
} ColorEntry;

    ColorEntry colorSet[] = {
        {255, 250, 250, "snow (255 250 250)"},
        {248, 248, 255, "ghost white (248 248 255)"},
        {245, 245, 245, "white smoke (245 245 245)"},
        {220, 220, 220, "gainsboro (220 220 220)"},
        {255, 250, 240, "floral white (255 250 240)"},
        {253, 245, 230, "old lace (253 245 230)"},
        {250, 240, 230, "linen (250 240 230)"},
        {250, 235, 215, "antique white (250 235 215)"},
        {255, 239, 213, "papaya whip (255 239 213)"},
        {255, 235, 205, "blanched almond (255 235 205)"},
        {255, 228, 196, "bisque (255 228 196)"},
        {255, 218, 185, "peach puff (255 218 185)"},
        {255, 222, 173, "navajo white (255 222 173)"},
        {255, 228, 181, "moccasin (255 228 181)"},
        {255, 248, 220, "cornsilk (255 248 220)"},
        {255, 255, 240, "ivory (255 255 240)"},
        {255, 250, 205, "lemon chiffon (255 250 205)"},
        {255, 245, 238, "seashell (255 245 238)"},
        {240, 255, 240, "honeydew (240 255 240)"},
        {245, 255, 250, "mint cream (245 255 250)"},
        {240, 255, 255, "azure (240 255 255)"},
        {240, 248, 255, "alice blue (240 248 255)"},
        {230, 230, 250, "lavender (230 230 250)"},
        {255, 240, 245, "lavender blush (255 240 245)"},
        {255, 228, 225, "misty rose (255 228 225)"},
        {255, 255, 255, "white (255 255 255)"},
        {0, 0, 0, "black (  0   0   0)"},
        {47, 79, 79, "dark slate gray ( 47  79  79)"},
        {105, 105, 105, "dim gray (105 105 105)"},
        {112, 128, 144, "slate gray (112 128 144)"},
        {119, 136, 153, "light slate gray (119 136 153)"},
        {190, 190, 190, "gray (190 190 190)"},
        {211, 211, 211, "light gray (211 211 211)"},
        {25, 25, 112, "midnight blue ( 25  25 112)"},
        {0, 0, 128, "navy blue (  0   0 128)"},
        {100, 149, 237, "cornflower blue (100 149 237)"},
        {72, 61, 139, "dark slate blue ( 72  61 139)"},
        {106, 90, 205, "slate blue (106  90 205)"},
        {123, 104, 238, "medium slate blue (123 104 238)"},
        {132, 112, 255, "light slate blue (132 112 255)"},
        {0, 0, 205, "medium blue (  0   0 205)"},
        {65, 105, 225, "royal blue ( 65 105 225)"},
        {0, 0, 255, "blue (  0   0 255)"},
        {30, 144, 255, "dodger blue ( 30 144 255)"},
        {0, 191, 255, "deep sky blue (  0 191 255)"},
        {135, 206, 235, "sky blue (135 206 235)"},
        {135, 206, 250, "light sky blue (135 206 250)"},
        {70, 130, 180, "steel blue ( 70 130 180)"},
        {176, 196, 222, "light steel blue (176 196 222)"},
        {173, 216, 230, "light blue (173 216 230)"},
        {176, 224, 230, "powder blue (176 224 230)"},
        {175, 238, 238, "pale turquoise (175 238 238)"},
        {0, 206, 209, "dark turquoise (  0 206 209)"},
        {72, 209, 204, "medium turquoise ( 72 209 204)"},
        {64, 224, 208, "turquoise ( 64 224 208)"},
        {0, 255, 255, "cyan (  0 255 255)"},
        {224, 255, 255, "light cyan (224 255 255)"},
        {95, 158, 160, "cadet blue ( 95 158 160)"},
        {102, 205, 170, "medium aquamarine (102 205 170)"},
        {127, 255, 212, "aquamarine (127 255 212)"},
        {0, 100, 0, "dark green (  0 100   0)"},
        {85, 107, 47, "dark olive green ( 85 107  47)"},
        {143, 188, 143, "dark sea green (143 188 143)"},
        {46, 139, 87, "sea green ( 46 139  87)"},
        {60, 179, 113, "medium sea green ( 60 179 113)"},
        {32, 178, 170, "light sea green ( 32 178 170)"},
        {152, 251, 152, "pale green (152 251 152)"},
        {0, 255, 127, "spring green (  0 255 127)"},
        {124, 252, 0, "lawn green (124 252   0)"},
        {0, 255, 0, "green (  0 255   0)"},
        {127, 255, 0, "chartreuse (127 255   0)"},
        {0, 250, 154, "medium spring green (  0 250 154)"},
        {173, 255, 47, "green yellow (173 255  47)"},
        {50, 205, 50, "lime green ( 50 205  50)"},
        {154, 205, 50, "yellow green (154 205  50)"},
        {34, 139, 34, "forest green ( 34 139  34)"},
        {107, 142, 35, "olive drab (107 142  35)"},
        {189, 183, 107, "dark khaki (189 183 107)"},
        {240, 230, 140, "khaki (240 230 140)"},
        {238, 232, 170, "pale goldenrod (238 232 170)"},
        {250, 250, 210, "light goldenrod yellow (250 250 210)"},
        {255, 255, 224, "light yellow (255 255 224)"},
        {255, 255, 0, "yellow (255 255   0)"},
        {255, 215, 0, "gold (255 215   0 )"},
        {238, 221, 130, "light goldenrod (238 221 130)"},
        {218, 165, 32, "goldenrod (218 165  32)"},
        {184, 134, 11, "dark goldenrod (184 134  11)"},
        {188, 143, 143, "rosy brown (188 143 143)"},
        {205, 92, 92, "indian red (205  92  92)"},
        {139, 69, 19, "saddle brown (139  69  19)"},
        {160, 82, 45, "sienna (160  82  45)"},
        {205, 133, 63, "peru (205 133  63)"},
        {222, 184, 135, "burlywood (222 184 135)"},
        {245, 245, 220, "beige (245 245 220)"},
        {245, 222, 179, "wheat (245 222 179)"},
        {244, 164, 96, "sandy brown (244 164  96)"},
        {210, 180, 140, "tan (210 180 140)"},
        {210, 105, 30, "chocolate (210 105  30)"},
        {178, 34, 34, "firebrick (178  34  34)"},
        {165, 42, 42, "brown (165  42  42)"},
        {233, 150, 122, "dark salmon (233 150 122)"},
        {250, 128, 114, "salmon (250 128 114)"},
        {255, 160, 122, "light salmon (255 160 122)"},
        {255, 165, 0, "orange (255 165   0)"},
        {255, 140, 0, "dark orange (255 140   0)"},
        {255, 127, 80, "coral (255 127  80)"},
        {240, 128, 128, "light coral (240 128 128)"},
        {255, 99, 71, "tomato (255  99  71)"},
        {255, 69, 0, "orange red (255  69   0)"},
        {255, 0, 0, "red (255   0   0)"},
        {255, 105, 180, "hot pink (255 105 180)"},
        {255, 20, 147, "deep pink (255  20 147)"},
        {255, 192, 203, "pink (255 192 203)"},
        {255, 182, 193, "light pink (255 182 193)"},
        {219, 112, 147, "pale violet red (219 112 147)"},
        {176, 48, 96, "maroon (176  48  96)"},
        {199, 21, 133, "medium violet red (199  21 133)"},
        {208, 32, 144, "violet red (208  32 144)"},
        {255, 0, 255, "magenta (255   0 255)"},
        {238, 130, 238, "violet (238 130 238)"},
        {221, 160, 221, "plum (221 160 221)"},
        {218, 112, 214, "orchid (218 112 214)"},
        {186, 85, 211, "medium orchid (186  85 211)"},
        {153, 50, 204, "dark orchid (153  50 204)"},
        {148, 0, 211, "dark violet (148   0 211)"},
        {138, 43, 226, "blue violet (138  43 226)"},
        {160, 32, 240, "purple (160  32 240)"},
        {147, 112, 219, "medium purple (147 112 219)"},
        {216, 191, 216, "thistle (216 191 216)"},
        {255, 250, 250, "snow 1 (255 250 250)"},
        {238, 233, 233, "snow 2 (238 233 233)"},
        {205, 201, 201, "snow 3 (205 201 201)"},
        {139, 137, 137, "snow 4 (139 137 137)"},
        {255, 245, 238, "seashell 1 (255 245 238)"},
        {238, 229, 222, "seashell 2 (238 229 222)"},
        {205, 197, 191, "seashell 3 (205 197 191)"},
        {139, 134, 130, "seashell 4 (139 134 130)"},
        {255, 239, 219, "antique white 1 (255 239 219)"},
        {238, 223, 204, "antique white 2 (238 223 204)"},
        {205, 192, 176, "antique white 3 (205 192 176)"},
        {139, 131, 120, "antique white 4 (139 131 120)"},
        {255, 228, 196, "bisque 1 (255 228 196)"},
        {238, 213, 183, "bisque 2 (238 213 183)"},
        {205, 183, 158, "bisque 3 (205 183 158)"},
        {139, 125, 107, "bisque 4 (139 125 107)"},
        {255, 218, 185, "peach puff 1 (255 218 185)"},
        {238, 203, 173, "peach puff 2 (238 203 173)"},
        {205, 175, 149, "peach puff 3 (205 175 149)"},
        {139, 119, 101, "peach puff 4 (139 119 101)"},
        {255, 222, 173, "navajo white 1 (255 222 173)"},
        {238, 207, 161, "navajo white 2 (238 207 161)"},
        {205, 179, 139, "navajo white 3 (205 179 139)"},
        {139, 121, 94, "navajo white 4 (139 121  94)"},
        {255, 250, 205, "lemon chiffon 1 (255 250 205)"},
        {238, 233, 191, "lemon chiffon 2 (238 233 191)"},
        {205, 201, 165, "lemon chiffon 3 (205 201 165)"},
        {139, 137, 112, "lemon chiffon 4 (139 137 112)"},
        {255, 248, 220, "cornsilk 1 (255 248 220)"},
        {238, 232, 205, "cornsilk 2 (238 232 205)"},
        {205, 200, 177, "cornsilk 3 (205 200 177)"},
        {139, 136, 120, "cornsilk 4 (139 136 120)"},
        {255, 255, 240, "ivory 1 (255 255 240)"},
        {238, 238, 224, "ivory 2 (238 238 224)"},
        {205, 205, 193, "ivory 3 (205 205 193)"},
        {139, 139, 131, "ivory 4 (139 139 131)"},
        {240, 255, 240, "honeydew 1 (240 255 240)"},
        {224, 238, 224, "honeydew 2 (224 238 224)"},
        {193, 205, 193, "honeydew 3 (193 205 193)"},
        {131, 139, 131, "honeydew 4 (131 139 131)"},
        {255, 240, 245, "lavender blush 1 (255 240 245)"},
        {238, 224, 229, "lavender blush 2 (238 224 229)"},
        {205, 193, 197, "lavender blush 3 (205 193 197)"},
        {139, 131, 134, "lavender blush 4 (139 131 134)"},
        {255, 228, 225, "misty rose 1 (255 228 225)"},
        {238, 213, 210, "misty rose 2 (238 213 210)"},
        {205, 183, 181, "misty rose 3 (205 183 181)"},
        {139, 125, 123, "misty rose 4 (139 125 123)"},
        {240, 255, 255, "azure 1 (240 255 255)"},
        {224, 238, 238, "azure 2 (224 238 238)"},
        {193, 205, 205, "azure 3 (193 205 205)"},
        {131, 139, 139, "azure 4 (131 139 139)"},
        {131, 111, 255, "slate blue 1 (131 111 255)"},
        {122, 103, 238, "slate blue 2 (122 103 238)"},
        {105, 89, 205, "slate blue 3 (105  89 205)"},
        {71, 60, 139, "slate blue 4 ( 71  60 139)"},
        {72, 118, 255, "royal blue 1 ( 72 118 255)"},
        {67, 110, 238, "royal blue 2 ( 67 110 238)"},
        {58, 95, 205, "royal blue 3 ( 58  95 205)"},
        {39, 64, 139, "royal blue 4 ( 39  64 139)"},
        {0, 0, 255, "blue 1 (  0   0 255)"},
        {0, 0, 238, "blue 2 (  0   0 238)"},
        {0, 0, 205, "blue 3 (  0   0 205)"},
        {0, 0, 139, "blue 4 (  0   0 139)"},
        {30, 144, 255, "dodger blue 1 ( 30 144 255)"},
        {28, 134, 238, "dodger blue 2 ( 28 134 238)"},
        {24, 116, 205, "dodger blue 3 ( 24 116 205)"},
        {16, 78, 139, "dodger blue 4 ( 16  78 139)"},
        {99, 184, 255, "steel blue 1 ( 99 184 255)"},
        {92, 172, 238, "steel blue 2 ( 92 172 238)"},
        {79, 148, 205, "steel blue 3 ( 79 148 205)"},
        {54, 100, 139, "steel blue 4 ( 54 100 139)"},
        {0, 191, 255, "deep sky blue 1 (  0 191 255)"},
        {0, 178, 238, "deep sky blue 2 (  0 178 238)"},
        {0, 154, 205, "deep sky blue 3 (  0 154 205)"},
        {0, 104, 139, "deep sky blue 4 (  0 104 139)"},
        {135, 206, 255, "sky blue 1 (135 206 255)"},
        {126, 192, 238, "sky blue 2 (126 192 238)"},
        {108, 166, 205, "sky blue 3 (108 166 205)"},
        {74, 112, 139, "sky blue 4 ( 74 112 139)"},
        {176, 226, 255, "light sky blue 1 (176 226 255)"},
        {164, 211, 238, "light sky blue 2 (164 211 238)"},
        {141, 182, 205, "light sky blue 3 (141 182 205)"},
        {96, 123, 139, "light sky blue 4 ( 96 123 139)"},
        {198, 226, 255, "slate gray 1 (198 226 255)"},
        {185, 211, 238, "slate gray 2 (185 211 238)"},
        {159, 182, 205, "slate gray 3 (159 182 205)"},
        {108, 123, 139, "slate gray 4 (108 123 139)"},
        {202, 225, 255, "light steel blue 1 (202 225 255)"},
        {188, 210, 238, "light steel blue 2 (188 210 238)"},
        {162, 181, 205, "light steel blue 3 (162 181 205)"},
        {110, 123, 139, "light steel blue 4 (110 123 139)"},
        {191, 239, 255, "light blue 1 (191 239 255)"},
        {178, 223, 238, "light blue 2 (178 223 238)"},
        {154, 192, 205, "light blue 3 (154 192 205)"},
        {104, 131, 139, "light blue 4 (104 131 139)"},
        {224, 255, 255, "light cyan 1 (224 255 255)"},
        {209, 238, 238, "light cyan 2 (209 238 238)"},
        {180, 205, 205, "light cyan 3 (180 205 205)"},
        {122, 139, 139, "light cyan 4 (122 139 139)"},
        {187, 255, 255, "pale turquoise 1 (187 255 255)"},
        {174, 238, 238, "pale turquoise 2 (174 238 238)"},
        {150, 205, 205, "pale turquoise 3 (150 205 205)"},
        {102, 139, 139, "pale turquoise 4 (102 139 139)"},
        {152, 245, 255, "cadet blue 1 (152 245 255)"},
        {142, 229, 238, "cadet blue 2 (142 229 238)"},
        {122, 197, 205, "cadet blue 3 (122 197 205)"},
        {83, 134, 139, "cadet blue 4 ( 83 134 139)"},
        {0, 245, 255, "turquoise 1 (  0 245 255)"},
        {0, 229, 238, "turquoise 2 (  0 229 238)"},
        {0, 197, 205, "turquoise 3 (  0 197 205)"},
        {0, 134, 139, "turquoise 4 (  0 134 139)"},
        {0, 255, 255, "cyan 1 (  0 255 255)"},
        {0, 238, 238, "cyan 2 (  0 238 238)"},
        {0, 205, 205, "cyan 3 (  0 205 205)"},
        {0, 139, 139, "cyan 4 (  0 139 139)"},
        {151, 255, 255, "dark slate gray 1 (151 255 255)"},
        {141, 238, 238, "dark slate gray 2 (141 238 238)"},
        {121, 205, 205, "dark slate gray 3 (121 205 205)"},
        {82, 139, 139, "dark slate gray 4 ( 82 139 139)"},
        {127, 255, 212, "aquamarine 1 (127 255 212)"},
        {118, 238, 198, "aquamarine 2 (118 238 198)"},
        {102, 205, 170, "aquamarine 3 (102 205 170)"},
        {69, 139, 116, "aquamarine 4 ( 69 139 116)"},
        {193, 255, 193, "dark sea green 1 (193 255 193)"},
        {180, 238, 180, "dark sea green 2 (180 238 180)"},
        {155, 205, 155, "dark sea green 3 (155 205 155)"},
        {105, 139, 105, "dark sea green 4 (105 139 105)"},
        {84, 255, 159, "sea green 1 ( 84 255 159)"},
        {78, 238, 148, "sea green 2 ( 78 238 148)"},
        {67, 205, 128, "sea green 3 ( 67 205 128)"},
        {46, 139, 87, "sea green 4 ( 46 139)"},
        {154, 255, 154, "pale green 1 (154 255 154)"},
        {144, 238, 144, "pale green 2 (144 238 144)"},
        {124, 205, 124, "pale green 3 (124 205 124)"},
        {84, 139, 84, "pale green 4 ( 84 139)"},
        {0, 255, 127, "spring green 1 (  0 255 127)"},
        {0, 238, 118, "spring green 2 (  0 238 118)"},
        {0, 205, 102, "spring green 3 (  0 205 102)"},
        {0, 139, 69, "spring green 4 (  0 139  69)"},
        {0, 255, 0, "green 1 (  0 255   0)"},
        {0, 238, 0, "green 2 (  0 238   0)"},
        {0, 205, 0, "green 3 (  0 205   0)"},
        {0, 139, 0, "green 4 (  0 139   0)"},
        {127, 255, 0, "chartreuse 1 (127 255   0)"},
        {118, 238, 0, "chartreuse 2 (118 238   0)"},
        {102, 205, 0, "chartreuse 3 (102 205   0)"},
        {69, 139, 0, "chartreuse 4 ( 69 139   0)"},
        {192, 255, 62, "olive drab 1 (192 255  62)"},
        {179, 238, 58, "olive drab 2 (179 238  58)"},
        {154, 205, 50, "olive drab 3 (154 205  50)"},
        {105, 139, 34, "olive drab 4 (105 139  34)"},
        {202, 255, 112, "dark olive green 1 (202 255 112)"},
        {188, 238, 104, "dark olive green 2 (188 238 104)"},
        {162, 205, 90, "dark olive green 3 (162 205)"},
        {110, 139, 61, "dark olive green 4 (110 139)"},
        {255, 246, 143, "khaki 1 (255 246 143)"},
        {238, 230, 133, "khaki 2 (238 230 133)"},
        {205, 198, 115, "khaki 3 (205 198 115)"},
        {139, 134, 78, "khaki 4 (139 134  78)"},
        {255, 236, 139, "light goldenrod 1 (255 236 139)"},
        {238, 220, 130, "light goldenrod 2 (238 220 130)"},
        {205, 190, 112, "light goldenrod 3 (205 190 112)"},
        {139, 129, 76, "light goldenrod 4 (139 129  76)"},
        {255, 255, 224, "light yellow 1 (255 255 224)"},
        {238, 238, 209, "light yellow 2 (238 238 209)"},
        {205, 205, 180, "light yellow 3 (205 205 180)"},
        {139, 139, 122, "light yellow 4 (139 139 122)"},
        {255, 255, 0, "yellow 1 (255 255   0)"},
        {238, 238, 0, "yellow 2 (238 238   0)"},
        {205, 205, 0, "yellow 3 (205 205   0)"},
        {139, 139, 0, "yellow 4 (139 139   0)"},
        {255, 215, 0, "gold 1 (255 215   0)"},
        {238, 201, 0, "gold 2 (238 201   0)"},
        {205, 173, 0, "gold 3 (205 173   0)"},
        {139, 117, 0, "gold 4 (139 117   0)"},
        {255, 193, 37, "goldenrod 1 (255 193  37)"},
        {238, 180, 34, "goldenrod 2 (238 180  34)"},
        {205, 155, 29, "goldenrod 3 (205 155  29)"},
        {139, 105, 20, "goldenrod 4 (139 105  20)"},
        {255, 185, 15, "dark goldenrod 1 (255 185  15)"},
        {238, 173, 14, "dark goldenrod 2 (238 173  14)"},
        {205, 149, 12, "dark goldenrod 3 (205 149  12)"},
        {139, 101, 8, "dark goldenrod 4 (139 101   8)"},
        {255, 193, 193, "rosy brown 1 (255 193 193)"},
        {238, 180, 180, "rosy brown 2 (238 180 180)"},
        {205, 155, 155, "rosy brown 3 (205 155 155)"},
        {139, 105, 105, "rosy brown 4 (139 105 105)"},
        {255, 106, 106, "indian red 1 (255 106 106)"},
        {238, 99, 99, "indian red 2 (238  99  99)"},
        {205, 85, 85, "indian red 3 (205  85  85)"},
        {139, 58, 58, "indian red 4 (139  58  58)"},
        {255, 130, 71, "sienna 1 (255 130  71)"},
        {238, 121, 66, "sienna 2 (238 121  66)"},
        {205, 104, 57, "sienna 3 (205 104  57)"},
        {139, 71, 38, "sienna 4 (139  71  38)"},
        {255, 211, 155, "burlywood 1 (255 211 155)"},
        {238, 197, 145, "burlywood 2 (238 197 145)"},
        {205, 170, 125, "burlywood 3 (205 170 125)"},
        {139, 115, 85, "burlywood 4 (139 115)"},
        {255, 231, 186, "wheat 1 (255 231 186)"},
        {238, 216, 174, "wheat 2 (238 216 174)"},
        {205, 186, 150, "wheat 3 (205 186 150)"},
        {139, 126, 102, "wheat 4 (139 126 102)"},
        {255, 165, 79, "tan 1 (255 165  79)"},
        {238, 154, 73, "tan 2 (238 154  73)"},
        {205, 133, 63, "tan 3 (205 133  63)"},
        {139, 90, 43, "tan 4 (139  90  43)"},
        {255, 127, 36, "chocolate 1 (255 127  36)"},
        {238, 118, 33, "chocolate 2 (238 118  33)"},
        {205, 102, 29, "chocolate 3 (205 102  29)"},
        {139, 69, 19, "chocolate 4 (139  69  19)"},
        {255, 48, 48, "firebrick 1 (255  48  48)"},
        {238, 44, 44, "firebrick 2 (238  44  44)"},
        {205, 38, 38, "firebrick 3 (205  38  38)"},
        {139, 26, 26, "firebrick 4 (139  26  26)"},
        {255, 64, 64, "brown 1 (255  64  64)"},
        {238, 59, 59, "brown 2 (238  59  59)"},
        {205, 51, 51, "brown 3 (205  51  51)"},
        {139, 35, 35, "brown 4 (139  35  35)"},
        {255, 140, 105, "salmon 1 (255 140 105)"},
        {238, 130, 98, "salmon 2 (238 130  98)"},
        {205, 112, 84, "salmon 3 (205 112  84)"},
        {139, 76, 57, "salmon 4 (139  76  57)"},
        {255, 160, 122, "light salmon 1 (255 160 122)"},
        {238, 149, 114, "light salmon 2 (238 149 114)"},
        {205, 129, 98, "light salmon 3 (205 129  98)"},
        {139, 87, 66, "light salmon 4 (139  87  66)"},
        {255, 165, 0, "orange 1 (255 165   0)"},
        {238, 154, 0, "orange 2 (238 154   0)"},
        {205, 133, 0, "orange 3 (205 133   0)"},
        {139, 90, 0, "orange 4 (139  90   0)"},
        {255, 127, 0, "dark orange 1 (255 127   0)"},
        {238, 118, 0, "dark orange 2 (238 118   0)"},
        {205, 102, 0, "dark orange 3 (205 102   0)"},
        {139, 69, 0, "dark orange 4 (139  69   0)"},
        {255, 114, 86, "coral 1 (255 114  86)"},
        {238, 106, 80, "coral 2 (238 106  80)"},
        {205, 91, 69, "coral 3 (205  91  69)"},
        {139, 62, 47, "coral 4 (139  62  47)"},
        {255, 99, 71, "tomato 1 (255  99  71)"},
        {238, 92, 66, "tomato 2 (238  92  66)"},
        {205, 79, 57, "tomato 3 (205  79  57)"},
        {139, 54, 38, "tomato 4 (139  54  38)"},
        {255, 69, 0, "orange red 1 (255  69   0)"},
        {238, 64, 0, "orange red 2 (238  64   0)"},
        {205, 55, 0, "orange red 3 (205  55   0)"},
        {139, 37, 0, "orange red 4 (139  37   0)"},
        {255, 0, 0, "red 1 (255   0   0)"},
        {238, 0, 0, "red 2 (238   0   0)"},
        {205, 0, 0, "red 3 (205   0   0)"},
        {139, 0, 0, "red 4 (139   0   0)"},
        {255, 20, 147, "deep pink 1 (255  20 147)"},
        {238, 18, 137, "deep pink 2 (238  18 137)"},
        {205, 16, 118, "deep pink 3 (205  16 118)"},
        {139, 10, 80, "deep pink 4 (139  10  80)"},
        {255, 110, 180, "hot pink 1 (255 110 180)"},
        {238, 106, 167, "hot pink 2 (238 106 167)"},
        {205, 96, 144, "hot pink 3 (205  96 144)"},
        {139, 58, 98, "hot pink 4 (139  58  98)"},
        {255, 181, 197, "pink 1 (255 181 197)"},
        {238, 169, 184, "pink 2 (238 169 184)"},
        {205, 145, 158, "pink 3 (205 145 158)"},
        {139, 99, 108, "pink 4 (139  99 108)"},
        {255, 174, 185, "light pink 1 (255 174 185)"},
        {238, 162, 173, "light pink 2 (238 162 173)"},
        {205, 140, 149, "light pink 3 (205 140 149)"},
        {139, 95, 101, "light pink 4 (139  95 101)"},
        {255, 130, 171, "pale violet red 1 (255 130 171)"},
        {238, 121, 159, "pale violet red 2 (238 121 159)"},
        {205, 104, 137, "pale violet red 3 (205 104 137)"},
        {139, 71, 93, "pale violet red 4 (139  71  93)"},
        {255, 52, 179, "maroon 1 (255  52 179)"},
        {238, 48, 167, "maroon 2 (238  48 167)"},
        {205, 41, 144, "maroon 3 (205  41 144)"},
        {139, 28, 98, "maroon 4 (139  28  98)"},
        {255, 62, 150, "violet red 1 (255  62 150)"},
        {238, 58, 140, "violet red 2 (238  58 140)"},
        {205, 50, 120, "violet red 3 (205  50 120)"},
        {139, 34, 82, "violet red 4 (139  34  82)"},
        {255, 0, 255, "magenta 1 (255   0 255)"},
        {238, 0, 238, "magenta 2 (238   0 238)"},
        {205, 0, 205, "magenta 3 (205   0 205)"},
        {139, 0, 139, "magenta 4 (139   0 139)"},
        {255, 131, 250, "orchid 1 (255 131 250)"},
        {238, 122, 233, "orchid 2 (238 122 233)"},
        {205, 105, 201, "orchid 3 (205 105 201)"},
        {139, 71, 137, "orchid 4 (139  71 137)"},
        {255, 187, 255, "plum 1 (255 187 255)"},
        {238, 174, 238, "plum 2 (238 174 238)"},
        {205, 150, 205, "plum 3 (205 150 205)"},
        {139, 102, 139, "plum 4 (139 102 139)"},
        {224, 102, 255, "medium orchid 1 (224 102 255)"},
        {209, 95, 238, "medium orchid 2 (209  95 238)"},
        {180, 82, 205, "medium orchid 3 (180  82 205)"},
        {122, 55, 139, "medium orchid 4 (122  55 139)"},
        {191, 62, 255, "dark orchid 1 (191  62 255)"},
        {178, 58, 238, "dark orchid 2 (178  58 238)"},
        {154, 50, 205, "dark orchid 3 (154  50 205)"},
        {104, 34, 139, "dark orchid 4 (104  34 139)"},
        {155, 48, 255, "purple 1 (155  48 255)"},
        {145, 44, 238, "purple 2 (145  44 238)"},
        {125, 38, 205, "purple 3 (125  38 205)"},
        {85, 26, 139, "purple 4 ( 85  26 139)"},
        {171, 130, 255, "medium purple 1 (171 130 255)"},
        {159, 121, 238, "medium purple 2 (159 121 238)"},
        {137, 104, 205, "medium purple 3 (137 104 205)"},
        {93, 71, 139, "medium purple 4 ( 93  71 139)"},
        {255, 225, 255, "thistle 1 (255 225 255)"},
        {238, 210, 238, "thistle 2 (238 210 238)"},
        {205, 181, 205, "thistle 3 (205 181 205)"},
        {139, 123, 139, "thistle 4 (139 123 139)"},
        {169, 169, 169, "dark grey (169 169 169)"},
        {169, 169, 169, "dark gray (169 169 169)"},
        {0, 0, 139, "dark blue (0     0 139)"},
        {0, 139, 139, "dark cyan (0   139 139)"},
        {139, 0, 139, "dark magenta (139   0 139)"},
        {139, 0, 0, "dark red (139   0   0)"},
        {144, 238, 144, "light green (144 238 144)"}
    };



class ColorItem : public Inkscape::UI::Previewable
{
public:
    ColorItem( unsigned int r, unsigned int g, unsigned int b, Glib::ustring& name );
    virtual ~ColorItem();
    ColorItem(ColorItem const &other);
    virtual ColorItem &operator=(ColorItem const &other);

    virtual Gtk::Widget* getPreview(PreviewStyle style, Gtk::BuiltinIconSize size);

    void buttonClicked();

    unsigned int _r;
    unsigned int _g;
    unsigned int _b;
    Glib::ustring _name;
};

ColorItem::ColorItem( unsigned int r, unsigned int g, unsigned int b, Glib::ustring& name ) :
    _r(r),
    _g(g),
    _b(b),
    _name(name)
{
}

ColorItem::~ColorItem()
{
}

ColorItem::ColorItem(ColorItem const &other) :
    Inkscape::UI::Previewable()
{
    if ( this != &other ) {
        *this = other;
    }
}

ColorItem &ColorItem::operator=(ColorItem const &other)
{
    if ( this != &other ) {
        _r = other._r;
        _g = other._g;
        _b = other._b;
        _name = other._name;
    }
    return *this;
}


Gtk::Widget* ColorItem::getPreview(PreviewStyle style, Gtk::BuiltinIconSize size)
{
    Gtk::Widget* widget = 0;
    if ( style == PREVIEW_STYLE_BLURB ) {
        Gtk::Label *lbl = new Gtk::Label(_name);
        lbl->set_alignment(Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER);
        widget = lbl;
    } else {
        Glib::ustring blank("          ");
        if ( size == Gtk::ICON_SIZE_MENU ) {
            blank = " ";
        }

        Gtk::Button *btn = new Gtk::Button(blank);
        Gdk::Color color;
        color.set_rgb(_r << 8, _g << 8, _b << 8);
        btn->modify_bg(Gtk::STATE_NORMAL, color);
        btn->modify_bg(Gtk::STATE_ACTIVE, color);
        btn->modify_bg(Gtk::STATE_PRELIGHT, color);
        btn->modify_bg(Gtk::STATE_SELECTED, color);
        btn->signal_clicked().connect( sigc::mem_fun(*this, &ColorItem::buttonClicked) );
        widget = btn;
    }

    return widget;
}

void ColorItem::buttonClicked()
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (desktop) {
        guint32 rgba = (_r << 24) | (_g << 16) | (_b << 8) | 0xff;
        //g_object_set_data(G_OBJECT(cp), "color", GUINT_TO_POINTER(rgba));
        Inkscape::XML::Node *repr = SP_OBJECT_REPR(desktop->namedview);

        gchar c[64];
        sp_svg_write_color(c, 64, rgba);
        if (repr)
            sp_repr_set_attr(repr, "fill", c);


        SPCSSAttr *css = sp_repr_css_attr_new();
        sp_repr_css_set_property( css, "fill", c );
        sp_desktop_set_style(desktop, css);

        sp_repr_css_attr_unref(css);
    }
}














SwatchesPanel& SwatchesPanel::getInstance()
{
    if ( !instance ) {
        instance = new SwatchesPanel();
    }

    return *instance;
}



/**
 * Constructor
 */
SwatchesPanel::SwatchesPanel() :
    _holder(0)
{
    _holder = new PreviewHolder();
    for ( unsigned int i = 0; i < G_N_ELEMENTS(colorSet); i++ ) {
        Glib::ustring str(colorSet[i].name);
        ColorItem *item = new ColorItem( colorSet[i].r, colorSet[i].g, colorSet[i].b, str );
        _holder->addPreview(item);
    }

    pack_start(*_holder, Gtk::PACK_EXPAND_WIDGET);
    setTargetFillable(_holder);

    show_all_children();
}

SwatchesPanel::~SwatchesPanel()
{
}

void SwatchesPanel::changeItTo(int val)
{
    switch ( val ) {
        case 0:
        {
            _holder->setStyle(Gtk::ICON_SIZE_BUTTON, VIEW_TYPE_LIST);
        }
        break;

        case 1:
        {
            _holder->setStyle(Gtk::ICON_SIZE_BUTTON, VIEW_TYPE_GRID);
        }
        break;

        default:
        {
            _holder->setStyle(Gtk::ICON_SIZE_MENU, VIEW_TYPE_GRID);
        }
    }
}


} //namespace Dialogs
} //namespace UI
} //namespace Inkscape


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
