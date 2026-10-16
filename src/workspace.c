#include "workspace.h"
#include "fen.h"
#include "move.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

sqlite3_stmt *workspace_stmt(Workspace *w, const char *sql)
{
    sqlite3_stmt *s = NULL;
    if (w) sqlite3_prepare_v2(w->db, sql, -1, &s, NULL);
    return s;
}
static void text(sqlite3_stmt *s, int n, const char *v) { sqlite3_bind_text(s,n,v?v:"",-1,SQLITE_TRANSIENT); }
static const char *str(const cJSON *j, const char *key) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key); return cJSON_IsString(v)?v->valuestring:"";
}
static int number(const cJSON *j, const char *key, int def) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key); return cJSON_IsNumber(v)?v->valueint:def;
}
cJSON *workspace_rows(sqlite3_stmt *s)
{
    cJSON *rows=cJSON_CreateArray();
    if (!s) return rows;
    while (sqlite3_step(s)==SQLITE_ROW) {
        cJSON *j=cJSON_CreateObject();
        for (int i=0;i<sqlite3_column_count(s);i++) {
            const char *name=sqlite3_column_name(s,i);
            if (sqlite3_column_type(s,i)==SQLITE_INTEGER)
                cJSON_AddNumberToObject(j,name,(double)sqlite3_column_int64(s,i));
            else if (sqlite3_column_type(s,i)!=SQLITE_NULL)
                cJSON_AddStringToObject(j,name,(const char *)sqlite3_column_text(s,i));
        }
        cJSON_AddItemToArray(rows,j);
    }
    sqlite3_finalize(s); return rows;
}
Workspace *workspace_open(const char *file)
{
    Workspace *w=calloc(1,sizeof *w);
    if (!w) return NULL;
    if (sqlite3_open(file,&w->db)!=SQLITE_OK) { workspace_close(w); return NULL; }
    sqlite3_busy_timeout(w->db,5000);
    const char *schema=
      "PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;"
      "CREATE TABLE IF NOT EXISTS oc_meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS oc_profiles(owner TEXT PRIMARY KEY,nickname TEXT NOT NULL DEFAULT '',avatar INTEGER NOT NULL DEFAULT 0);"
      "CREATE TABLE IF NOT EXISTS oc_analyses(owner TEXT,id TEXT,title TEXT,pgn TEXT,node TEXT,updated INTEGER,PRIMARY KEY(owner,id));"
      "CREATE TABLE IF NOT EXISTS oc_ratings(owner TEXT,category INTEGER,value INTEGER NOT NULL,PRIMARY KEY(owner,category));"
      "CREATE TABLE IF NOT EXISTS oc_rating_events(id INTEGER PRIMARY KEY,owner TEXT,category INTEGER,value INTEGER,created INTEGER,source TEXT,UNIQUE(owner,category,source));"
      "CREATE INDEX IF NOT EXISTS oc_rating_owner ON oc_rating_events(owner,category,created);"
      "CREATE TABLE IF NOT EXISTS oc_daily(day INTEGER PRIMARY KEY,puzzle TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS oc_daily_progress(owner TEXT,day INTEGER,completed INTEGER NOT NULL DEFAULT 0,practiced INTEGER NOT NULL DEFAULT 1,PRIMARY KEY(owner,day));"
      "CREATE TABLE IF NOT EXISTS oc_attempts(owner TEXT,id TEXT,puzzle TEXT NOT NULL,rated INTEGER,day INTEGER NOT NULL DEFAULT 0,settled INTEGER DEFAULT 0,solved INTEGER DEFAULT 0,hint INTEGER DEFAULT 0,reveal INTEGER DEFAULT 0,elapsed INTEGER DEFAULT 0,delta INTEGER DEFAULT 0,created INTEGER NOT NULL,puzzle_id TEXT NOT NULL DEFAULT '',PRIMARY KEY(owner,id));"
      "CREATE INDEX IF NOT EXISTS oc_attempt_owner ON oc_attempts(owner,created);"
      "CREATE TABLE IF NOT EXISTS oc_friends(a INTEGER,b INTEGER,state INTEGER NOT NULL,requester INTEGER NOT NULL,PRIMARY KEY(a,b),CHECK(a<b));"
      "CREATE TABLE IF NOT EXISTS oc_blocks(owner INTEGER,target INTEGER,PRIMARY KEY(owner,target));"
      "CREATE TABLE IF NOT EXISTS oc_messages(id INTEGER PRIMARY KEY,sender INTEGER,recipient INTEGER,body TEXT NOT NULL,created INTEGER,read INTEGER DEFAULT 0);"
      "CREATE INDEX IF NOT EXISTS oc_message_pair ON oc_messages(sender,recipient,id);"
      "CREATE TABLE IF NOT EXISTS oc_games(id TEXT PRIMARY KEY,white TEXT,black TEXT,white_name TEXT,black_name TEXT,result TEXT,pgn TEXT,base INTEGER,inc INTEGER,category INTEGER,created INTEGER,rated INTEGER,series TEXT,plies INTEGER);"
      "INSERT OR IGNORE INTO oc_meta VALUES('schema','1');";
    if (sqlite3_exec(w->db,schema,NULL,NULL,NULL)!=SQLITE_OK) { workspace_close(w);return NULL; }
    sqlite3_stmt *columns=workspace_stmt(w,"PRAGMA table_info(oc_attempts)");bool has_id=false;
    while(sqlite3_step(columns)==SQLITE_ROW)if(!strcmp((const char *)sqlite3_column_text(columns,1),"puzzle_id"))has_id=true;
    sqlite3_finalize(columns);
    if(!has_id)sqlite3_exec(w->db,"ALTER TABLE oc_attempts ADD COLUMN puzzle_id TEXT NOT NULL DEFAULT ''",NULL,NULL,NULL);
    sqlite3_stmt *old=workspace_stmt(w,"SELECT owner,id,puzzle FROM oc_attempts WHERE puzzle_id=''");
    while(sqlite3_step(old)==SQLITE_ROW){cJSON *j=cJSON_Parse((const char *)sqlite3_column_text(old,2));sqlite3_stmt *update=workspace_stmt(w,"UPDATE oc_attempts SET puzzle_id=? WHERE owner=? AND id=?");text(update,1,str(j,"id"));text(update,2,(const char *)sqlite3_column_text(old,0));text(update,3,(const char *)sqlite3_column_text(old,1));sqlite3_step(update);sqlite3_finalize(update);cJSON_Delete(j);}
    sqlite3_finalize(old);
    sqlite3_exec(w->db,"CREATE INDEX IF NOT EXISTS oc_attempt_puzzle ON oc_attempts(owner,puzzle_id,settled);UPDATE oc_meta SET value='2' WHERE key='schema';",NULL,NULL,NULL);
    return w;
}
void workspace_close(Workspace *w) { if(w){sqlite3_close(w->db);free(w);} }
int workspace_today(void) { return (int)(((int64_t)time(NULL)+28800)/86400); }
/* Gregorian civil dates, independent of the host timezone and DST. */
static int civil(int y,unsigned m,unsigned d) {
    y-=m<=2; int era=(y>=0?y:y-399)/400; unsigned yy=(unsigned)(y-era*400);
    unsigned doy=(153*(m+(m>2?(unsigned)-3:9))+2)/5+d-1;
    return era*146097+(int)(yy*365+yy/4-yy/100+doy)-719468;
}
void workspace_date(int day,char out[11]) {
    int z=day+719468,era=(z>=0?z:z-146096)/146097;
    unsigned doe=(unsigned)(z-era*146097),yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    int y=(int)yoe+era*400;unsigned doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
    unsigned d=doy-(153*mp+2)/5+1,m=mp+(mp<10?3:(unsigned)-9);y+=m<=2;
    snprintf(out,11,"%04d-%02u-%02u",y,m,d);
}
int workspace_day(const char *date) {
    int y,m,d;char tail;
    if(!date||sscanf(date,"%d-%d-%d%c",&y,&m,&d,&tail)!=3||y<1970||y>9999||m<1||m>12||d<1||d>31)return -1;
    int day=civil(y,(unsigned)m,(unsigned)d);char check[11];workspace_date(day,check);
    return strcmp(check,date)?-1:day;
}
int workspace_weekday(int day) { return ((day+3)%7+7)%7; }
int workspace_time_class(int base,int inc) { int64_t t=(int64_t)base+40LL*inc;return t<180000?0:t<480000?1:t<1500000?2:3; }
const char *workspace_class_name(int c) { static const char *n[]={"Bullet","Blitz","Rapid","Classical","Puzzles"};return c>=0&&c<5?n[c]:""; }
cJSON *workspace_puzzle_json(const Puzzle *p) {
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id",p->id);cJSON_AddStringToObject(j,"fen",p->fen);
    cJSON_AddStringToObject(j,"moves",p->moves);cJSON_AddNumberToObject(j,"rating",p->rating);cJSON_AddStringToObject(j,"themes",p->themes);return j;
}
bool workspace_puzzle_parse(const cJSON *j,Puzzle *p) {
    if(!j||!str(j,"id")[0]||!str(j,"fen")[0]||!str(j,"moves")[0])return false;
    memset(p,0,sizeof *p);snprintf(p->id,sizeof p->id,"%s",str(j,"id"));snprintf(p->fen,sizeof p->fen,"%s",str(j,"fen"));
    snprintf(p->moves,sizeof p->moves,"%s",str(j,"moves"));p->rating=number(j,"rating",1500);
    snprintf(p->themes,sizeof p->themes,"%s",str(j,"themes"));return true;
}
bool workspace_daily(Workspace *w,int day,Puzzle *out) {
    sqlite3_stmt *s=workspace_stmt(w,"SELECT puzzle FROM oc_daily WHERE day=?");if(!s)return false;
    sqlite3_bind_int(s,1,day);bool ok=false;
    if(sqlite3_step(s)==SQLITE_ROW){cJSON *j=cJSON_Parse((const char *)sqlite3_column_text(s,0));ok=workspace_puzzle_parse(j,out);cJSON_Delete(j);}sqlite3_finalize(s);return ok;
}
bool workspace_daily_fill(Workspace *w,const Puzzles *p,int today) {
    if(!w||!p||today<0)return false;
    sqlite3_stmt *s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_meta VALUES('daily_start',?)");
    if(!s)return false;sqlite3_bind_int(s,1,today);sqlite3_step(s);sqlite3_finalize(s);
    s=workspace_stmt(w,"SELECT CAST(value AS INTEGER) FROM oc_meta WHERE key='daily_start'");
    int start=today;if(sqlite3_step(s)==SQLITE_ROW)start=sqlite3_column_int(s,0);sqlite3_finalize(s);
    static const int lo[]={600,800,1000,1200,1400,1600,1900},hi[]={799,999,1199,1399,1599,1899,2399};
    if(sqlite3_exec(w->db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)return false;
    bool ok=true;
    for(int d=start;d<=today;d++){
        Puzzle chosen;if(workspace_daily(w,d,&chosen))continue;
        int weekday=workspace_weekday(d),count=0;uint32_t seed=(uint32_t)d*2654435761u;
        for(int k=0;k<puzzles_count(p);k++){
            Puzzle candidate;puzzles_get(p,k,&candidate);
            if(candidate.rating<lo[weekday]||candidate.rating>hi[weekday])continue;
            seed=seed*1664525u+1013904223u;if(seed%(unsigned)++count==0)chosen=candidate;
        }
        if(!count){ok=false;break;}
        cJSON *j=workspace_puzzle_json(&chosen);char *snapshot=cJSON_PrintUnformatted(j);cJSON_Delete(j);
        s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_daily VALUES(?,?)");sqlite3_bind_int(s,1,d);text(s,2,snapshot);free(snapshot);
        if(sqlite3_step(s)!=SQLITE_DONE)ok=false;sqlite3_finalize(s);if(!ok)break;
    }
    sqlite3_exec(w->db,ok?"COMMIT":"ROLLBACK",NULL,NULL,NULL);return ok;
}
cJSON *workspace_calendar(Workspace *w,const char *owner,int first,int last) {
    sqlite3_stmt *s=workspace_stmt(w,"SELECT d.day,COALESCE(p.completed,0) completed,COALESCE(p.practiced,0) practiced FROM oc_daily d LEFT JOIN oc_daily_progress p ON p.day=d.day AND p.owner=? WHERE d.day>=? AND d.day<=? ORDER BY d.day");
    text(s,1,owner);sqlite3_bind_int(s,2,first);sqlite3_bind_int(s,3,last);return workspace_rows(s);
}
void workspace_streak(Workspace *w,const char *owner,int today,int *current,int *longest) {
    *current=*longest=0;sqlite3_stmt *s=workspace_stmt(w,"SELECT day FROM oc_daily_progress WHERE owner=? AND completed=1 AND day<=? ORDER BY day");
    if(!s)return;text(s,1,owner);sqlite3_bind_int(s,2,today);int last=-100000,n=0;
    while(sqlite3_step(s)==SQLITE_ROW){int day=sqlite3_column_int(s,0);n=day==last+1?n+1:1;last=day;if(n>*longest)*longest=n;}
    if(last==today||last==today-1)*current=n;sqlite3_finalize(s);
}
void workspace_profile(Workspace *w,const char *owner,int initial) {
    sqlite3_stmt *s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_profiles(owner) VALUES(?)");if(!s)return;text(s,1,owner);sqlite3_step(s);sqlite3_finalize(s);
    for(int k=0;k<5;k++){
        s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_ratings VALUES(?,?,?)");text(s,1,owner);sqlite3_bind_int(s,2,k);sqlite3_bind_int(s,3,initial);sqlite3_step(s);sqlite3_finalize(s);
        s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_rating_events(owner,category,value,created,source) VALUES(?,?,?,?, 'initial')");text(s,1,owner);sqlite3_bind_int(s,2,k);sqlite3_bind_int(s,3,workspace_rating(w,owner,k));sqlite3_bind_int64(s,4,time(NULL));sqlite3_step(s);sqlite3_finalize(s);
    }
}
int workspace_rating(Workspace *w,const char *owner,int c) {
    sqlite3_stmt *s=workspace_stmt(w,"SELECT value FROM oc_ratings WHERE owner=? AND category=?");if(!s)return 1500;text(s,1,owner);sqlite3_bind_int(s,2,c);int r=1500;if(sqlite3_step(s)==SQLITE_ROW)r=sqlite3_column_int(s,0);sqlite3_finalize(s);return r;
}
bool workspace_pick(Workspace *w,const Puzzles *p,const char *owner,int rating,bool smart,const char *theme,Puzzle *out,char reason[128]) {
    reason[0]=0;char selected[64]="";int sample=0;
    if(smart&&w){
        sqlite3_stmt *s=workspace_stmt(w,"SELECT puzzle,CASE WHEN hint=1 OR reveal=1 THEN 0 ELSE solved END,elapsed FROM oc_attempts WHERE owner=? AND settled=1 AND day=0 AND created>=? AND rowid=(SELECT MIN(a.rowid) FROM oc_attempts a WHERE a.owner=oc_attempts.owner AND a.puzzle_id=oc_attempts.puzzle_id AND a.day=0 AND a.settled=1) ORDER BY created DESC LIMIT 1000");
        text(s,1,owner);sqlite3_bind_int64(s,2,time(NULL)-30*86400);
        struct {char name[64];int n,success;int64_t elapsed;} stats[64]={0};int nstats=0;
        while(sqlite3_step(s)==SQLITE_ROW){sample++;cJSON *j=cJSON_Parse((const char *)sqlite3_column_text(s,0));char tags[192];snprintf(tags,sizeof tags,"%s",str(j,"themes"));cJSON_Delete(j);
            char *save=NULL;for(char *t=strtok_r(tags,"|",&save);t;t=strtok_r(NULL,"|",&save)){
                if(!strcmp(t,"short")||!strcmp(t,"long")||!strcmp(t,"veryLong")||!strcmp(t,"middlegame")||!strcmp(t,"advantage")||!strcmp(t,"crushing"))continue;
                int k=0;while(k<nstats&&strcmp(stats[k].name,t))k++;
                if(k==nstats){if(nstats==64)continue;snprintf(stats[k].name,64,"%s",t);nstats++;}
                if(stats[k].n<50){stats[k].n++;stats[k].success+=sqlite3_column_int(s,1);stats[k].elapsed+=sqlite3_column_int(s,2);}
            }
        }sqlite3_finalize(s);
        static const char *topics[]={"fork","pin","skewer","discoveredAttack","deflection","attraction","sacrifice","defensiveMove","mateIn1","mateIn2","mateIn3","endgame","promotion","hangingPiece","backRankMate"};
        for(size_t t=0;t<sizeof topics/sizeof topics[0];t++){int k=0;while(k<nstats&&strcmp(stats[k].name,topics[t]))k++;if(k==nstats&&nstats<64){snprintf(stats[nstats++].name,64,"%s",topics[t]);}}
        int roll=rand()%100;
        if(sample<10&&nstats){int least=100000,best=0,ties=0;for(int k=0;k<nstats;k++){if(stats[k].n<least){least=stats[k].n;best=k;ties=1;}else if(stats[k].n==least&&rand()%++ties==0)best=k;}snprintf(selected,sizeof selected,"%s",stats[best].name);}
        if(sample>=10&&roll>=90){s=workspace_stmt(w,"SELECT puzzle FROM oc_attempts WHERE owner=? AND day=0 AND settled=1 AND solved=0 AND created<=? ORDER BY RANDOM() LIMIT 1");text(s,1,owner);sqlite3_bind_int64(s,2,time(NULL)-86400);bool found=false;
            if(sqlite3_step(s)==SQLITE_ROW){cJSON *j=cJSON_Parse((const char *)sqlite3_column_text(s,0));found=workspace_puzzle_parse(j,out);cJSON_Delete(j);}sqlite3_finalize(s);
            if(found){snprintf(reason,128,"Review: %s",out->themes);return true;}}
        if(sample>=10&&nstats){int best=-1;double score=2;
            /* 70% weakest topics; 20% least practiced. */
            for(int k=0;k<nstats;k++){double v=roll<70?(stats[k].success+2.0)/(stats[k].n+4.0):stats[k].n/100.0;
                if(best<0||v<score){score=v;best=k;}}
            if(roll<70){
                int weak[3]={-1,-1,-1};
                for(int k=0;k<nstats;k++)for(int rank=0;rank<3;rank++){
                    int old=weak[rank];double v=(stats[k].success+2.0)/(stats[k].n+4.0);
                    double previous=old<0?2:(stats[old].success+2.0)/(stats[old].n+4.0);
                    if(v<previous||(v==previous&&old>=0&&stats[k].elapsed/(stats[k].n?stats[k].n:1)>stats[old].elapsed/(stats[old].n?stats[old].n:1))){for(int j=2;j>rank;j--)weak[j]=weak[j-1];weak[rank]=k;break;}
                }
                int count=nstats<3?nstats:3;best=weak[rand()%count];
            }
            if(best>=0)snprintf(selected,sizeof selected,"%s",stats[best].name);
        }
    }
    const char *filter=selected[0]?selected:theme;
    for(int span=200;span<=400;span+=200)if(puzzles_pick(p,rating-span,rating+span,filter,out)){
        snprintf(reason,128,"%s%s",smart?(selected[0]?"Practice: ":"Explore: "):"",filter&&*filter?filter:"balanced topics");return true;}
    return puzzles_pick(p,400,4000,theme,out);
}
/* Five relative difficulty bands. Fall back to the nearest available puzzle
 * at pool boundaries, preserving theme filters where possible. */
bool workspace_pick_level(const Puzzles *p,int rating,int level,const char *theme,Puzzle *out)
{
    if(level<0||level>4)level=2;int target=rating+(level-2)*300;
    if(puzzles_pick(p,target-150,target+149,theme,out))return true;
    int distance=2147483647,ties=0;bool found=false;
    for(int k=0;k<puzzles_count(p);k++){Puzzle candidate;puzzles_get(p,k,&candidate);if(theme&&*theme&&!strstr(candidate.themes,theme))continue;
        int d=abs(candidate.rating-target);if(d<distance){*out=candidate;distance=d;ties=1;found=true;}else if(d==distance&&rand()%++ties==0)*out=candidate;}
    if(!found&&theme&&*theme)return workspace_pick_level(p,rating,level,NULL,out);return found;
}
bool workspace_issue(Workspace *w,const char *owner,const char *id,const Puzzle *p,bool rated,int day) {
    if(!w||!owner||!id||!*id)return false;
    cJSON *j=workspace_puzzle_json(p);char *snapshot=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    sqlite3_stmt *s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_attempts(owner,id,puzzle,rated,day,created,puzzle_id) VALUES(?,?,?,?,?,?,?)");text(s,1,owner);text(s,2,id);text(s,3,snapshot);sqlite3_bind_int(s,4,rated&&day==0);sqlite3_bind_int(s,5,day);sqlite3_bind_int64(s,6,time(NULL));text(s,7,p->id);free(snapshot);
    bool ok=sqlite3_step(s)==SQLITE_DONE;sqlite3_finalize(s);
    if(ok&&day){s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_daily_progress(owner,day,completed,practiced) VALUES(?,?,0,1)");text(s,1,owner);sqlite3_bind_int(s,2,day);sqlite3_step(s);sqlite3_finalize(s);}return ok;
}
/* Only an unresolved ordinary attempt can change scoring mode. */
bool workspace_puzzle_mode(Workspace *w,const char *owner,const char *attempt,bool rated)
{
    if(!w||!owner||!attempt)return false;
    sqlite3_stmt *s=workspace_stmt(w,"UPDATE oc_attempts SET rated=? WHERE owner=? AND id=? AND day=0 AND settled=0");
    sqlite3_bind_int(s,1,rated);text(s,2,owner);text(s,3,attempt);
    bool ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(w->db)>0;sqlite3_finalize(s);return ok;
}
static bool solution(const Puzzle *p,const char *moves) {
    /* The first UCI move belongs to the opponent; callers submit the complete
     * actual line, including automatic replies, never a claimed success flag. */
    Board b;if(!fen_parse(p->fen,&b))return false;
    char expected[256],actual[256];snprintf(expected,sizeof expected,"%s",p->moves);snprintf(actual,sizeof actual,"%s",moves);
    char *es=NULL,*as=NULL,*e=strtok_r(expected," ",&es),*a=strtok_r(actual," ",&as);int ply=0;
    while(e&&a){
        if(strcmp(e,a))return false;
        if(strlen(a)<4)return false;
        Move m;if(!uci_to_move(&b,a,&m))return false;
        make_move_plumb(&b,m);
        ply++;e=strtok_r(NULL," ",&es);a=strtok_r(NULL," ",&as);
    }return !e&&!a&&ply>=2;
}
cJSON *workspace_submit(Workspace *w,const char *owner,const char *id,const char *moves,bool hint,bool reveal,int elapsed) {
    cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"attempt",id);
    if(!w||sqlite3_exec(w->db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK){cJSON_AddStringToObject(r,"error","storage unavailable");return r;}
    sqlite3_stmt *s=workspace_stmt(w,"SELECT puzzle,rated,day,settled,delta,solved FROM oc_attempts WHERE owner=? AND id=?");text(s,1,owner);text(s,2,id);
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);sqlite3_exec(w->db,"ROLLBACK",NULL,NULL,NULL);cJSON_AddStringToObject(r,"error","unknown attempt");return r;}
    cJSON *j=cJSON_Parse((const char *)sqlite3_column_text(s,0));Puzzle p;bool valid=workspace_puzzle_parse(j,&p);cJSON_Delete(j);
    int rated=sqlite3_column_int(s,1),day=sqlite3_column_int(s,2),settled=sqlite3_column_int(s,3),delta=sqlite3_column_int(s,4),solved=sqlite3_column_int(s,5);sqlite3_finalize(s);
    if(!settled){solved=valid&&!reveal&&solution(&p,moves);int old=workspace_rating(w,owner,4),next=old;
        if(rated){s=workspace_stmt(w,"SELECT 1 FROM oc_attempts WHERE owner=? AND puzzle_id=? AND day=0 AND settled=1 AND id!=? LIMIT 1");text(s,1,owner);text(s,2,p.id);text(s,3,id);if(sqlite3_step(s)==SQLITE_ROW)rated=0;sqlite3_finalize(s);}
        if(rated){double exp=1.0/(1.0+pow(10.0,(p.rating-old)/400.0));
            if(!solved||!hint)next=(int)lround(old+(old<2000?40:24)*((solved?1.0:0.0)-exp));
            if(next<400)next=400;if(next>3000)next=3000;delta=next-old;
        }
        if(elapsed<0)elapsed=0;if(elapsed>24*3600000)elapsed=24*3600000;
        s=workspace_stmt(w,"UPDATE oc_attempts SET settled=1,solved=?,hint=?,reveal=?,elapsed=?,delta=?,rated=? WHERE owner=? AND id=?");sqlite3_bind_int(s,1,solved);sqlite3_bind_int(s,2,hint);sqlite3_bind_int(s,3,reveal);sqlite3_bind_int(s,4,elapsed);sqlite3_bind_int(s,5,delta);sqlite3_bind_int(s,6,rated);text(s,7,owner);text(s,8,id);sqlite3_step(s);sqlite3_finalize(s);
        if(rated){s=workspace_stmt(w,"UPDATE oc_ratings SET value=? WHERE owner=? AND category=4");sqlite3_bind_int(s,1,next);text(s,2,owner);sqlite3_step(s);sqlite3_finalize(s);
            s=workspace_stmt(w,"INSERT OR IGNORE INTO oc_rating_events(owner,category,value,created,source) VALUES(?,4,?,?,?)");text(s,1,owner);sqlite3_bind_int(s,2,next);sqlite3_bind_int64(s,3,time(NULL));text(s,4,id);sqlite3_step(s);sqlite3_finalize(s);}
        if(day){s=workspace_stmt(w,"INSERT INTO oc_daily_progress(owner,day,completed) VALUES(?,?,?) ON CONFLICT(owner,day) DO UPDATE SET practiced=1,completed=MAX(completed,excluded.completed)");text(s,1,owner);sqlite3_bind_int(s,2,day);sqlite3_bind_int(s,3,solved);sqlite3_step(s);sqlite3_finalize(s);}
    }
    sqlite3_exec(w->db,"COMMIT",NULL,NULL,NULL);cJSON_AddBoolToObject(r,"solved",solved);cJSON_AddBoolToObject(r,"rated",rated!=0);cJSON_AddNumberToObject(r,"delta",delta);cJSON_AddNumberToObject(r,"rating",workspace_rating(w,owner,4));return r;
}
