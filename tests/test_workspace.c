#include "../src/workspace.h"
#include "../src/position.h"
#include "../src/fen.h"
#include "../src/move.h"
#include "../server/community.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);failures++;}}while(0)
static int failures;
static int integer(cJSON *j,const char *k){cJSON *v=cJSON_GetObjectItem(j,k);return cJSON_IsNumber(v)?v->valueint:0;}
int main(void)
{
    CHECK(workspace_day("2026-10-16")>0);CHECK(workspace_weekday(workspace_day("2026-10-16"))==4);
    CHECK(workspace_day("2025-02-29")==-1);CHECK(workspace_day("2024-02-29")>0);CHECK(workspace_day("2026-02-30")==-1);
    char date[11];workspace_date(workspace_day("2026-12-31")+1,date);CHECK(!strcmp(date,"2027-01-01"));
    CHECK(workspace_time_class(60000,0)==0);CHECK(workspace_time_class(300000,3000)==1);CHECK(workspace_time_class(900000,10000)==2);CHECK(workspace_time_class(1800000,0)==3);
    Board b;board_reset(&b);char err[160];CHECK(position_validate(&b,err));
    PositionEditor e;position_begin(&e,&b);position_place(&e,0,EMPTY);CHECK(e.board.board[0]==EMPTY);position_undo(&e);CHECK(e.board.board[0]==WR);position_redo(&e);CHECK(e.board.board[0]==EMPTY);
    b.board[4]=EMPTY;CHECK(!position_validate(&b,err));board_reset(&b);b.ep_square=20;CHECK(!position_validate(&b,err));
    CHECK(fen_parse("4k3/8/8/8/8/8/8/4K3 w - - 0 9999",&b));CHECK(b.fullmove_number==9999);char fen[128];fen_generate(&b,fen,sizeof fen);CHECK(strstr(fen,"9999")!=NULL);
    Workspace *w=workspace_open(":memory:");CHECK(w!=NULL);if(!w)return 1;
    workspace_profile(w,"alice",1610);workspace_profile(w,"alice",1500);CHECK(workspace_rating(w,"alice",0)==1610);CHECK(workspace_rating(w,"alice",4)==1610);
    Puzzles *p=puzzles_load("assets/puzzles/starter.jsonl");CHECK(p!=NULL);if(!p)return 1;
    int valid=0,bad=0;
    for(int k=0;k<puzzles_count(p);k++){Puzzle puzzle;puzzles_get(p,k,&puzzle);Board board;if(!fen_parse(puzzle.fen,&board)){bad++;continue;}
        char line[256];snprintf(line,sizeof line,"%s",puzzle.moves);char *save=NULL;bool ok=true;
        for(char *t=strtok_r(line," ",&save);t;t=strtok_r(NULL," ",&save)){Move m;if(!uci_to_move(&board,t,&m)){ok=false;break;}make_move_plumb(&board,m);}
        if(ok)valid++;else {bad++;fprintf(stderr,"Invalid bundled puzzle: %s\n",puzzle.id);}
    }CHECK(bad==0);printf("Bundled puzzle legal lines: %d; invalid: %d\n",valid,bad);
    int start=workspace_day("2026-10-12"),end=start+6;
    CHECK(workspace_daily_fill(w,p,start));CHECK(workspace_daily_fill(w,p,end));
    cJSON *cal=workspace_calendar(w,"alice",start,end);CHECK(cJSON_GetArraySize(cal)==7);cJSON_Delete(cal);
    int lows[]={600,800,1000,1200,1400,1600,1900},highs[]={799,999,1199,1399,1599,1899,2399};
    for(int k=0;k<7;k++){Puzzle puzzle;CHECK(workspace_daily(w,start+k,&puzzle));CHECK(puzzle.rating>=lows[k]&&puzzle.rating<=highs[k]);
        char id[32];snprintf(id,sizeof id,"daily-%d",k);CHECK(workspace_issue(w,"alice",id,&puzzle,true,start+k));
        cJSON *result=workspace_submit(w,"alice",id,puzzle.moves,k==1,false,1200);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"solved")));CHECK(integer(result,"delta")==0);cJSON_Delete(result);
        result=workspace_submit(w,"alice",id,"",false,true,10);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"solved")));cJSON_Delete(result);
    }
    CHECK(workspace_rating(w,"alice",4)==1610);int current,longest;workspace_streak(w,"alice",end,&current,&longest);CHECK(current==7&&longest==7);
    Puzzle puzzle;CHECK(workspace_daily(w,end,&puzzle));CHECK(workspace_daily_fill(w,p,end));Puzzle same;CHECK(workspace_daily(w,end,&same));CHECK(!strcmp(puzzle.id,same.id));
    CHECK(workspace_issue(w,"bob","reveal",&puzzle,false,end));cJSON *result=workspace_submit(w,"bob","reveal",puzzle.moves,false,true,100);CHECK(!cJSON_IsTrue(cJSON_GetObjectItem(result,"solved")));cJSON_Delete(result);workspace_streak(w,"bob",end,&current,&longest);CHECK(current==0&&longest==0);
    workspace_profile(w,"bob",1500);CHECK(workspace_issue(w,"bob","rated",&puzzle,true,0));result=workspace_submit(w,"bob","rated",puzzle.moves,false,false,100);CHECK(integer(result,"delta")>0);int after=workspace_rating(w,"bob",4);cJSON_Delete(result);result=workspace_submit(w,"bob","rated","",false,true,100);CHECK(workspace_rating(w,"bob",4)==after);cJSON_Delete(result);
    char reason[128];CHECK(workspace_pick(w,p,"alice",1500,true,NULL,&puzzle,reason));
    sqlite3_exec(w->db,"CREATE TABLE users(id INTEGER PRIMARY KEY,name TEXT);INSERT INTO users VALUES(1,'alice'),(2,'bob'),(3,'eve');",NULL,NULL,NULL);
    cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"op","friend.action");cJSON_AddNumberToObject(q,"peer",2);cJSON_AddStringToObject(q,"action","request");result=community_request(w,p,1,"alice",q);cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"friend.action\",\"peer\":1,\"action\":\"accept\"}");result=community_request(w,p,2,"bob",q);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"message.send\",\"peer\":2,\"text\":\"你好，朋友\"}");result=community_request(w,p,1,"alice",q);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(result,"rows"))==1);cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"messages\",\"peer\":2}");result=community_request(w,p,3,"eve",q);CHECK(cJSON_IsFalse(cJSON_GetObjectItem(result,"ok")));cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"friend.action\",\"peer\":1,\"action\":\"block\"}");result=community_request(w,p,2,"bob",q);cJSON_Delete(result);cJSON_Delete(q);CHECK(community_blocked(w,1,2));
    puzzles_free(p);workspace_close(w);printf("Workspace, daily archive, immutable scoring, social permissions and setup: %s\n",failures?"FAILED":"PASSED");return failures!=0;
}
