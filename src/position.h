#ifndef POSITION_H
#define POSITION_H
#include "board.h"
#define POSITION_UNDO 128
typedef struct {
    Board board;
    Board undo[POSITION_UNDO], redo[POSITION_UNDO];
    int undo_count, redo_count;
    Piece brush;
    int dragging;
    char error[160];
} PositionEditor;
void position_begin(PositionEditor *p, const Board *board);
void position_commit(PositionEditor *p, const Board *board);
void position_place(PositionEditor *p,int square,Piece piece);
void position_undo(PositionEditor *p);
void position_redo(PositionEditor *p);
bool position_validate(const Board *b,char error[160]);
#endif
