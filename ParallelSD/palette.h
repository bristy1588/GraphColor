#ifndef _PALETTE_INCLUDED_H
#define _PALETTE_INCLUDED_H

/*
typedef struct palette {
  // TODO: Change int back to long long when debugging is done
  int *colors;
} palette;

*/
typedef struct palette{
  unsigned long long *colors;
} palette;

palette *init_palette(int num_colors);
bool set_color(palette *palette, int color);
int find_min(palette *palette);

#endif // _PALETTE_INCLUDED_H
