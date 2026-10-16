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
    /* Relative selection and settlement must agree locally and online. */
    int previous_gain=-1,previous_loss=-100;
    for(int level=0;level<5;level++){
        int target=1500+(level-2)*300;Puzzle chosen;
        for(int sample=0;sample<25;sample++){CHECK(workspace_pick_level(p,1500,level,NULL,&chosen));CHECK(chosen.rating>=target-150&&chosen.rating<=target+149);}
        chosen.rating=target; /* Fix the rating to test settlement independently of sampling. */
        char owner[32];snprintf(owner,sizeof owner,"win-level-%d",level);workspace_profile(w,owner,1500);
        CHECK(workspace_issue(w,owner,"win",&chosen,true,0));cJSON *settled=workspace_submit(w,owner,"win",chosen.moves,false,false,100);
        int gain=integer(settled,"delta");CHECK(gain>previous_gain);previous_gain=gain;cJSON_Delete(settled);
        snprintf(owner,sizeof owner,"loss-level-%d",level);workspace_profile(w,owner,1500);
        CHECK(workspace_issue(w,owner,"loss",&chosen,true,0));settled=workspace_submit(w,owner,"loss","",false,true,100);
        int loss=integer(settled,"delta");CHECK(loss>previous_loss&&loss<0);previous_loss=loss;cJSON_Delete(settled);
    }
    Puzzle boundary;CHECK(workspace_pick_level(p,400,0,NULL,&boundary));CHECK(boundary.rating==600);
    CHECK(workspace_pick_level(p,3000,4,NULL,&boundary));CHECK(boundary.rating==2799);
    CHECK(workspace_pick_level(p,1500,2,"no-such-theme",&boundary));
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
    CHECK(workspace_issue(w,"bob","repeat",&puzzle,true,0));result=workspace_submit(w,"bob","repeat",puzzle.moves,false,false,100);CHECK(integer(result,"delta")==0);CHECK(!cJSON_IsTrue(cJSON_GetObjectItem(result,"rated")));CHECK(workspace_rating(w,"bob",4)==after);cJSON_Delete(result);
    CHECK(workspace_issue(w,"bob","daily-retry",&puzzle,true,end));result=workspace_submit(w,"bob","daily-retry",puzzle.moves,true,false,100);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"solved")));CHECK(integer(result,"delta")==0);cJSON_Delete(result);workspace_streak(w,"bob",end,&current,&longest);CHECK(current==1&&longest==1);CHECK(workspace_rating(w,"bob",4)==after);
        /* A v1 attempt snapshot survives an idempotent migration and reopening. */
    char legacy_path[100];snprintf(legacy_path,sizeof legacy_path,"/tmp/oc-migration-%u.db",(unsigned)rand());remove(legacy_path);sqlite3 *legacy_db=NULL;CHECK(sqlite3_open(legacy_path,&legacy_db)==SQLITE_OK);
    CHECK(sqlite3_exec(legacy_db,"CREATE TABLE oc_attempts(owner TEXT,id TEXT,puzzle TEXT NOT NULL,rated INTEGER,day INTEGER DEFAULT 0,settled INTEGER DEFAULT 0,solved INTEGER DEFAULT 0,hint INTEGER DEFAULT 0,reveal INTEGER DEFAULT 0,elapsed INTEGER DEFAULT 0,delta INTEGER DEFAULT 0,created INTEGER NOT NULL,PRIMARY KEY(owner,id));",NULL,NULL,NULL)==SQLITE_OK);
    cJSON *snapshot=workspace_puzzle_json(&puzzle);char *encoded=cJSON_PrintUnformatted(snapshot);cJSON_Delete(snapshot);sqlite3_stmt *legacy_insert=NULL;sqlite3_prepare_v2(legacy_db,"INSERT INTO oc_attempts(owner,id,puzzle,rated,created) VALUES('old','issued',?,1,0)",-1,&legacy_insert,NULL);sqlite3_bind_text(legacy_insert,1,encoded,-1,SQLITE_TRANSIENT);CHECK(sqlite3_step(legacy_insert)==SQLITE_DONE);sqlite3_finalize(legacy_insert);free(encoded);sqlite3_close(legacy_db);
    Workspace *migrated=workspace_open(legacy_path);CHECK(migrated!=NULL);sqlite3_stmt *migrated_row=workspace_stmt(migrated,"SELECT puzzle_id FROM oc_attempts WHERE owner='old'");CHECK(sqlite3_step(migrated_row)==SQLITE_ROW);CHECK(!strcmp((const char *)sqlite3_column_text(migrated_row,0),puzzle.id));sqlite3_finalize(migrated_row);workspace_close(migrated);migrated=workspace_open(legacy_path);CHECK(migrated!=NULL);workspace_close(migrated);remove(legacy_path);
    char reason[128];CHECK(workspace_pick(w,p,"alice",1500,true,NULL,&puzzle,reason));
    sqlite3_exec(w->db,"CREATE TABLE users(id INTEGER PRIMARY KEY,name TEXT);INSERT INTO users VALUES(1,'alice'),(2,'bob'),(3,'eve');",NULL,NULL,NULL);
    cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"op","friend.action");cJSON_AddNumberToObject(q,"peer",2);cJSON_AddStringToObject(q,"action","request");result=community_request(w,p,1,"alice",q);cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"friend.action\",\"peer\":1,\"action\":\"accept\"}");result=community_request(w,p,2,"bob",q);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"message.send\",\"peer\":2,\"text\":\"你好，朋友\"}");result=community_request(w,p,1,"alice",q);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(result,"rows"))==1);cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"messages\",\"peer\":2}");result=community_request(w,p,3,"eve",q);CHECK(cJSON_IsFalse(cJSON_GetObjectItem(result,"ok")));cJSON_Delete(result);cJSON_Delete(q);
    q=cJSON_Parse("{\"op\":\"friend.action\",\"peer\":1,\"action\":\"block\"}");result=community_request(w,p,2,"bob",q);cJSON_Delete(result);cJSON_Delete(q);CHECK(community_blocked(w,1,2));
    for(int level=0;level<5;level++){
        q=cJSON_CreateObject();cJSON_AddStringToObject(q,"op","puzzle.next");cJSON_AddNumberToObject(q,"difficulty",level);
        result=community_request(w,p,1,"alice",q);Puzzle issued;CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));
        CHECK(workspace_puzzle_parse(cJSON_GetObjectItem(result,"puzzle"),&issued));int target=1610+(level-2)*300;
        CHECK(issued.rating>=target-150&&issued.rating<=target+149);cJSON_Delete(result);cJSON_Delete(q);
    }
    /* Live mode changes cannot affect daily, settled or another player's attempt. */
    workspace_profile(w,"switch",1500);CHECK(workspace_issue(w,"switch","switch-off",&puzzle,true,0));
    CHECK(!workspace_puzzle_mode(w,"alice","switch-off",false));CHECK(workspace_puzzle_mode(w,"switch","switch-off",false));
    result=workspace_submit(w,"switch","switch-off",puzzle.moves,false,false,100);CHECK(integer(result,"delta")==0);cJSON_Delete(result);
    CHECK(!workspace_puzzle_mode(w,"switch","switch-off",true));
    CHECK(workspace_issue(w,"switch","switch-on",&puzzle,false,0));CHECK(workspace_puzzle_mode(w,"switch","switch-on",true));
    result=workspace_submit(w,"switch","switch-on",puzzle.moves,false,false,100);CHECK(integer(result,"delta")==0);cJSON_Delete(result); /* Repeat puzzle still cannot score. */
    CHECK(workspace_issue(w,"switch","daily-mode",&puzzle,false,end));CHECK(!workspace_puzzle_mode(w,"switch","daily-mode",true));
    CHECK(workspace_issue(w,"alice","server-mode",&puzzle,true,0));
    q=cJSON_Parse("{\"op\":\"puzzle.mode\",\"attempt\":\"server-mode\",\"rated\":false}");result=community_request(w,p,1,"alice",q);CHECK(cJSON_IsTrue(cJSON_GetObjectItem(result,"ok")));CHECK(cJSON_IsFalse(cJSON_GetObjectItem(result,"rated")));cJSON_Delete(result);cJSON_Delete(q);
    puzzles_free(p);workspace_close(w);printf("Workspace, daily archive, immutable scoring, social permissions and setup: %s\n",failures?"FAILED":"PASSED");return failures!=0;
}
