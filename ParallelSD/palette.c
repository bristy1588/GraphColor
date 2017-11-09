typedef struct palette {
  uint64_t *colors;
} palette;

uint64_t MAX = 0xFFFFFFFF;

palette *init_palette(int num_colors) {
  palette *palette = malloc(sizeof(palette *));
  palette->colors = malloc(((num_colors - 1) / 64 + 1) * sizeof(uint64_t));
  return palette;
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
    helper_set_color(palette, 1, color - 64);
  } else {
    helper_set_color(palette, color / 64, color % 64);
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
