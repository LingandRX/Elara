
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif
#ifndef LV_FONT_SC_14
#define LV_FONT_SC_14 1
#endif
#if LV_FONT_SC_14
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = "\xff\xa0\xb6\xd0\x12\x12\x12\xff\x24\x24\x24\xff\x48\x48\x48\x21\x1d\x5a\xd1\x86\x29\x6b\x57\x10\x80\x61\x92\x92\x94\x68\x08\x16\x29\x29\x49\x86\x30\x48\x48\x48\x50\x22\x52\x8a\x84\x86\x79\xe0\x08\x88\x84\x42\x10\x82\x10\x41\x04\x82\x08\x21\x04\x21\x08\x84\x44\x40\x25\x5c\xea\x90\x10\x20\x47\xf1\x02\x04\x00\xf5\x80\xfe\xf0\x04\x10\x82\x10\x82\x10\x42\x08\x00\x38\x8a\x0c\x38\xb2\x68\xe1\x82\x88\xe0\x30\xa2\x40\x81\x02\x04\x08\x10\x23\xf8\x7a\x10\x41\x08\x42\x10\x82\x0f\xc0\xf0\x20\x82\x11\xc0\x81\x04\x2f\x00\x04\x0c\x14\x24\x24\x44\x84\xff\x04\x04\x04\xfe\x08\x20\xf0\x20\x41\x04\x2f\x00\x3c\x82\x04\x0b\xd8\x60\xc1\x82\x88\xe0\xfc\x10\x84\x10\x82\x10\x41\x04\x00\x7a\x18\x61\x48\xc4\xa1\x86\x17\x80\x38\x8a\x0c\x18\x30\xde\x81\x04\x09\xe0\xf0\x0f\xf0\x0f\x58\x03\x0c\x30\xc0\x30\x0c\x03\xfc\x0f\xc0\xc0\x30\x0c\x03\x0c\x30\xc0\x72\x28\x41\x08\x42\x08\x20\x02\x08\x3c\x42\x81\x9d\xa5\xa5\xa5\x9e\x80\x40\x3c\x18\x18\x18\x24\x24\x24\x42\x42\x7e\x81\x81\xfa\x18\x61\x87\xe8\x61\x86\x1f\x80\x3d\x08\x20\x82\x08\x20\x81\x03\xc0\xf2\x28\x61\x86\x18\x61\x86\x2f\x00\xfe\x08\x20\x83\xf8\x20\x82\x0f\xc0\xfe\x08\x20\x83\xf8\x20\x82\x08\x00\x3d\x08\x20\x82\x08\xe1\x85\x13\xc0\x86\x18\x61\x87\xf8\x61\x86\x18\x40\xf9\x08\x42\x10\x84\x21\x3e\x38\x42\x10\x84\x21\x0c\x7c\x85\x12\x44\x8a\x18\x28\x48\x89\x0a\x08\x84\x21\x08\x42\x10\x84\x3e\x83\x8f\x1e\x3a\xb5\x6a\xc9\x93\x06\x08\x87\x1c\x69\xa6\x99\x65\x8e\x38\x40\x31\x28\x61\x86\x18\x61\x85\x23\x00\xfa\x18\x61\x86\x2f\x20\x82\x08\x00\x30\x92\x14\x28\x50\xa1\x42\x84\x90\xc0\x81\x01\xc0\xf4\x63\x19\x72\x92\x94\x62\x7a\x08\x20\x40\xc0\x81\x04\x1f\x80\xfe\x20\x40\x81\x02\x04\x08\x10\x20\x40\x86\x18\x61\x86\x18\x61\x86\x17\x80\x83\x06\x0a\x24\x48\x8a\x14\x28\x20\x40\x93\x26\x4d\x5a\xb5\x5b\x22\x44\x89\x10\x85\x24\x8c\x30\xc3\x12\x4a\x18\x40\x83\x05\x11\x42\x82\x04\x08\x10\x20\x40\xfc\x10\x82\x10\x42\x10\x42\x0f\xc0\xf8\x88\x88\x88\x88\x88\x8f\x81\x01\x01\x02\x02\x04\x04\x08\x08\x08\xf1\x11\x11\x11\x11\x11\x1f\x10\x20\xa1\x44\x48\xa0\x80\xfe\x90\x3d\x18\x61\x86\x18\xdd\x82\x08\x2e\xc6\x18\x61\x86\x2f\x00\x3d\x08\x20\x82\x04\x0f\x04\x10\x4f\x46\x18\x61\x86\x37\x40\x39\x18\x7f\x82\x04\x0f\x1e\x40\x81\x0f\xc4\x08\x10\x20\x40\x80\x3d\x18\x61\x86\x18\xdd\x06\x27\x00\x82\x08\x2e\xc6\x18\x61\x86\x18\x40\x20\x01\xc2\x10\x84\x21\x0e\x08\x00\xf0\x84\x21\x08\x42\x11\x70\x84\x21\x19\x53\x18\xa4\xa2\xe1\x08\x42\x10\x84\x21\x0e\xb7\xb6\x4c\x99\x32\x64\xc9\xbb\x18\x61\x86\x18\x61\x31\x28\x61\x86\x14\x8c\xbb\x18\x61\x86\x18\xbc\x82\x08\x00\x3d\x18\x61\x86\x18\xdd\x04\x10\x40\xbe\x63\x08\x42\x10\x7a\x18\x18\x18\x18\x5e\x20\x82\x3f\x20\x82\x08\x20\x70\x86\x18\x61\x86\x18\xdd\x83\x05\x12\x24\x45\x0a\x08\x93\x26\xad\x5a\xb5\x51\x22\x8a\x94\x42\x2a\x31\x42\x84\x89\x21\x43\x02\x04\x10\x23\x80\xfc\x10\x84\x21\x08\x3f\x1c\x82\x08\x20\x8c\x08\x20\x82\x08\x20\x70\xff\xe0\xe0\x41\x04\x10\x40\xc4\x10\x41\x04\x13\x80\x66\xd9\x80\xfd\xf7\x8e\x30\x40\xff\xf0\x20\x04\x00\xd0\x1c\xc3\x46\xc4\x38\x40\x04\x00\x40\x04\x00\x40\x17\xf0\x80\x08\x00\x5f\xf6\x00\x33\xfb\x80\x04\xfe\x20\x01\x3f\x89\x04\x4f\xe2\x41\x00\x30\x81\x08\x09\x07\xfe\x04\x00\x40\x04\x0f\xff\x05\x00\x90\x18\x83\x06\xc0\x30\x00\x1f\xf1\x10\xd2\x4d\x26\xdf\xed\x10\xd1\x0d\xfe\xd1\x0d\x10\x13\xf1\xc0\x70\x00\x80\x11\xe2\x00\x40\x3f\xf9\x48\x29\x05\x69\xab\xa7\xdc\x83\x10\xcc\x3f\xc2\x04\x20\x41\xf8\xff\xf1\x00\x30\x03\xfc\x20\x40\x04\x00\xc0\x38\x00\x20\x1e\x3f\x02\x00\x3f\xf2\x00\x20\x02\xfe\x28\x22\x82\x48\x24\xfe\x88\x20\x02\x00\x90\x04\x80\x7f\xc2\x20\x01\x01\xff\xc0\xe0\x05\x80\x44\x06\x18\xc0\x70\x00\x00\x02\x00\x10\x1f\xfc\x80\x24\x01\x0f\xf0\x08\x02\x40\x13\xe1\x90\x0a\x80\xcf\xf0\x00\x00\x02\x00\x10\x1f\xfc\x88\x24\x25\x14\xd1\xac\xc9\x9a\x7f\x82\x10\x08\x88\x7f\xc2\x02\x00\x7f\xe1\x88\x18\x81\x88\xff\xf1\x88\x10\x81\x08\x10\x82\x08\x60\x88\x08\x01\x00\x0f\x00\x49\xff\xe0\x10\x3e\x91\x14\x88\xac\x7d\xc0\x04\x07\xf7\xe6\xa0\x63\x00\x22\x02\x2c\x22\x6f\xa0\x23\xf2\xf0\x29\x23\x94\xe1\xc2\x08\x23\x92\xc5\xe0\x30\x21\x82\x18\x2f\xff\x22\x23\x42\xff\x21\x03\x10\xef\xf2\x24\x27\xc2\x3e\x6c\x10\x10\x21\x0e\x91\x8b\x50\xb5\x0f\xff\xb1\x4b\x54\xd5\x4d\x14\x91\x4f\xe4\x82\x40\x00\x60\x06\xf0\x69\x7f\x90\x69\x06\xf2\x69\x16\x91\x69\x06\xf0\x69\x06\x00\xc0\x3f\xc2\x04\x3f\xc2\x04\x3f\xc0\x00\x49\x22\x96\x29\x40\x90\xff\xf0\x02\x00\x10\x0f\xf8\x04\x00\x20\x01\x01\xff\xc1\xe0\x0a\x81\x92\x18\x8c\x04\x20\x20\x00\x02\x06\x30\x09\xfc\x48\x60\x92\x0c\x90\x84\x04\x20\x22\x82\x34\x13\x19\x30\x70\x10\x02\x0f\x3e\x92\x89\x48\x94\x89\x7e\x90\x89\x18\x91\xc9\x32\xf6\x09\x40\x00\x03\xef\x82\x22\x22\x22\x7a\x2c\xbf\xc8\x14\x81\x4f\xd7\x81\x48\x10\x06\x3f\xe0\x00\x00\x00\xff\xe0\x20\x09\x20\x49\x84\x44\x62\x16\x10\xc0\x80\x1c\x00\x11\x07\x9f\x08\x88\x48\x8f\x3f\x1b\xf9\xc0\x4d\x7e\xa2\x81\x35\x8a\x8e\x47\xc2\x00\x00\x21\x01\x1f\x19\xc8\xaa\x8f\x08\x11\xa1\x18\xde\x7c\x02\x21\xd1\x10\xf8\x04\x40\xff\xf0\x06\x22\xd7\xd9\x33\x32\x6e\xad\x15\xe4\xb0\x06\x00\xc0\x70\x00\x0f\xa4\x52\x87\x7e\x51\x05\x10\x77\xe5\x18\x59\x8f\x2c\x92\x41\x46\x14\x20\x0c\x04\x3f\xf0\x18\x0f\xfe\x03\x01\xff\xc0\x60\x3f\xf8\x08\x60\x81\x84\x04\x20\x01\x0e\x08\x10\x40\x83\x04\x28\x21\x41\x59\x0d\x8c\x58\x30\x80\x00\x67\xf9\x06\x00\x30\x01\x8e\x6c\x13\x78\x9b\x04\xd8\x2e\xc1\xb6\x03\xfe\x40\x53\x02\x88\x10\x1f\xfe\x04\x70\x20\x9f\x04\x64\x23\x21\x1d\x09\xca\x60\x72\x01\x00\x43\x02\x18\x2f\xf0\x10\xe2\x62\xfc\x20\xa2\x32\x2e\x42\x0c\x29\xe3\x61\x00\x00\x47\xf1\x21\x89\x0c\x0f\xee\x00\x13\xf8\x82\x05\xff\x21\x41\x5b\x0d\x8c\x18\x30\x00\x83\xe4\x11\x20\xab\xe5\x52\x2a\xd1\x5a\x8a\x94\x54\x41\xc2\x0b\x29\x8a\x20\x00\x00\x30\x03\x3e\x7c\xa3\x12\x31\xaf\xf4\x13\xe5\xe2\x52\x25\x3e\xf2\x29\xff\x80\x00\x41\x03\x7f\x88\x80\x05\x0e\x7f\x10\x40\x82\x05\xff\x20\x81\x04\x16\x01\x0f\xf0\x00\x47\xf8\xff\xf0\x20\x3f\xc2\x24\x3f\xc2\x24\x3f\xc0\x20\x7f\xe0\x20\xff\xf0\x22\x42\x24\x7f\xe4\x24\x42\x4f\xff\xa0\x0f\xfe\x24\x22\x7e\x2c\x23\x7e\x04\x20";
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0,.adv_w = 0,.box_w = 0,.box_h = 0,.ofs_x = 0,.ofs_y = 0} ,
    {.bitmap_index = 0,.adv_w = 134,.box_w = 0,.box_h = 0,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 0,.adv_w = 134,.box_w = 1,.box_h = 11,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 2,.adv_w = 134,.box_w = 3,.box_h = 4,.ofs_x = 3,.ofs_y = 7},
    {.bitmap_index = 4,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 15,.adv_w = 134,.box_w = 5,.box_h = 15,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 25,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 36,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 47,.adv_w = 134,.box_w = 1,.box_h = 3,.ofs_x = 4,.ofs_y = 8},
    {.bitmap_index = 48,.adv_w = 134,.box_w = 5,.box_h = 14,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 57,.adv_w = 134,.box_w = 5,.box_h = 14,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 66,.adv_w = 134,.box_w = 5,.box_h = 6,.ofs_x = 1,.ofs_y = 5},
    {.bitmap_index = 70,.adv_w = 134,.box_w = 7,.box_h = 7,.ofs_x = 0,.ofs_y = 2},
    {.bitmap_index = 77,.adv_w = 134,.box_w = 2,.box_h = 5,.ofs_x = 4,.ofs_y = -3},
    {.bitmap_index = 79,.adv_w = 134,.box_w = 7,.box_h = 1,.ofs_x = 1,.ofs_y = 5},
    {.bitmap_index = 80,.adv_w = 134,.box_w = 2,.box_h = 2,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 81,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 90,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 100,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 110,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 119,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 128,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 139,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 148,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 158,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 167,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 176,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 186,.adv_w = 134,.box_w = 2,.box_h = 8,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 188,.adv_w = 134,.box_w = 2,.box_h = 11,.ofs_x = 3,.ofs_y = -3},
    {.bitmap_index = 191,.adv_w = 134,.box_w = 8,.box_h = 7,.ofs_x = 0,.ofs_y = 2},
    {.bitmap_index = 198,.adv_w = 134,.box_w = 6,.box_h = 3,.ofs_x = 1,.ofs_y = 4},
    {.bitmap_index = 201,.adv_w = 134,.box_w = 8,.box_h = 7,.ofs_x = 0,.ofs_y = 2},
    {.bitmap_index = 208,.adv_w = 134,.box_w = 6,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 217,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 228,.adv_w = 134,.box_w = 8,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 239,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 248,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 257,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 266,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 275,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 284,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 293,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 302,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 309,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 316,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 326,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 333,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 343,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 352,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 361,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 370,.adv_w = 134,.box_w = 7,.box_h = 14,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 383,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 390,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 399,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 409,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 418,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 428,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 438,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 447,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 457,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 466,.adv_w = 134,.box_w = 4,.box_h = 14,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 473,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 483,.adv_w = 134,.box_w = 4,.box_h = 14,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 490,.adv_w = 134,.box_w = 7,.box_h = 7,.ofs_x = 0,.ofs_y = 4},
    {.bitmap_index = 497,.adv_w = 134,.box_w = 7,.box_h = 1,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 498,.adv_w = 134,.box_w = 2,.box_h = 2,.ofs_x = 2,.ofs_y = 9},
    {.bitmap_index = 499,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 505,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 514,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 520,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 529,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 535,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 545,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 554,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 563,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 570,.adv_w = 134,.box_w = 5,.box_h = 14,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 579,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 586,.adv_w = 134,.box_w = 5,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 593,.adv_w = 134,.box_w = 7,.box_h = 8,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 600,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 606,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 612,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 621,.adv_w = 134,.box_w = 6,.box_h = 11,.ofs_x = 1,.ofs_y = -3},
    {.bitmap_index = 630,.adv_w = 134,.box_w = 5,.box_h = 8,.ofs_x = 2,.ofs_y = 0},
    {.bitmap_index = 635,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 641,.adv_w = 134,.box_w = 6,.box_h = 10,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 649,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 655,.adv_w = 134,.box_w = 7,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 662,.adv_w = 134,.box_w = 7,.box_h = 8,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 669,.adv_w = 134,.box_w = 5,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 674,.adv_w = 134,.box_w = 7,.box_h = 11,.ofs_x = 0,.ofs_y = -3},
    {.bitmap_index = 684,.adv_w = 134,.box_w = 6,.box_h = 8,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 690,.adv_w = 134,.box_w = 6,.box_h = 14,.ofs_x = 2,.ofs_y = -2},
    {.bitmap_index = 701,.adv_w = 134,.box_w = 1,.box_h = 11,.ofs_x = 3,.ofs_y = 0},
    {.bitmap_index = 703,.adv_w = 134,.box_w = 6,.box_h = 14,.ofs_x = 1,.ofs_y = -2},
    {.bitmap_index = 714,.adv_w = 134,.box_w = 6,.box_h = 3,.ofs_x = 1,.ofs_y = 3},
    {.bitmap_index = 717,.adv_w = 134,.box_w = 6,.box_h = 6,.ofs_x = 1,.ofs_y = 2},
    {.bitmap_index = 722,.adv_w = 224,.box_w = 12,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 740,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 762,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 782,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 802,.adv_w = 224,.box_w = 11,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 820,.adv_w = 224,.box_w = 12,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 838,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 858,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 880,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 902,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 924,.adv_w = 224,.box_w = 12,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 942,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 964,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 984,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1004,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1024,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1044,.adv_w = 224,.box_w = 12,.box_h = 11,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 1061,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1083,.adv_w = 224,.box_w = 13,.box_h = 12,.ofs_x = 1,.ofs_y = 0},
    {.bitmap_index = 1103,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1123,.adv_w = 224,.box_w = 12,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1141,.adv_w = 224,.box_w = 13,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1161,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1183,.adv_w = 224,.box_w = 13,.box_h = 12,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1203,.adv_w = 224,.box_w = 11,.box_h = 12,.ofs_x = 2,.ofs_y = -1},
    {.bitmap_index = 1220,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1240,.adv_w = 224,.box_w = 9,.box_h = 13,.ofs_x = 3,.ofs_y = -1},
    {.bitmap_index = 1255,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1277,.adv_w = 224,.box_w = 13,.box_h = 11,.ofs_x = 0,.ofs_y = 0},
    {.bitmap_index = 1295,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1317,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1337,.adv_w = 224,.box_w = 13,.box_h = 12,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 1357,.adv_w = 224,.box_w = 13,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1379,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1399,.adv_w = 224,.box_w = 13,.box_h = 12,.ofs_x = 0,.ofs_y = -1},
    {.bitmap_index = 1419,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1},
    {.bitmap_index = 1439,.adv_w = 224,.box_w = 12,.box_h = 13,.ofs_x = 1,.ofs_y = -1}
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
const lv_font_t lv_font_sc_14 = {
#else
lv_font_t lv_font_sc_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 16,
    .base_line = 3,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7,4,0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,
#if LV_VERSION_CHECK(8,2,0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};
#endif 
