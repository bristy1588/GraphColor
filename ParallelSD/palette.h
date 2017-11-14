#ifndef _PALETTE_INCLUDED_H
#define _PALETTE_INCLUDED_H

typedef struct palette {
  long long *colors;
} palette;

palette *init_palette(int num_colors);
bool set_color(palette *palette, int color);
int find_min(palette *palette);

#endif // _PALETTE_INCLUDED_H
