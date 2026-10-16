#include "community.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static const char *string(const cJSON *j,const char *k){const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:"";}
static int integer(const cJSON *j,const char *k,int def){const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsNumber(v)?v->valueint:def;}
static bool boolean(const cJSON *j,const char *k){return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,k));}
static void bind(sqlite3_stmt *s,int i,const char *v){sqlite3_bind_text(s,i,v,-1,SQLITE_TRANSIENT);}
static cJSON *error(cJSON *r,const char *message){cJSON_ReplaceItemInObject(r,"ok",cJSON_CreateBool(false));cJSON_AddStringToObject(r,"error",message);return r;}
bool community_blocked(Workspace *w,int a,int b){sqlite3_stmt *s=workspace_stmt(w,"SELECT 1 FROM oc_blocks WHERE (owner=? AND target=?) OR (owner=? AND target=?)");sqlite3_bind_int(s,1,a);sqlite3_bind_int(s,2,b);sqlite3_bind_int(s,3,b);sqlite3_bind_int(s,4,a);bool v=sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);return v;}
static bool friends(Workspace *w,int a,int b){sqlite3_stmt *s=workspace_stmt(w,"SELECT 1 FROM oc_friends WHERE a=MIN(?,?) AND b=MAX(?,?) AND state=1");sqlite3_bind_int(s,1,a);sqlite3_bind_int(s,2,b);sqlite3_bind_int(s,3,a);sqlite3_bind_int(s,4,b);bool v=sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);return v;}
cJSON *community_request(Workspace *w,Puzzles *p,int uid,const char *owner,const cJSON *q)
{
    const char *op=string(q,"op");cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"type","response");cJSON_AddStringToObject(r,"op",op);cJSON_AddNumberToObject(r,"request_id",integer(q,"request_id",0));cJSON_AddBoolToObject(r,"ok",true);
    cJSON_AddNumberToObject(r,"offset",integer(q,"offset",0));cJSON_AddStringToObject(r,"search",string(q,"search"));
    if(!w)return error(r,"Storage unavailable");
    int peer=integer(q,"peer",0),offset=integer(q,"offset",0);if(offset<0)offset=0;
    sqlite3_stmt *s=NULL;
    if(!strcmp(op,"daily.get")||!strcmp(op,"daily.calendar")){
        int today=workspace_today();workspace_daily_fill(w,p,today);
        if(!strcmp(op,"daily.calendar")){int start=integer(q,"first",today-31),end=integer(q,"last",today);if(end>today)end=today;if(end-start>366)return error(r,"Date range too large");
            cJSON_AddItemToObject(r,"rows",workspace_calendar(w,owner,start,end));int current,longest;workspace_streak(w,owner,today,&current,&longest);cJSON_AddNumberToObject(r,"current",current);cJSON_AddNumberToObject(r,"longest",longest);cJSON_AddNumberToObject(r,"today",today);return r;}
        int day=integer(q,"day",today);Puzzle puzzle;if(day>today||!workspace_daily(w,day,&puzzle))return error(r,"Daily puzzle unavailable for this date");
        cJSON_AddNumberToObject(r,"day",day);cJSON_AddItemToObject(r,"puzzle",workspace_puzzle_json(&puzzle));return r;
    }
    if(uid<=0)return error(r,"Please log in");
    if(!strcmp(op,"profile.get")){
        s=workspace_stmt(w,"SELECT nickname,avatar FROM oc_profiles WHERE owner=?");bind(s,1,owner);cJSON_AddItemToObject(r,"rows",workspace_rows(s));
        s=workspace_stmt(w,"SELECT category,value FROM oc_ratings WHERE owner=? ORDER BY category");bind(s,1,owner);cJSON_AddItemToObject(r,"ratings",workspace_rows(s));
    }else if(!strcmp(op,"profile.set")){
        const char *nick=string(q,"nickname");if(!*nick||strlen(nick)>96)return error(r,"Nickname must be 1-96 UTF-8 bytes");
        s=workspace_stmt(w,"UPDATE oc_profiles SET nickname=?,avatar=? WHERE owner=?");bind(s,1,nick);sqlite3_bind_int(s,2,abs(integer(q,"avatar",0))%12);bind(s,3,owner);sqlite3_step(s);sqlite3_finalize(s);
    }else if(!strcmp(op,"ratings.history")){
        s=workspace_stmt(w,"SELECT id,category,value,created FROM oc_rating_events WHERE id IN(SELECT MAX(id) FROM oc_rating_events WHERE owner=? AND category=? AND created>=? GROUP BY CASE WHEN ?=0 THEN strftime('%Y-%m',created,'unixepoch','+8 hours') ELSE CAST((created+28800)/86400 AS TEXT) END) ORDER BY created LIMIT 366");bind(s,1,owner);sqlite3_bind_int(s,2,integer(q,"category",0));sqlite3_bind_int(s,3,integer(q,"since",0));sqlite3_bind_int(s,4,integer(q,"since",0));cJSON_AddItemToObject(r,"rows",workspace_rows(s));
    }else if(!strcmp(op,"people")||!strcmp(op,"friends")){
        const char *sql=!strcmp(op,"people")?
         "SELECT u.id,u.name,COALESCE(p.nickname,u.name) nickname,COALESCE(p.avatar,0) avatar,COALESCE(f.state,-1) state,COALESCE(f.requester,0) requester,(SELECT COUNT(*) FROM oc_messages m WHERE m.sender=u.id AND m.recipient=? AND m.read=0) unread FROM users u LEFT JOIN oc_profiles p ON p.owner=u.name LEFT JOIN oc_friends f ON f.a=MIN(u.id,?) AND f.b=MAX(u.id,?) WHERE u.id!=? AND u.name LIKE ? AND NOT EXISTS(SELECT 1 FROM oc_blocks WHERE owner=? AND target=u.id) ORDER BY u.name LIMIT 10 OFFSET ?":
         "SELECT u.id,u.name,COALESCE(p.nickname,u.name) nickname,COALESCE(p.avatar,0) avatar,f.state,f.requester,(SELECT COUNT(*) FROM oc_messages m WHERE m.sender=u.id AND m.recipient=? AND m.read=0) unread FROM users u JOIN oc_friends f ON f.a=MIN(u.id,?) AND f.b=MAX(u.id,?) LEFT JOIN oc_profiles p ON p.owner=u.name WHERE u.id!=? AND u.name LIKE ? AND NOT EXISTS(SELECT 1 FROM oc_blocks WHERE owner=? AND target=u.id) ORDER BY u.name LIMIT 10 OFFSET ?";
        s=workspace_stmt(w,sql);for(int k=1;k<=4;k++)sqlite3_bind_int(s,k,uid);char pattern[130];snprintf(pattern,sizeof pattern,"%%%.*s%%",96,string(q,"search"));bind(s,5,pattern);sqlite3_bind_int(s,6,uid);sqlite3_bind_int(s,7,offset);cJSON_AddItemToObject(r,"rows",workspace_rows(s));cJSON_AddNumberToObject(r,"self",uid);
        s=workspace_stmt(w,"SELECT COUNT(*) unread FROM oc_messages m WHERE recipient=? AND read=0 AND NOT EXISTS(SELECT 1 FROM oc_blocks b WHERE b.owner=m.recipient AND b.target=m.sender)");sqlite3_bind_int(s,1,uid);cJSON_AddItemToObject(r,"notifications",workspace_rows(s));
        s=workspace_stmt(w,"SELECT COUNT(*) pending FROM oc_friends WHERE (a=? OR b=?) AND state=0 AND requester!=?");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,uid);sqlite3_bind_int(s,3,uid);cJSON_AddItemToObject(r,"requests",workspace_rows(s));
    }else if(!strcmp(op,"blocks")){
        s=workspace_stmt(w,"SELECT u.id,u.name,u.name nickname,-2 state,0 requester,0 unread FROM users u JOIN oc_blocks b ON b.target=u.id WHERE b.owner=? ORDER BY u.name LIMIT 10 OFFSET ?");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,offset);cJSON_AddItemToObject(r,"rows",workspace_rows(s));cJSON_AddNumberToObject(r,"self",uid);
    }else if(!strcmp(op,"friend.action")){
        const char *action=string(q,"action");if(peer<=0||peer==uid)return error(r,"Invalid player");
        if(!strcmp(action,"block")||!strcmp(action,"unblock")){
            s=workspace_stmt(w,!strcmp(action,"block")?"INSERT OR IGNORE INTO oc_blocks VALUES(?,?)":"DELETE FROM oc_blocks WHERE owner=? AND target=?");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,peer);sqlite3_step(s);sqlite3_finalize(s);return r;}
        if(community_blocked(w,uid,peer))return error(r,"Player unavailable");
        if(!strcmp(action,"request")){
            s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_friends SELECT MIN(?,id),MAX(?,id),0,? FROM users WHERE id=?");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,uid);sqlite3_bind_int(s,3,uid);sqlite3_bind_int(s,4,peer);
        }else if(!strcmp(action,"accept")){
            s=workspace_stmt(w,"UPDATE oc_friends SET state=1 WHERE a=MIN(?,?) AND b=MAX(?,?) AND requester!=? AND state=0");
            sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,peer);sqlite3_bind_int(s,3,uid);sqlite3_bind_int(s,4,peer);sqlite3_bind_int(s,5,uid);
        }else if(!strcmp(action,"remove")||!strcmp(action,"decline")){
            s=workspace_stmt(w,"DELETE FROM oc_friends WHERE a=MIN(?,?) AND b=MAX(?,?)");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,peer);sqlite3_bind_int(s,3,uid);sqlite3_bind_int(s,4,peer);
        }else return error(r,"Unknown friend action");
        sqlite3_step(s);sqlite3_finalize(s);
    }else if(!strcmp(op,"messages")||!strcmp(op,"message.send")||!strcmp(op,"message.read")){
        if(community_blocked(w,uid,peer)||!friends(w,uid,peer))return error(r,"Private messages require friendship");
        if(!strcmp(op,"message.send")){
            const char *body=string(q,"text");if(!*body||strlen(body)>640)return error(r,"Message must be 1-640 UTF-8 bytes");
            s=workspace_stmt(w,"INSERT INTO oc_messages(sender,recipient,body,created) VALUES(?,?,?,?)");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,peer);bind(s,3,body);sqlite3_bind_int64(s,4,time(NULL));sqlite3_step(s);sqlite3_finalize(s);
        }else if(!strcmp(op,"message.read")){
            s=workspace_stmt(w,"UPDATE oc_messages SET read=1 WHERE sender=? AND recipient=? AND id<=?");sqlite3_bind_int(s,1,peer);sqlite3_bind_int(s,2,uid);sqlite3_bind_int(s,3,integer(q,"through",0));sqlite3_step(s);sqlite3_finalize(s);
        }
        s=workspace_stmt(w,"SELECT id,sender,recipient,body,created,read FROM oc_messages WHERE ((sender=? AND recipient=?) OR (sender=? AND recipient=?)) AND (?=0 OR id<?) ORDER BY id DESC LIMIT 10");sqlite3_bind_int(s,1,uid);sqlite3_bind_int(s,2,peer);sqlite3_bind_int(s,3,peer);sqlite3_bind_int(s,4,uid);sqlite3_bind_int(s,5,offset);sqlite3_bind_int(s,6,offset);cJSON_AddItemToObject(r,"rows",workspace_rows(s));cJSON_AddNumberToObject(r,"peer",peer);
    }else if(!strcmp(op,"games")){
        s=workspace_stmt(w,"SELECT id,white_name,black_name,result,base,inc,category,created,rated,series,plies FROM oc_games WHERE white=? OR black=? ORDER BY created DESC LIMIT 10 OFFSET ?");bind(s,1,owner);bind(s,2,owner);sqlite3_bind_int(s,3,offset);cJSON_AddItemToObject(r,"rows",workspace_rows(s));
    }else if(!strcmp(op,"game.get")){
        s=workspace_stmt(w,"SELECT pgn FROM oc_games WHERE id=? AND (white=? OR black=?)");bind(s,1,string(q,"id"));bind(s,2,owner);bind(s,3,owner);cJSON_AddItemToObject(r,"rows",workspace_rows(s));
    }else if(!strcmp(op,"headtohead")){
        s=workspace_stmt(w,"SELECT name FROM users WHERE id=?");sqlite3_bind_int(s,1,peer);char other[40]="";if(sqlite3_step(s)==SQLITE_ROW)snprintf(other,40,"%s",sqlite3_column_text(s,0));sqlite3_finalize(s);
        s=workspace_stmt(w,"SELECT SUM(CASE WHEN result='1/2-1/2' THEN 1 ELSE 0 END) draws,SUM(CASE WHEN (white=? AND result='1-0') OR (black=? AND result='0-1') THEN 1 ELSE 0 END) wins,COUNT(*) games FROM oc_games WHERE (white=? AND black=?) OR (white=? AND black=?)");bind(s,1,owner);bind(s,2,owner);bind(s,3,owner);bind(s,4,other);bind(s,5,other);bind(s,6,owner);cJSON_AddItemToObject(r,"rows",workspace_rows(s));
    }else if(!strcmp(op,"puzzle.next")){
        Puzzle puzzle;char reason[128];int day=integer(q,"day",0);bool found;
        if(day){workspace_daily_fill(w,p,workspace_today());found=day<=workspace_today()&&workspace_daily(w,day,&puzzle);reason[0]=0;}
        else found=workspace_pick(w,p,owner,workspace_rating(w,owner,4),boolean(q,"smart"),string(q,"theme"),&puzzle,reason);
        if(!found)return error(r,"No matching puzzle");
        char id[100];sqlite3_randomness(16,id);char attempt[40];for(int k=0;k<16;k++)snprintf(attempt+k*2,3,"%02x",(unsigned char)id[k]);
        workspace_issue(w,owner,attempt,&puzzle,boolean(q,"rated"),day);cJSON_AddStringToObject(r,"attempt",attempt);cJSON_AddNumberToObject(r,"day",day);cJSON_AddStringToObject(r,"reason",reason);cJSON_AddItemToObject(r,"puzzle",workspace_puzzle_json(&puzzle));
    }else if(!strcmp(op,"puzzle.submit")){
        cJSON_AddItemToObject(r,"result",workspace_submit(w,owner,string(q,"attempt"),string(q,"moves"),boolean(q,"hint"),boolean(q,"reveal"),integer(q,"elapsed",0)));
    }else return error(r,"Unknown operation");
    return r;
}
