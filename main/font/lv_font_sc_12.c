
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif
#ifndef LV_FONT_SC_12
#define LV_FONT_SC_12 1
#endif
#if LV_FONT_SC_12
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = "\xfd\x80\xb6\x80\x14\x29\xf9\x4f\xca\x14\x00\x23\xab\x56\x18\xb5\xab\x88\x45\x4a\xa2\x41\x04\x8a\xa5\x44\x62\x49\x28\x42\x89\x62\x74\xe0\x29\x49\x24\x48\x80\x89\x12\x49\x4a\x00\x25\x5c\xea\x90\x21\x3e\x42\x00\xf6\xf8\xf0\x04\x10\x82\x10\x42\x08\x41\x08\x20\x74\x63\x3a\xe6\x31\x70\x59\x24\x92\xe0\x74\x42\x11\x11\x10\xf8\x74\x42\x60\x84\x31\x70\x10\xc5\x24\xfc\x41\x04\x10\xfc\x21\xe0\x84\x31\x70\x32\x21\xe8\xc6\x31\x70\xf8\x42\x22\x10\x84\x20\x74\x62\xe8\xc6\x31\x70\x74\x63\x18\xbc\x22\x60\xf0\xf0\xf0\xf6\x12\x48\x42\x10\xf8\x3e\x84\x21\x24\x80\x74\x42\x22\x10\x04\x20\x7a\x1b\x75\xd6\xe8\x1e\x74\x63\x1f\xc6\x31\x88\xf4\x63\xe8\xc6\x31\xf0\x74\x63\x08\x42\x11\x70\xf4\x63\x18\xc6\x31\xf0\xfc\x21\x0f\x42\x10\xf8\xfc\x21\x0f\x42\x10\x80\x74\x61\x09\xc6\x31\x70\x8c\x63\x1f\xc6\x31\x88\xff\x80\x08\x42\x10\x86\x31\x70\x8c\xa9\x88\x62\x92\x88\x84\x21\x08\x42\x10\xf8\x8e\xeb\x18\xc6\x31\x88\x8c\x73\x59\xc6\x31\x88\x74\x63\x18\xc6\x31\x70\xf4\x63\x1f\x42\x10\x80\x74\x63\x18\xc6\x31\x71\x04\xf4\x63\x1f\x46\x31\x88\x74\x61\x07\x04\x31\x70\xf9\x08\x42\x10\x84\x20\x8c\x63\x18\xc6\x31\x70\x83\x05\x12\x24\x45\x0a\x08\x10\x8c\x63\x18\xd7\x71\x88\x8c\x62\xa2\x2a\x31\x88\x8c\x63\x15\x10\x84\x20\xf8\x42\x22\x22\x10\xf8\xf2\x49\x24\x93\x80\x82\x04\x10\x20\x81\x04\x08\x20\x41\xe4\x92\x49\x27\x80\x22\xa2\xfe\x88\x80\x7c\x63\x19\xb4\x84\x21\xe8\xc6\x31\xf0\x74\x61\x08\xb8\x08\x42\xf8\xc6\x31\x78\x74\x7f\x08\xb8\x34\x4e\x44\x44\x40\x7c\x63\x18\xbc\x31\x70\x84\x21\xe8\xc6\x31\x88\xdf\x80\x24\x12\x49\x24\xe0\x84\x21\x2a\x62\x92\x88\xaa\xaa\xc0\xfd\x6b\x5a\xd4\xf4\x63\x18\xc4\x74\x63\x18\xb8\xf4\x63\x18\xfa\x10\x7c\x63\x18\xbc\x21\xb6\x63\x08\x40\x74\x5c\x18\xb8\x44\xe4\x44\x43\x8c\x63\x19\xb4\x8c\x54\xa2\x10\xad\x6b\x5a\xa8\x8a\x88\x45\x44\x8c\x63\x18\xbc\x31\x70\xf8\x88\x88\x7c\x29\x25\x12\x48\x80\xff\xfe\x89\x24\x52\x4a\x00\x6d\x80\x7b\x8c\x40\xff\xc1\x00\x80\x68\x29\xb2\x30\x80\x20\x08\x02\x00\x1f\xc6\x00\xff\x90\x06\xfd\xc0\x0b\xf1\x7e\x28\x45\xf8\xa1\x00\x00\x08\x81\x21\xfe\x08\x02\x0f\xfc\x30\x12\x0c\x4c\x0c\x00\x7f\x92\x14\xb5\xfd\x44\x51\x15\xe5\x10\x5f\x1c\x0c\x02\x3e\x80\x20\x1f\xfa\x48\x94\x25\x29\xfc\xc3\x21\xb8\x3f\x08\x43\xf0\x00\xff\xcf\xc2\x10\x04\x01\x01\xc0\x01\xc7\xc0\x80\x1f\xf2\x00\x40\x0b\xf1\x42\x28\x49\xf9\x21\x00\x08\x14\x1f\xc9\x08\x87\xfc\x30\x28\x22\x60\xe0\x20\x04\x07\xfe\x40\x24\x02\x3f\xc2\x40\x27\xc2\x40\x34\x04\x7e\x00\x00\x04\x3f\xf8\x86\xd5\x56\xa6\x5f\xe2\x10\x44\x9f\xe4\x08\xff\xc4\x81\x23\xff\x12\x04\x82\x20\x88\x42\x20\x80\x05\x00\x93\xff\x01\x07\xa8\x95\x12\xa3\xd8\x05\x1f\x78\x33\x00\x24\x09\x2f\x40\x9f\x2a\x0c\xae\x38\x8c\x27\x4e\x7c\x0c\x21\x05\xfd\xd1\x11\x42\xfe\x66\x3b\xf9\x34\x27\x84\x79\xb0\x80\x10\xe4\xcb\x62\xc8\xff\xee\xaf\x6b\x4a\x92\xbf\x28\x48\x00\x80\x2e\x0a\xbf\xa0\xb9\x2a\x2a\x82\xe0\xa8\x20\x18\x3f\x08\x43\xf0\xfc\x12\x14\xa3\x38\xcc\xff\xc0\x04\x00\x80\xfe\x02\x07\xfc\x1c\x05\x41\xac\x64\xc0\x88\x10\x00\x04\x18\x81\xbf\x04\x21\xa4\x24\x08\x82\x38\x45\x19\x12\xc1\x80\x40\x23\xdf\x34\x9a\x4f\xe4\x92\x69\x4f\xe2\x60\x00\xff\x91\x24\x49\xe2\xdf\xf4\x15\xf5\xc1\x50\x40\x60\x7f\x00\x0f\xf8\x20\x2a\x12\x44\x8a\x22\x08\x0e\x00\x12\x1c\xf0\xb2\x3b\xe2\xfc\xe0\xbb\xf1\x6e\x2c\xe4\xf0\x80\x00\x24\x11\xe4\xea\xac\xe3\x13\x3f\x7a\x12\x34\xb1\xe0\x48\xff\xc9\x76\xb6\xd9\x2d\x77\xa7\x11\x80\xc0\xc0\xfa\x8e\xfd\x44\x28\x87\x7e\xa3\x16\x67\x92\x16\x42\x84\x08\x23\xfc\x1f\xf0\x7f\xc1\x83\xfe\x08\x61\x02\x20\x04\x70\x82\x10\x43\x08\xa1\x54\x34\x45\x04\x00\x00\xdf\xd0\x80\x23\x28\x4b\xd2\x85\xa1\xa8\x3f\xc0\x40\x8c\x14\x82\x87\xfe\x08\x5f\x08\xa1\x14\x27\xa6\x0c\x81\x80\x00\x0c\x60\xbf\x81\x0e\x64\x5f\x08\x49\x32\x28\x84\x38\xd8\x84\x00\xcf\x8d\x10\x3e\x60\x04\xfc\xbf\xd0\xc3\xa4\x58\x40\x00\x01\x0f\x21\xa7\x35\x26\xa4\xda\x9a\x63\x4c\x30\x8d\x2b\x19\x80\x20\x1f\xf8\x89\x11\xaf\xd8\x4f\x9f\x13\x22\xe7\xd3\xfc\x00\x00\x04\x0c\x80\xbf\x04\x86\xfc\x42\x0f\xf9\x08\x21\x0a\x00\x3f\x80\x00\x4f\xf3\xff\x82\x03\xf8\x7f\x09\x20\xf8\x7f\xc0\x83\xff\x80\x44\x88\x91\xff\x22\x4f\xff\x40\x3f\xf1\x42\x3f\xc5\xf8\x21\x00";
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0,.adv_w = 0,.box_w = 0,.box_h = 0,.ofs_x = 0,.ofs_y = 0} ,
    {.bitmap_index = 0,.adv_w = 115,.box_w = 0,.box_h = 0,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 0,.adv_w = 115,.box_w = 1,.box_h = 9,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 2,.adv_w = 115,.box_w = 3,.box_h = 3,.ofs_x = 2,.ofs_y = 6},
    {.bitmap_index = 4,.adv_w = 115,.box_w = 7,.box_h = 7,.ofs_x = 0,.ofs_y = 3},
    {.bitmap_index = 11,.adv_w = 115,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 18,.adv_w = 115,.box_w = 7,.box_h = 9,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 26,.adv_w = 115,.box_w = 6,.box_h = 9,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 33,.adv_w = 115,.box_w = 1,.box_h = 3,.ofs_x = 3,.ofs_y = 6},
    {.bitmap_index = 34,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 39,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 44,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 3},
    {.bitmap_index = 48,.adv_w = 115,.box_w = 5,.box_h = 5,.ofs_x = 1,.ofs_y = 2},
    {.bitmap_index = 52,.adv_w = 115,.box_w = 2,.box_h = 4,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 53,.adv_w = 115,.box_w = 5,.box_h = 1,.ofs_x = 1,.ofs_y = 4},
    {.bitmap_index = 54,.adv_w = 115,.box_w = 2,.box_h = 2,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 55,.adv_w = 115,.box_w = 6,.box_h = 12,.ofs_x = 0,.ofs_y = -2},
    {.bitmap_index = 64,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 70,.adv_w = 115,.box_w = 3,.box_h = 9,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 74,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 80,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 86,.adv_w = 115,.box_w = 6,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 93,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 99,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 105,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 111,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 117,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 123,.adv_w = 115,.box_w = 2,.box_h = 6,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 125,.adv_w = 115,.box_w = 2,.box_h = 8,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 127,.adv_w = 115,.box_w = 4,.box_h = 7,.ofs_x = 1,.ofs_y = 1},
    {.bitmap_index = 131,.adv_w = 115,.box_w = 5,.box_h = 3,.ofs_x = 1,.ofs_y = 3},
    {.bitmap_index = 133,.adv_w = 115,.box_w = 4,.box_h = 7,.ofs_x = 2,.ofs_y = 1},
    {.bitmap_index = 137,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 143,.adv_w = 115,.box_w = 6,.box_h = 8,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 149,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 155,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 161,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 167,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 173,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 179,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 185,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 191,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 197,.adv_w = 115,.box_w = 1,.box_h = 9,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 199,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 205,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 211,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 217,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 223,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 229,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 235,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 241,.adv_w = 115,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 248,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 254,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 260,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 266,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 272,.adv_w = 115,.box_w = 7,.box_h = 9,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 280,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 286,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 292,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 298,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 304,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 309,.adv_w = 115,.box_w = 6,.box_h = 12,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 318,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 323,.adv_w = 115,.box_w = 5,.box_h = 3,.ofs_x = 1,.ofs_y = 6},
    {.bitmap_index = 325,.adv_w = 115,.box_w = 7,.box_h = 1,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 326,.adv_w = 115,.box_w = 3,.box_h = 3,.ofs_x = 2,.ofs_y = 7},
    {.bitmap_index = 328,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 332,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 338,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 342,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 348,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 352,.adv_w = 115,.box_w = 4,.box_h = 9,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 357,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 363,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 369,.adv_w = 115,.box_w = 1,.box_h = 9,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 371,.adv_w = 115,.box_w = 3,.box_h = 12,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 376,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 382,.adv_w = 115,.box_w = 2,.box_h = 9,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 385,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 389,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 393,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 397,.adv_w = 115,.box_w = 5,.box_h = 8,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 402,.adv_w = 115,.box_w = 5,.box_h = 8,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 407,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 411,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 415,.adv_w = 115,.box_w = 4,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 419,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 423,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 427,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 431,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 435,.adv_w = 115,.box_w = 5,.box_h = 9,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 441,.adv_w = 115,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 445,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 450,.adv_w = 115,.box_w = 1,.box_h = 15,.ofs_x = 3,.ofs_y = -3},
    {.bitmap_index = 452,.adv_w = 115,.box_w = 3,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 457,.adv_w = 115,.box_w = 5,.box_h = 2,.ofs_x = 1,.ofs_y = 7},
    {.bitmap_index = 459,.adv_w = 115,.box_w = 5,.box_h = 4,.ofs_x = 1,.ofs_y = 2},
    {.bitmap_index = 462,.adv_w = 192,.box_w = 10,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 475,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 491,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 505,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 519,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 533,.adv_w = 192,.box_w = 10,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 546,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 562,.adv_w = 192,.box_w = 9,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 575,.adv_w = 192,.box_w = 12,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 592,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 606,.adv_w = 192,.box_w = 10,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 619,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 635,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 649,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 665,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 679,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 693,.adv_w = 192,.box_w = 10,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 705,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 721,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 737,.adv_w = 192,.box_w = 9,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 750,.adv_w = 192,.box_w = 10,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 763,.adv_w = 192,.box_w = 10,.box_h = 10,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 776,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 792,.adv_w = 192,.box_w = 10,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 806,.adv_w = 192,.box_w = 9,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 818,.adv_w = 192,.box_w = 11,.box_h = 10,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 832,.adv_w = 192,.box_w = 7,.box_h = 11,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 842,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 858,.adv_w = 192,.box_w = 10,.box_h = 9,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 870,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 886,.adv_w = 192,.box_w = 11,.box_h = 12,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 903,.adv_w = 192,.box_w = 11,.box_h = 10,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 917,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 933,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 949,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 965,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 981,.adv_w = 192,.box_w = 11,.box_h = 11,.ofs_x = 1,.ofs_y = -1}
};
static const uint16_t unicode_list_1[] = {
    0,10321,10789,11191,11380,11500,11835,11858,
    13173,13790,13834,14660,15450,15554,15849,16369,
    16442,16514,16750,17765,21033,21061,21374,21623,
    22816,22933,23192,23598,26088,26117,26137,26153,
    26163,26473,26569,27170,27665,28509
};
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32,.range_length = 95,.glyph_id_start = 1,
        .unicode_list = NULL,.glyph_id_ofs_list = NULL,.list_length = 0,.type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
},
    {
        .range_start = 9660,.range_length = 28510,.glyph_id_start = 96,
        .unicode_list = unicode_list_1,.glyph_id_ofs_list = NULL,.list_length = 38,.type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
}
};
#if LVGL_VERSION_MAJOR == 8
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif
#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_sc_12 = {
#else
lv_font_t lv_font_sc_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 15,
    .base_line = 3,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7,4,0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,
#if LV_VERSION_CHECK(8,2,0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};
#endif 
