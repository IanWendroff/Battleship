#ifndef MESSAGE_H
#define MESSAGE_H

struct Cell {
    bool been_shot = false;
    int ship_connection = -1;
};

struct Ship {
    int hp = 5;
    int offsets[6][2] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1}};
    int maxRow = -1;
    int maxCol = -1;
};

struct Shot {
    int boardID = -1;
    int col = -1;
    int row = -1;
};

#endif