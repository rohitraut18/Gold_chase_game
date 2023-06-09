
#ifndef MAP_H
#define MAP_H

#include<ncurses.h>
#include<panel.h>
#include "Screen.h"

/////
// The Map class uses the Screen class to paint a map
/////
class Map {
  public:
    Map(const unsigned char* mapmem, int l, int w);
    void drawMap();
    void postNotice(const char* msg);
    int getKey();
    unsigned int getPlayer(unsigned int PlayerMask);
    std::string getMessage();
  private:
    int num_player_bits(unsigned char ch);
    unsigned char operator()(int y, int x);
    Screen theMap;
    const unsigned char* mapmem;
    int mapHeight;
    int mapWidth;
};

#endif //MAP_H
