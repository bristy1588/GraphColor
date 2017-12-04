#include "palette.h"
#include "stdlib.h"
#include <stdio.h>
#include <iostream>
using namespace std;

//unsigned long long MAX = 18446744073709551615ull;
bool P_DEBUG = false;
/*
palette *init_palette(int num_colors) {
  palette *new_palette = (palette *) malloc(sizeof(palette));
  new_palette->colors = (int *) malloc(num_colors * sizeof(int));
  for (int i = 0; i < num_colors; i++)
    new_palette->colors[i] = 0;
  return new_palette;
}

bool set_color(palette *palette, int color){
  if (P_DEBUG) printf("Entering set Color\n");
  bool result = (palette->colors[color] == 0);
  palette->colors[color] = 1;
  if (P_DEBUG) printf("Exiting Set Color\n");
  return result;
}

// Make this parallel 
int find_min(palette *palette){
  int i = 0;
  while(palette->colors[i] == 1){
    i++;
  }
  return i;
}

*/
palette *init_palette(int num_colors){
  palette *new_palette = (palette *) malloc(sizeof(palette));
  new_palette-> colors = (unsigned long long *) malloc( (num_colors/64 + 1) *sizeof(unsigned long long));
  for (int i = 0; i <= num_colors/64; i++){ 
    new_palette->colors[i]= 0ull;
    //if (new_palette->colors[i] != 0ull) printf("Not initialized to Zero properly\n");
   }
  // cout<< "MAX:: " << MAX<<endl;
  return new_palette;
}

static bool helper_set_color(palette *palette, int i, int j) {
// cout<< "Setting i: " << i << " j: " << j << " Before: " <<palette->colors[i]<<endl;
 if (palette->colors[i] & (1ull << j)) {
    return false;
  } else {
    palette->colors[i] |= (1ull << j); 
//    cout<<"After:" <<palette->colors[i]<<endl;
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
  unsigned long long MAX = 18446744073709551615ull;
  while (true) {
    i++;
    if (palette->colors[i] == MAX) continue;
    else {
//      int min_col = i * 64 + __builtin_ctzll(palette->colors[i] ^ MAX);
      //cout << " i: " << i << " Palette :"<<  palette->colors[i] << " min col " <<min_col <<endl;
      int min_col = i*64 + 64;
      return min_col;
    }
  }
}

/*
int main(){

  palette* pal = init_palette(500);

  for(int i = 0; i < 67; i++){
    set_color(pal, i);
  }
  cout <<pal->colors[0] << "  " << pal->colors[1]<< " "<< pal->colors[2] <<endl; 
  cout << find_min(pal);
  
  return 0;
}
*/
