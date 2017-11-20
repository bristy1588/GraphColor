#include "palette.h"
#include "stdlib.h"

long long MAX = 0xFFFFFFFF;

palette *init_palette(int num_colors) {
  palette *new_palette = (palette *) malloc(sizeof(palette));
  new_palette->colors = (long long *) malloc(((num_colors - 1) / 64 + 1) * sizeof(long long));
  return new_palette;
}

static bool helper_set_color(palette *palette, int i, int j) {
  if (palette->colors[i] & (1 << j)) {
    return false;
  } else {
    palette->colors[i] |= (1 << j);
    return true;
  }
}

bool set_color(palette *palette, int color) {
  if (color < 64) {
    return helper_set_color(palette, 0, color);
  } else if (color < 128) {
    return helper_set_color(palette, 1, color - 64);
  } else {
    return helper_set_color(palette, color / 64, color % 64);
  }
}

int find_min(palette *palette) {
  int i = -1;
  while (true) {
    i++;
    if (palette->colors[i] == MAX) {
      continue;
    } else {
      return __builtin_ctz(palette->colors[i] ^ MAX);
    }
  }
}
