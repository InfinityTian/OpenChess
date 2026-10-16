#include "position.h"
#include "move.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void position_begin(PositionEditor *p,const Board *b) { memset(p,0,sizeof *p);p->board=*b;p->dragging=-1; }
static void push(Board *stack,int *count,const Board *b) {
    if(*count==POSITION_UNDO){memmove(stack,stack+1,(POSITION_UNDO-1)*sizeof *stack);(*count)--;}
    stack[(*count)++]=*b;
}
void position_commit(PositionEditor *p,const Board *b) {
    push(p->undo,&p->undo_count,&p->board);p->board=*b;p->redo_count=0;p->error[0]=0;
}
void position_place(PositionEditor *p,int sq,Piece pc) {
    if(sq<0||sq>63||pc<EMPTY||pc>BK)return;Board b=p->board;b.board[sq]=pc;position_commit(p,&b);
}
void position_undo(PositionEditor *p) {if(p->undo_count){push(p->redo,&p->redo_count,&p->board);p->board=p->undo[--p->undo_count];p->error[0]=0;}}
void position_redo(PositionEditor *p) {if(p->redo_count){push(p->undo,&p->undo_count,&p->board);p->board=p->redo[--p->redo_count];p->error[0]=0;}}
static bool invalid(char *e,const char *message){snprintf(e,160,"%s",message);return false;}
bool position_validate(const Board *b,char error[160]) {
    error[0]=0;int kings[2]={0},counts[2]={0},pawns[2]={0};
    for(int sq=0;sq<64;sq++){
        Piece pc=b->board[sq];if(pc==WK)kings[0]++;if(pc==BK)kings[1]++;
        if(pc!=EMPTY)counts[BLACK_PIECE(pc)?1:0]++;
        if(pc==WP)pawns[0]++;if(pc==BP)pawns[1]++;
        if((pc==WP||pc==BP)&&(sq<8||sq>=56))return invalid(error,"Pawns cannot be on rank 1 or rank 8.");
    }
    if(kings[0]!=1||kings[1]!=1)return invalid(error,"Exactly one king of each colour is required.");
    if(counts[0]>16||counts[1]>16)return invalid(error,"Each side can have at most 16 pieces.");
    if(pawns[0]>8||pawns[1]>8)return invalid(error,"Each side can have at most 8 pawns.");
    int wk=find_king(b,WHITE),bk=find_king(b,BLACK);
    if(abs(wk%8-bk%8)<=1&&abs(wk/8-bk/8)<=1)return invalid(error,"The kings cannot be adjacent.");
    if(in_check(b,b->side==WHITE?BLACK:WHITE))return invalid(error,"The side that just moved cannot remain in check.");
    static const int rook[]={7,0,63,56};
    for(int k=0;k<4;k++)if((b->castling&(1<<k))&&
       (b->board[k<2?4:60]!=(k<2?WK:BK)||b->board[rook[k]]!=(k<2?WR:BR)))
        return invalid(error,"Castling rights require a king and rook on their original squares.");
    if(b->ep_square>=0){int ep=b->ep_square;int pawn=b->side==WHITE?ep-8:ep+8;
        if(ep/8!=(b->side==WHITE?5:2)||b->board[ep]!=EMPTY||pawn<0||pawn>=64||b->board[pawn]!=(b->side==WHITE?BP:WP))
            return invalid(error,"The en-passant square is inconsistent with the position.");
    }
    return true;
}
