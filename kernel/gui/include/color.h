#ifndef GUI_COLOR_H
#define GUI_COLOR_H

#include "types.h"

/* ============================================================
 * Kenux GUI Color System
 * Supports 32-bit true color (16,777,216 colors) with alpha
 * Uses integer math (no FPU required) - factors use 0-256 scale
 * ============================================================ */

/* Core color macro: 0xAARRGGBB format (alpha in high byte) */
#define RGBA(r, g, b, a)  ((uint32_t)(((uint32_t)(a) << 24) | \
                                      ((uint32_t)(r) << 16) | \
                                      ((uint32_t)(g) << 8)  | \
                                       (uint32_t)(b)))

/* RGB macro: opaque color (alpha = 255) */
#undef RGB
#define RGB(r, g, b)  RGBA(r, g, b, 0xFF)

/* Extract color channels */
#define COLOR_R(c)  (((c) >> 16) & 0xFF)
#define COLOR_G(c)  (((c) >> 8)  & 0xFF)
#define COLOR_B(c)  ((c) & 0xFF)
#define COLOR_A(c)  (((c) >> 24) & 0xFF)

/* ============================================================
 * Comprehensive Named Color Palette (150+ colors)
 * CSS/X11 + Material Design + Apple System colors
 * ============================================================ */

/* --- Basic colors --- */
#define COL_BLACK           RGB(0,   0,   0)
#define COL_WHITE           RGB(255, 255, 255)
#define COL_RED             RGB(255, 0,   0)
#define COL_GREEN           RGB(0,   255, 0)
#define COL_BLUE            RGB(0,   0,   255)
#define COL_YELLOW          RGB(255, 255, 0)
#define COL_CYAN            RGB(0,   255, 255)
#define COL_MAGENTA         RGB(255, 0,   255)

/* --- Grayscale (10 levels) --- */
#define COL_GRAY10          RGB(26,  26,  26)
#define COL_GRAY20          RGB(51,  51,  51)
#define COL_GRAY30          RGB(77,  77,  77)
#define COL_GRAY40          RGB(102, 102, 102)
#define COL_GRAY50          RGB(128, 128, 128)
#define COL_GRAY60          RGB(153, 153, 153)
#define COL_GRAY70          RGB(179, 179, 179)
#define COL_GRAY80          RGB(204, 204, 204)
#define COL_GRAY90          RGB(230, 230, 230)
#define COL_GRAY95          RGB(242, 242, 242)
#define COL_DARKGRAY        RGB(64,  64,  64)
#define COL_GRAY            RGB(128, 128, 128)
#define COL_LIGHTGRAY       RGB(211, 211, 211)
#define COL_GAINSBORO       RGB(220, 220, 220)
#define COL_WHITESMOKE      RGB(245, 245, 245)
#define COL_SNOW            RGB(255, 250, 250)

/* --- Reds --- */
#define COL_DARKRED         RGB(139, 0,   0)
#define COL_FIREBRICK       RGB(178, 34,  34)
#define COL_CRIMSON         RGB(220, 20,  60)
#define COL_INDIANRED       RGB(205, 92,  92)
#define COL_LIGHTCORAL      RGB(240, 128, 128)
#define COL_SALMON          RGB(250, 128, 114)
#define COL_DARKSALMON      RGB(233, 150, 122)
#define COL_LIGHTSALMON     RGB(255, 160, 122)
#define COL_TOMATO          RGB(255, 99,  71)
#define COL_ORANGERED       RGB(255, 69,  0)

/* --- Oranges --- */
#define COL_ORANGE          RGB(255, 165, 0)
#define COL_DARKORANGE      RGB(255, 140, 0)
#define COL_CORAL           RGB(255, 127, 80)
#define COL_GOLD            RGB(255, 215, 0)
#define COL_DARKGOLDENROD   RGB(184, 134, 11)
#define COL_GOLDENROD       RGB(218, 165, 32)
#define COL_KHAKI           RGB(240, 230, 140)
#define COL_DARKKHAKI       RGB(189, 183, 107)

/* --- Yellows --- */
#define COL_LIGHTYELLOW     RGB(255, 255, 224)
#define COL_LEMONCHIFFON    RGB(255, 250, 205)
#define COL_PAPAYAWHIP      RGB(255, 239, 213)
#define COL_MOCCASIN        RGB(255, 228, 181)
#define COL_PEACHPUFF       RGB(255, 218, 185)
#define COL_PALEGOLDENROD   RGB(238, 232, 170)

/* --- Greens --- */
#define COL_DARKGREEN       RGB(0,   100, 0)
#define COL_FORESTGREEN     RGB(34,  139, 34)
#define COL_SEAGREEN        RGB(46,  139, 87)
#define COL_MEDIUMSEAGREEN  RGB(60,  179, 113)
#define COL_LIGHTSEAGREEN   RGB(32,  178, 170)
#define COL_LIMEGREEN       RGB(50,  205, 50)
#define COL_LIME            RGB(0,   255, 0)
#define COL_CHARTREUSE      RGB(127, 255, 0)
#define COL_LAWNGREEN       RGB(124, 252, 0)
#define COL_GREENYELLOW     RGB(173, 255, 47)
#define COL_SPRINGGREEN     RGB(0,   255, 127)
#define COL_MEDIUMSPRINGGREEN RGB(0, 250, 154)
#define COL_PALEGREEN       RGB(152, 251, 152)
#define COL_LIGHTGREEN      RGB(144, 238, 144)
#define COL_OLIVE           RGB(128, 128, 0)
#define COL_OLIVEDRAB       RGB(107, 142, 35)
#define COL_DARKOLIVEGREEN  RGB(85,  107, 47)
#define COL_YELLOWGREEN     RGB(154, 205, 50)

/* --- Cyans / Teals --- */
#define COL_AQUA            RGB(0,   255, 255)
#define COL_AQUAMARINE      RGB(127, 255, 212)
#define COL_TURQUOISE       RGB(64,  224, 208)
#define COL_MEDIUMTURQUOISE RGB(72,  209, 204)
#define COL_DARKTURQUOISE   RGB(0,   206, 209)
#define COL_LIGHTCYAN       RGB(224, 255, 255)
#define COL_PALETURQUOISE   RGB(175, 238, 238)
#define COL_TEAL            RGB(0,   128, 128)
#define COL_MEDIUMAQUAMARINE RGB(102, 205, 170)

/* --- Blues --- */
#define COL_NAVY            RGB(0,   0,   128)
#define COL_MIDNIGHTBLUE    RGB(25,  25,  112)
#define COL_DARKBLUE        RGB(0,   0,   139)
#define COL_MEDIUMBLUE      RGB(0,   0,   205)
#define COL_ROYALBLUE       RGB(65,  105, 225)
#define COL_STEELBLUE       RGB(70,  130, 180)
#define COL_DODGERBLUE      RGB(30,  144, 255)
#define COL_DEEPSKYBLUE     RGB(0,   191, 255)
#define COL_SKYBLUE         RGB(135, 206, 235)
#define COL_LIGHTSKYBLUE    RGB(135, 206, 250)
#define COL_LIGHTBLUE       RGB(173, 216, 230)
#define COL_POWDERBLUE      RGB(176, 224, 230)
#define COL_LIGHTSTEELBLUE  RGB(176, 196, 222)
#define COL_CORNFLOWERBLUE  RGB(100, 149, 237)
#define COL_ALICEBLUE       RGB(240, 248, 255)
#define COL_CADETBLUE       RGB(95,  158, 160)
#define COL_AZURE           RGB(240, 255, 255)
#define COL_BLUEVIOLET      RGB(138, 43,  226)

/* --- Purples / Violets --- */
#define COL_INDIGO          RGB(75,  0,   130)
#define COL_PURPLE          RGB(128, 0,   128)
#define COL_DARKVIOLET      RGB(148, 0,   211)
#define COL_DARKORCHID      RGB(153, 50,  204)
#define COL_MEDIUMORCHID    RGB(186, 85,  211)
#define COL_ORCHID          RGB(218, 112, 214)
#define COL_VIOLET          RGB(238, 130, 238)
#define COL_PLUM            RGB(221, 160, 221)
#define COL_THISTLE         RGB(216, 191, 216)
#define COL_LAVENDER        RGB(230, 230, 250)
#define COL_MEDIUMPURPLE    RGB(147, 112, 219)
#define COL_MEDIUMSLATEBLUE RGB(123, 104, 238)
#define COL_SLATEBLUE       RGB(106, 90,  205)
#define COL_DARKSLATEBLUE   RGB(72,  61,  139)
#define COL_REBECCAPURPLE   RGB(102, 51,  153)

/* --- Pinks --- */
#define COL_PINK            RGB(255, 192, 203)
#define COL_LIGHTPINK       RGB(255, 182, 193)
#define COL_HOTPINK         RGB(255, 105, 180)
#define COL_DEEPPINK        RGB(255, 20,  147)
#define COL_PALEVIOLETRED   RGB(219, 112, 147)
#define COL_MEDIUMVIOLETRED RGB(199, 21,  133)
#define COL_LAVENDERBLUSH   RGB(255, 240, 245)

/* --- Browns --- */
#define COL_BROWN           RGB(165, 42,  42)
#define COL_SADDLEBROWN     RGB(139, 69,  19)
#define COL_SIENNA          RGB(160, 82,  45)
#define COL_CHOCOLATE       RGB(210, 105, 30)
#define COL_PERU            RGB(205, 133, 63)
#define COL_TAN             RGB(210, 180, 140)
#define COL_ROSYBROWN       RGB(188, 143, 143)
#define COL_SANDYBROWN      RGB(244, 164, 96)
#define COL_BURLYWOOD       RGB(222, 184, 135)
#define COL_WHEAT           RGB(245, 222, 179)
#define COL_NAVAJOWHITE     RGB(255, 222, 173)
#define COL_BISQUE          RGB(255, 228, 196)
#define COL_BLANCHEDALMOND  RGB(255, 235, 205)
#define COL_CORNSILK        RGB(255, 248, 220)
#define COL_OLDLACE         RGB(253, 245, 230)
#define COL_LINEN           RGB(250, 240, 230)
#define COL_ANTIQUEWHITE    RGB(250, 235, 215)

/* --- Material Design colors --- */
#define COL_MAT_RED         RGB(244, 67,  54)
#define COL_MAT_PINK        RGB(233, 30,  99)
#define COL_MAT_PURPLE      RGB(156, 39,  176)
#define COL_MAT_DEEPPURPLE  RGB(103, 58,  183)
#define COL_MAT_INDIGO      RGB(63,  81,  181)
#define COL_MAT_BLUE        RGB(33,  150, 243)
#define COL_MAT_LIGHTBLUE   RGB(3,   169, 244)
#define COL_MAT_CYAN        RGB(0,   188, 212)
#define COL_MAT_TEAL        RGB(0,   150, 136)
#define COL_MAT_GREEN       RGB(76,  175, 80)
#define COL_MAT_LIGHTGREEN  RGB(139, 195, 74)
#define COL_MAT_LIME        RGB(205, 220, 57)
#define COL_MAT_YELLOW      RGB(255, 235, 59)
#define COL_MAT_AMBER       RGB(255, 193, 7)
#define COL_MAT_ORANGE      RGB(255, 152, 0)
#define COL_MAT_DEEPORANGE  RGB(255, 87,  34)
#define COL_MAT_BROWN       RGB(121, 85,  72)
#define COL_MAT_GREY        RGB(158, 158, 158)
#define COL_MAT_BLUEGREY    RGB(96,  125, 139)

/* --- Apple system colors (dark mode) --- */
#define COL_APPLE_BLUE      RGB(0x0A, 0x84, 0xFF)
#define COL_APPLE_GREEN     RGB(0x30, 0xD1, 0x58)
#define COL_APPLE_INDIGO    RGB(0x5E, 0x5C, 0xE6)
#define COL_APPLE_ORANGE    RGB(0xFF, 0x9F, 0x0A)
#define COL_APPLE_PINK      RGB(0xFF, 0x37, 0x85)
#define COL_APPLE_PURPLE    RGB(0xBF, 0x5A, 0xF2)
#define COL_APPLE_RED       RGB(0xFF, 0x45, 0x3A)
#define COL_APPLE_TEAL      RGB(0x40, 0xCA, 0xE8)
#define COL_APPLE_YELLOW    RGB(0xFF, 0xD6, 0x0A)
#define COL_APPLE_MINT      RGB(0x66, 0xD4, 0xCF)
#define COL_APPLE_CYAN      RGB(0x5A, 0xC8, 0xFA)
#define COL_APPLE_BROWN     RGB(0xAC, 0x8E, 0x68)

/* --- Apple system colors (light mode) --- */
#define COL_APPLE_BLUE_LT   RGB(0x00, 0x78, 0xD7)
#define COL_APPLE_GREEN_LT  RGB(0x34, 0xC7, 0x59)
#define COL_APPLE_INDIGO_LT RGB(0x36, 0x34, 0xE3)
#define COL_APPLE_PINK_LT   RGB(0xFF, 0x2D, 0x92)
#define COL_APPLE_PURPLE_LT RGB(0xAF, 0x52, 0xDE)
#define COL_APPLE_RED_LT    RGB(0xFF, 0x3B, 0x30)
#define COL_APPLE_TEAL_LT   RGB(0x30, 0xC0, 0xE8)
#define COL_APPLE_YELLOW_LT RGB(0xFF, 0xCC, 0x00)
#define COL_APPLE_MINT_LT   RGB(0x00, 0xC7, 0xBE)
#define COL_APPLE_CYAN_LT   RGB(0x32, 0xAE, 0xF6)

/* --- Dark mode backgrounds (Apple-style) --- */
#define COL_DARK_BG             RGB(0x1C, 0x1C, 0x1E)
#define COL_DARK_BG_SECONDARY   RGB(0x2C, 0x2C, 0x2E)
#define COL_DARK_BG_TERTIARY    RGB(0x3A, 0x3A, 0x3C)
#define COL_DARK_BG_ELEVATED    RGB(0x48, 0x48, 0x4A)
#define COL_DARK_SEPARATOR      RGB(0x38, 0x38, 0x3A)
#define COL_DARK_TEXT           RGB(0xFF, 0xFF, 0xFF)
#define COL_DARK_TEXT_SECONDARY RGB(0x8E, 0x8E, 0x93)
#define COL_DARK_TEXT_TERTIARY  RGB(0x48, 0x48, 0x4A)

/* --- Light mode backgrounds --- */
#define COL_LIGHT_BG             RGB(0xF2, 0xF2, 0xF7)
#define COL_LIGHT_BG_SECONDARY   RGB(0xFF, 0xFF, 0xFF)
#define COL_LIGHT_BG_TERTIARY    RGB(0xE5, 0xE5, 0xEA)
#define COL_LIGHT_BG_ELEVATED    RGB(0xFF, 0xFF, 0xFF)
#define COL_LIGHT_SEPARATOR      RGB(0xC6, 0xC6, 0xC8)
#define COL_LIGHT_TEXT           RGB(0x00, 0x00, 0x00)
#define COL_LIGHT_TEXT_SECONDARY RGB(0x3C, 0x3C, 0x43)
#define COL_LIGHT_TEXT_TERTIARY  RGB(0x8E, 0x8E, 0x93)

/* --- Accent color presets --- */
#define ACCENT_BLUE     RGB(0x0A, 0x84, 0xFF)
#define ACCENT_PURPLE   RGB(0xBF, 0x5A, 0xF2)
#define ACCENT_PINK     RGB(0xFF, 0x37, 0x85)
#define ACCENT_RED      RGB(0xFF, 0x45, 0x3A)
#define ACCENT_ORANGE   RGB(0xFF, 0x9F, 0x0A)
#define ACCENT_YELLOW   RGB(0xFF, 0xD6, 0x0A)
#define ACCENT_GREEN    RGB(0x30, 0xD1, 0x58)
#define ACCENT_MINT     RGB(0x66, 0xD4, 0xCF)
#define ACCENT_TEAL     RGB(0x40, 0xCA, 0xE8)
#define ACCENT_CYAN     RGB(0x5A, 0xC8, 0xFA)
#define ACCENT_INDIGO   RGB(0x5E, 0x5C, 0xE6)
#define ACCENT_ROSE     RGB(0xF4, 0x3F, 0x5E)
#define ACCENT_AMBER    RGB(0xF5, 0x9E, 0x0B)
#define ACCENT_LIME     RGB(0x84, 0xCC, 0x16)
#define ACCENT_EMERALD  RGB(0x10, 0xB9, 0x81)
#define ACCENT_SKY      RGB(0x0E, 0xA5, 0xE9)
#define ACCENT_VIOLET   RGB(0x8B, 0x5C, 0xF6)
#define ACCENT_FUCHSIA  RGB(0xD9, 0x46, 0xEF)

/* ============================================================
 * Tailwind CSS Color Palette (50-900 for each hue)
 * The most comprehensive modern color system used by
 * modern operating systems and web frameworks
 * ============================================================ */

/* Slate */
#define COL_SLATE_50    RGB(0xF8, 0xFA, 0xFC)
#define COL_SLATE_100   RGB(0xF1, 0xF5, 0xF9)
#define COL_SLATE_200   RGB(0xE2, 0xE8, 0xF0)
#define COL_SLATE_300   RGB(0xCB, 0xD5, 0xE1)
#define COL_SLATE_400   RGB(0x94, 0xA3, 0xB8)
#define COL_SLATE_500   RGB(0x64, 0x74, 0x8B)
#define COL_SLATE_600   RGB(0x47, 0x55, 0x69)
#define COL_SLATE_700   RGB(0x33, 0x41, 0x55)
#define COL_SLATE_800   RGB(0x1E, 0x29, 0x3B)
#define COL_SLATE_900   RGB(0x0F, 0x17, 0x2A)

/* Gray */
#define COL_GRAY_50     RGB(0xF9, 0xFA, 0xFB)
#define COL_GRAY_100    RGB(0xF3, 0xF4, 0xF6)
#define COL_GRAY_200    RGB(0xE5, 0xE7, 0xEB)
#define COL_GRAY_300    RGB(0xD1, 0xD5, 0xDB)
#define COL_GRAY_400    RGB(0x9C, 0xA3, 0xAF)
#define COL_GRAY_500    RGB(0x6B, 0x72, 0x80)
#define COL_GRAY_600    RGB(0x4B, 0x55, 0x63)
#define COL_GRAY_700    RGB(0x37, 0x41, 0x51)
#define COL_GRAY_800    RGB(0x1F, 0x29, 0x37)
#define COL_GRAY_900    RGB(0x11, 0x18, 0x27)

/* Red */
#define COL_RED_50      RGB(0xFE, 0xF2, 0xF2)
#define COL_RED_100     RGB(0xFE, 0xE2, 0xE2)
#define COL_RED_200     RGB(0xFE, 0xCA, 0xCA)
#define COL_RED_300     RGB(0xFC, 0xA5, 0xA5)
#define COL_RED_400     RGB(0xF8, 0x71, 0x71)
#define COL_RED_500     RGB(0xEF, 0x44, 0x44)
#define COL_RED_600     RGB(0xDC, 0x26, 0x26)
#define COL_RED_700     RGB(0xB9, 0x1C, 0x1C)
#define COL_RED_800     RGB(0x99, 0x1B, 0x1B)
#define COL_RED_900     RGB(0x7F, 0x1D, 0x1D)

/* Orange */
#define COL_ORANGE_50   RGB(0xFF, 0xF7, 0xED)
#define COL_ORANGE_100  RGB(0xFF, 0xED, 0xD5)
#define COL_ORANGE_200  RGB(0xFE, 0xD7, 0xAA)
#define COL_ORANGE_300  RGB(0xFD, 0xBA, 0x74)
#define COL_ORANGE_400  RGB(0xFB, 0x92, 0x3C)
#define COL_ORANGE_500  RGB(0xF9, 0x73, 0x16)
#define COL_ORANGE_600  RGB(0xEA, 0x58, 0x0C)
#define COL_ORANGE_700  RGB(0xC2, 0x4A, 0x0A)
#define COL_ORANGE_800  RGB(0x9A, 0x34, 0x12)
#define COL_ORANGE_900  RGB(0x7C, 0x2D, 0x12)

/* Amber */
#define COL_AMBER_50    RGB(0xFF, 0xFB, 0xEB)
#define COL_AMBER_100   RGB(0xFE, 0xF3, 0xC7)
#define COL_AMBER_200   RGB(0xFD, 0xE6, 0x8A)
#define COL_AMBER_300   RGB(0xFC, 0xD3, 0x4D)
#define COL_AMBER_400   RGB(0xFB, 0xBB, 0x24)
#define COL_AMBER_500   RGB(0xF5, 0x9E, 0x0B)
#define COL_AMBER_600   RGB(0xD9, 0x77, 0x06)
#define COL_AMBER_700   RGB(0xB4, 0x53, 0x09)
#define COL_AMBER_800   RGB(0x92, 0x42, 0x0E)
#define COL_AMBER_900   RGB(0x78, 0x35, 0x0F)

/* Yellow */
#define COL_YELLOW_50   RGB(0xFE, 0xFC, 0xE8)
#define COL_YELLOW_100  RGB(0xFE, 0xF9, 0xC3)
#define COL_YELLOW_200  RGB(0xFE, 0xF0, 0x8A)
#define COL_YELLOW_300  RGB(0xFD, 0xDE, 0x37)
#define COL_YELLOW_400  RGB(0xFA, 0xCC, 0x15)
#define COL_YELLOW_500  RGB(0xEA, 0xB3, 0x08)
#define COL_YELLOW_600  RGB(0xCA, 0x8A, 0x04)
#define COL_YELLOW_700  RGB(0xA1, 0x62, 0x07)
#define COL_YELLOW_800  RGB(0x85, 0x4D, 0x0E)
#define COL_YELLOW_900  RGB(0x71, 0x3F, 0x12)

/* Lime */
#define COL_LIME_50     RGB(0xF7, 0xFE, 0xE7)
#define COL_LIME_100    RGB(0xEC, 0xFC, 0xCC)
#define COL_LIME_200    RGB(0xD9, 0xF9, 0x9D)
#define COL_LIME_300    RGB(0xBE, 0xF2, 0x64)
#define COL_LIME_400    RGB(0xA3, 0xE6, 0x35)
#define COL_LIME_500    RGB(0x84, 0xCC, 0x16)
#define COL_LIME_600    RGB(0x65, 0xA3, 0x0D)
#define COL_LIME_700    RGB(0x4D, 0x7C, 0x0F)
#define COL_LIME_800    RGB(0x3F, 0x62, 0x1F)
#define COL_LIME_900    RGB(0x36, 0x53, 0x14)

/* Green */
#define COL_GREEN_50    RGB(0xF0, 0xFD, 0xF4)
#define COL_GREEN_100   RGB(0xDC, 0xFC, 0xE7)
#define COL_GREEN_200   RGB(0xBB, 0xF7, 0xD0)
#define COL_GREEN_300   RGB(0x86, 0xEF, 0xAC)
#define COL_GREEN_400   RGB(0x4A, 0xDE, 0x80)
#define COL_GREEN_500   RGB(0x22, 0xC5, 0x5E)
#define COL_GREEN_600   RGB(0x16, 0xA3, 0x4A)
#define COL_GREEN_700   RGB(0x15, 0x80, 0x3D)
#define COL_GREEN_800   RGB(0x16, 0x65, 0x34)
#define COL_GREEN_900   RGB(0x14, 0x53, 0x2D)

/* Emerald */
#define COL_EMERALD_50  RGB(0xEC, 0xFD, 0xF5)
#define COL_EMERALD_100 RGB(0xD1, 0xFA, 0xE5)
#define COL_EMERALD_200 RGB(0xA7, 0xF3, 0xD0)
#define COL_EMERALD_300 RGB(0x6E, 0xE7, 0xB7)
#define COL_EMERALD_400 RGB(0x34, 0xD3, 0x99)
#define COL_EMERALD_500 RGB(0x10, 0xB9, 0x81)
#define COL_EMERALD_600 RGB(0x05, 0x96, 0x69)
#define COL_EMERALD_700 RGB(0x04, 0x78, 0x57)
#define COL_EMERALD_800 RGB(0x06, 0x5F, 0x46)
#define COL_EMERALD_900 RGB(0x06, 0x4E, 0x3B)

/* Teal */
#define COL_TEAL_50     RGB(0xF0, 0xFD, 0xFA)
#define COL_TEAL_100    RGB(0xCC, 0xFB, 0xF1)
#define COL_TEAL_200    RGB(0x99, 0xF6, 0xE4)
#define COL_TEAL_300    RGB(0x5E, 0xEA, 0xD4)
#define COL_TEAL_400    RGB(0x2D, 0xDA, 0xBF)
#define COL_TEAL_500    RGB(0x14, 0xB8, 0xA6)
#define COL_TEAL_600    RGB(0x0D, 0x94, 0x8D)
#define COL_TEAL_700    RGB(0x0F, 0x76, 0x6E)
#define COL_TEAL_800    RGB(0x11, 0x5E, 0x59)
#define COL_TEAL_900    RGB(0x13, 0x4E, 0x4A)

/* Cyan */
#define COL_CYAN_50     RGB(0xEC, 0xFE, 0xFF)
#define COL_CYAN_100    RGB(0xCF, 0xFE, 0xFF)
#define COL_CYAN_200    RGB(0xA5, 0xF3, 0xFC)
#define COL_CYAN_300    RGB(0x67, 0xE8, 0xF9)
#define COL_CYAN_400    RGB(0x22, 0xD3, 0xEE)
#define COL_CYAN_500    RGB(0x06, 0xB6, 0xD4)
#define COL_CYAN_600    RGB(0x08, 0x91, 0xB1)
#define COL_CYAN_700    RGB(0x0E, 0x74, 0x90)
#define COL_CYAN_800    RGB(0x15, 0x5E, 0x75)
#define COL_CYAN_900    RGB(0x16, 0x4E, 0x63)

/* Sky */
#define COL_SKY_50      RGB(0xF0, 0xF9, 0xFF)
#define COL_SKY_100     RGB(0xE0, 0xF2, 0xFE)
#define COL_SKY_200     RGB(0xBA, 0xE6, 0xFD)
#define COL_SKY_300     RGB(0x7D, 0xD3, 0xFC)
#define COL_SKY_400     RGB(0x38, 0xBD, 0xF8)
#define COL_SKY_500     RGB(0x0E, 0xA5, 0xE9)
#define COL_SKY_600     RGB(0x02, 0x86, 0xC7)
#define COL_SKY_700     RGB(0x03, 0x69, 0xA1)
#define COL_SKY_800     RGB(0x07, 0x58, 0x83)
#define COL_SKY_900     RGB(0x0C, 0x4A, 0x6E)

/* Blue */
#define COL_BLUE_50     RGB(0xEF, 0xF6, 0xFF)
#define COL_BLUE_100    RGB(0xDB, 0xEA, 0xFE)
#define COL_BLUE_200    RGB(0xBF, 0xDB, 0xFE)
#define COL_BLUE_300    RGB(0x93, 0xC5, 0xFD)
#define COL_BLUE_400    RGB(0x60, 0xA5, 0xFA)
#define COL_BLUE_500    RGB(0x3B, 0x82, 0xF6)
#define COL_BLUE_600    RGB(0x25, 0x63, 0xEB)
#define COL_BLUE_700    RGB(0x1D, 0x4E, 0xD8)
#define COL_BLUE_800    RGB(0x1E, 0x40, 0xAF)
#define COL_BLUE_900    RGB(0x1E, 0x3A, 0x8A)

/* Indigo */
#define COL_INDIGO_50   RGB(0xEE, 0xF2, 0xFF)
#define COL_INDIGO_100  RGB(0xE0, 0xE7, 0xFF)
#define COL_INDIGO_200  RGB(0xC7, 0xD2, 0xFE)
#define COL_INDIGO_300  RGB(0xA5, 0xB4, 0xFC)
#define COL_INDIGO_400  RGB(0x81, 0x8C, 0xF8)
#define COL_INDIGO_500  RGB(0x6F, 0x7C, 0xF7)
#define COL_INDIGO_600  RGB(0x4F, 0x46, 0xE5)
#define COL_INDIGO_700  RGB(0x43, 0x38, 0xCA)
#define COL_INDIGO_800  RGB(0x37, 0x30, 0xA3)
#define COL_INDIGO_900  RGB(0x31, 0x27, 0x82)

/* Violet */
#define COL_VIOLET_50   RGB(0xF5, 0xF3, 0xFF)
#define COL_VIOLET_100  RGB(0xED, 0xE9, 0xFE)
#define COL_VIOLET_200  RGB(0xDD, 0xD6, 0xFE)
#define COL_VIOLET_300  RGB(0xC4, 0xB5, 0xFD)
#define COL_VIOLET_400  RGB(0xA7, 0x8B, 0xFA)
#define COL_VIOLET_500  RGB(0x8B, 0x5C, 0xF6)
#define COL_VIOLET_600  RGB(0x7C, 0x3A, 0xED)
#define COL_VIOLET_700  RGB(0x6D, 0x28, 0xD9)
#define COL_VIOLET_800  RGB(0x5B, 0x21, 0xB6)
#define COL_VIOLET_900  RGB(0x4C, 0x1D, 0x95)

/* Purple */
#define COL_PURPLE_50   RGB(0xFA, 0xF5, 0xFF)
#define COL_PURPLE_100  RGB(0xF3, 0xE8, 0xFF)
#define COL_PURPLE_200  RGB(0xE9, 0xD5, 0xFF)
#define COL_PURPLE_300  RGB(0xD8, 0xB4, 0xFE)
#define COL_PURPLE_400  RGB(0xC0, 0x84, 0xFC)
#define COL_PURPLE_500  RGB(0xA8, 0x55, 0xF7)
#define COL_PURPLE_600  RGB(0x93, 0x33, 0xEA)
#define COL_PURPLE_700  RGB(0x7E, 0x22, 0xCE)
#define COL_PURPLE_800  RGB(0x6B, 0x21, 0xA8)
#define COL_PURPLE_900  RGB(0x58, 0x1C, 0x87)

/* Fuchsia */
#define COL_FUCHSIA_50  RGB(0xFD, 0xF4, 0xFF)
#define COL_FUCHSIA_100 RGB(0xFA, 0xE8, 0xFF)
#define COL_FUCHSIA_200 RGB(0xF5, 0xD0, 0xFE)
#define COL_FUCHSIA_300 RGB(0xF0, 0xAB, 0xFC)
#define COL_FUCHSIA_400 RGB(0xE8, 0x79, 0xF9)
#define COL_FUCHSIA_500 RGB(0xD9, 0x46, 0xEF)
#define COL_FUCHSIA_600 RGB(0xC0, 0x26, 0xD3)
#define COL_FUCHSIA_700 RGB(0xA2, 0x1C, 0xAF)
#define COL_FUCHSIA_800 RGB(0x86, 0x19, 0x8F)
#define COL_FUCHSIA_900 RGB(0x70, 0x1A, 0x75)

/* Pink */
#define COL_PINK_50     RGB(0xFD, 0xF2, 0xF8)
#define COL_PINK_100    RGB(0xFC, 0xE7, 0xF3)
#define COL_PINK_200    RGB(0xFC, 0xBC, 0xE7)
#define COL_PINK_300    RGB(0xF9, 0xA8, 0xD4)
#define COL_PINK_400    RGB(0xF4, 0x74, 0xB6)
#define COL_PINK_500    RGB(0xEC, 0x48, 0x99)
#define COL_PINK_600    RGB(0xDB, 0x27, 0x77)
#define COL_PINK_700    RGB(0xBE, 0x18, 0x5D)
#define COL_PINK_800    RGB(0x9D, 0x17, 0x4D)
#define COL_PINK_900    RGB(0x83, 0x19, 0x3F)

/* Rose */
#define COL_ROSE_50     RGB(0xFF, 0xF1, 0xF2)
#define COL_ROSE_100    RGB(0xFF, 0xE4, 0xE6)
#define COL_ROSE_200    RGB(0xFE, 0xCD, 0xD3)
#define COL_ROSE_300    RGB(0xFD, 0xA4, 0xAF)
#define COL_ROSE_400    RGB(0xFB, 0x71, 0x85)
#define COL_ROSE_500    RGB(0xF4, 0x3F, 0x5E)
#define COL_ROSE_600    RGB(0xE1, 0x1D, 0x48)
#define COL_ROSE_700    RGB(0xBE, 0x12, 0x3C)
#define COL_ROSE_800    RGB(0x9F, 0x12, 0x39)
#define COL_ROSE_900    RGB(0x88, 0x13, 0x33)

/* ============================================================
 * Additional Modern Colors (Fluent Design / Windows 11)
 * ============================================================ */
#define COL_FLUENT_BLUE     RGB(0x00, 0x5F, 0xB9)
#define COL_FLUENT_TEAL     RGB(0x00, 0xB7, 0xC3)
#define COL_FLUENT_GREEN    RGB(0x10, 0x7C, 0x10)
#define COL_FLUENT_ORANGE   RGB(0xCA, 0x50, 0x10)
#define COL_FLUENT_RED      RGB(0xC4, 0x2B, 0x1C)
#define COL_FLUENT_PURPLE   RGB(0x5C, 0x2D, 0x91)
#define COL_FLUENT_MAGENTA  RGB(0x88, 0x17, 0x98)

/* Mica material colors (Windows 11 style) */
#define COL_MICA_LIGHT      RGB(0xF3, 0xF3, 0xF3)
#define COL_MICA_DARK       RGB(0x20, 0x20, 0x20)
#define COL_MICA_BASE       RGB(0x2C, 0x2C, 0x2C)
#define COL_MICA_ALT        RGB(0x1C, 0x1C, 0x1C)
#define COL_MICA_ACCENT     RGB(0x00, 0x5F, 0xB9)

/* ============================================================
 * Color manipulation functions (integer math, no FPU needed)
 * All factor parameters use 0-256 scale (256 = 100%)
 * ============================================================ */

/* Lighten color: factor 0-256 (256 = no change, 128 = 50% lighter) */
uint32_t color_lighten(uint32_t color, uint32_t factor);

/* Darken color: factor 0-256 (256 = no change, 128 = 50% darker) */
uint32_t color_darken(uint32_t color, uint32_t factor);

/* Mix two colors: ratio 0-256 (0 = color1, 256 = color2) */
uint32_t color_mix(uint32_t color1, uint32_t color2, uint32_t ratio);

/* Alpha blend: src over dst with alpha 0-255 */
uint32_t color_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha);

/* Set alpha channel on a color */
uint32_t color_with_alpha(uint32_t color, uint8_t alpha);

/* Invert a color */
uint32_t color_invert(uint32_t color);

/* Grayscale conversion (luminance weighted) */
uint32_t color_to_grayscale(uint32_t color);

/* Get perceived luminance (0-255) */
uint8_t color_luminance(uint32_t color);

/* Pick black or white text for best contrast with given background */
uint32_t color_text_for_bg(uint32_t bg_color);

/* Tint a color toward white by amount 0-256 */
uint32_t color_tint(uint32_t color, uint32_t amount);

/* Shade a color toward black by amount 0-256 */
uint32_t color_shade(uint32_t color, uint32_t amount);

/* ============================================================
 * HSL (Hue, Saturation, Lightness) color space
 * ============================================================ */

typedef struct {
    uint16_t h;  /* 0-359 */
    uint8_t  s;  /* 0-100 */
    uint8_t  l;  /* 0-100 */
} hsl_color_t;

hsl_color_t rgb_to_hsl(uint32_t rgb);
uint32_t hsl_to_rgb(uint16_t h, uint8_t s, uint8_t l);

/* ============================================================
 * Gradient support
 * ============================================================ */

typedef enum {
    GRADIENT_HORIZONTAL = 0,
    GRADIENT_VERTICAL,
    GRADIENT_DIAGONAL
} gradient_dir_t;

/* Draw a gradient between two colors */
void gfx_draw_gradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t color1, uint32_t color2,
                       gradient_dir_t direction);

/* Interpolate between two colors at position t (0-256) */
uint32_t color_lerp(uint32_t color1, uint32_t color2, uint32_t t);

/* Multi-stop gradient: get color at position t (0-256) from stops array */
uint32_t gradient_sample(const uint32_t* stops, uint32_t count, uint32_t t);

#endif /* GUI_COLOR_H */
