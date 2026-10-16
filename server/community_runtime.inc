/* Runtime invitations; durable social data lives in community.c. */
typedef struct {int id,from,to,base,inc,rated,color;uint64_t expires;} Challenge;
static Challenge challenges[256];
static int challenge_sequence;
static Session *user_session(int uid){for(int k=0;k<MAX_SESS;k++)if(sessions[k].used&&sessions[k].logged_in&&sessions[k].user_id==uid)return &sessions[k];return NULL;}
static bool available(Session *s){return s&&!account_busy(s)&&!s->queued&&(s->room<0||rooms[s->room].over);}
static void handle_request(Session *s,const ProtoFrame *f)
{
    const char *op=proto_field_str(f,"op");if(!op)return;
    cJSON *response;
    if(!strncmp(op,"challenge.",10)){
        response=proto_new(PROTO_S2C_RESPONSE);cJSON_AddStringToObject(response,"op",op);cJSON_AddNumberToObject(response,"request_id",proto_field_int(f,"request_id",0));
        cJSON_AddBoolToObject(response,"ok",true);const char *err=NULL;
        if(!s->logged_in)err="Please log in";
        else if(!strcmp(op,"challenge.create")){
            int uid=proto_field_int(f,"peer",0);Session *target=user_session(uid);
            if(uid==s->user_id||!available(s)||!available(target)||community_blocked(g_workspace,s->user_id,uid))err="Player is unavailable";
            else {
                int slot=-1;for(int k=0;k<256;k++){
                    if(challenges[k].expires>now_ms()&&(challenges[k].from==s->user_id||challenges[k].to==uid)){err="An invitation is already pending";break;}
                    if(challenges[k].expires<=now_ms())slot=k;
                }
                int base=proto_field_int(f,"time",300000),inc=proto_field_int(f,"inc",0);
                if(base<1000||base>10800000||inc<0||inc>60000)err="Invalid time control";
                if(!err&&slot>=0)challenges[slot]=(Challenge){++challenge_sequence,s->user_id,uid,base,inc,proto_field_bool(f,"rated",false),proto_field_int(f,"color",-1),now_ms()+60000};
                else if(!err)err="Too many invitations";
            }
        }else if(!strcmp(op,"challenge.list")){
            cJSON *rows=cJSON_AddArrayToObject(response,"rows");
            for(int k=0;k<256;k++){Challenge *c=&challenges[k];if(c->expires<=now_ms()||(c->from!=s->user_id&&c->to!=s->user_id)||community_blocked(g_workspace,c->from,c->to))continue;
                cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"id",c->id);cJSON_AddNumberToObject(j,"from",c->from);cJSON_AddNumberToObject(j,"to",c->to);cJSON_AddNumberToObject(j,"time",c->base);cJSON_AddNumberToObject(j,"inc",c->inc);cJSON_AddNumberToObject(j,"seconds",(double)(c->expires-now_ms())/1000);cJSON_AddItemToArray(rows,j);}
        }else{
            int id=proto_field_int(f,"id",0);Challenge *c=NULL;for(int k=0;k<256;k++)if(challenges[k].id==id&&challenges[k].expires>now_ms())c=&challenges[k];
            if(!c||(c->from!=s->user_id&&c->to!=s->user_id))err="Invitation expired";
            else if(!strcmp(op,"challenge.accept")){
                Session *from=user_session(c->from);
                if(c->to!=s->user_id||!available(s)||!available(from)||community_blocked(g_workspace,c->from,c->to))err="Player is unavailable";
                else {
                    leave_if_over(from);leave_if_over(s);char code[7];int ri=alloc_room(code,c->base,c->inc);
                    if(ri<0)err="Server is full";
                    else {
                        Session *pair[2];int color=c->color==0?0:c->color==1?1:rand()%2;pair[color]=from;pair[1-color]=s;
                        Room *r=&rooms[ri];for(int i=0;i<2;i++){pair[i]->room=ri;pair[i]->seat=i;pair[i]->want_rated=c->rated;r->seat[i]=sidx(pair[i]);r->connected[i]=1;snprintf(r->name[i],40,"%s",pair[i]->nick);snprintf(r->seattok[i],40,"%s",pair[i]->token);}start_match(r);
                        for(int k=0;k<256;k++)if(challenges[k].from==c->from||challenges[k].to==c->from||challenges[k].from==c->to||challenges[k].to==c->to)challenges[k].expires=0;
                    }
                }
            }else if(!strcmp(op,"challenge.cancel")||!strcmp(op,"challenge.decline"))c->expires=0;
            else err="Unknown invitation action";
        }
        if(err){cJSON_ReplaceItemInObject(response,"ok",cJSON_CreateBool(false));cJSON_AddStringToObject(response,"error",err);}
    }else response=community_request(g_workspace,g_puzzles,s->logged_in?s->user_id:0,s->logged_in?s->user:s->token,f->root);
    if(!strcmp(op,"people")||!strcmp(op,"friends")){
        cJSON *row=NULL;cJSON_ArrayForEach(row,cJSON_GetObjectItem(response,"rows")){
            int uid=cJSON_GetObjectItem(row,"id")->valueint;Session *other=user_session(uid);cJSON_AddBoolToObject(row,"online",other!=NULL);cJSON_AddBoolToObject(row,"available",available(other));
        }
    }
    if(!strcmp(op,"puzzle.submit")){
        s->puzzle_rating=workspace_rating(g_workspace,s->user,4);accounts_set_puzzle(g_accounts,s->user_id,s->puzzle_rating);send_auth(s,true,NULL);
    }
    cJSON_AddStringToObject(response,"actor",s->logged_in?s->user:"");
    sess_send(response,s);
}
static void persist_game(Room *r,const char *result)
{
    if(!g_workspace)return;
    char pgn[20000];int n=snprintf(pgn,sizeof pgn,"[White \"%s\"]\n[Black \"%s\"]\n[Result \"%s\"]\n\n",r->owner[0],r->owner[1],result);

    for(int k=0;k<r->ply&&n<(int)sizeof pgn-40;k++){
        if(k%2==0)n+=snprintf(pgn+n,sizeof pgn-n,"%d. ",k/2+1);
        n+=snprintf(pgn+n,sizeof pgn-n,"%s ",r->san[k]);
    }snprintf(pgn+n,sizeof pgn-n,"%s",result);
    sqlite3_exec(g_workspace->db,"BEGIN IMMEDIATE",NULL,NULL,NULL);
    sqlite3_stmt *q=workspace_stmt(g_workspace,"INSERT OR IGNORE INTO oc_games VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const char *strings[]={r->game_id,r->owner[0],r->owner[1],r->name[0],r->name[1],result,pgn};
    for(int k=0;k<7;k++)sqlite3_bind_text(q,k+1,strings[k],-1,SQLITE_TRANSIENT);
    int category=workspace_time_class(r->init_time,r->inc_ms);sqlite3_bind_int(q,8,r->init_time);sqlite3_bind_int(q,9,r->inc_ms);sqlite3_bind_int(q,10,category);sqlite3_bind_int64(q,11,time(NULL));sqlite3_bind_int(q,12,r->rated);sqlite3_bind_text(q,13,r->series,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,14,r->ply);
    bool inserted=sqlite3_step(q)==SQLITE_DONE&&sqlite3_changes(g_workspace->db)>0;sqlite3_finalize(q);
    if(inserted&&r->rated){int ratings[2]={workspace_rating(g_workspace,r->owner[0],category),workspace_rating(g_workspace,r->owner[1],category)};
        double score=!strcmp(result,"1-0")?1:!strcmp(result,"0-1")?0:0.5;
        int delta=(int)lround(32*(score-1.0/(1+pow(10,(ratings[1]-ratings[0])/400.0))));
        for(int k=0;k<2;k++){int value=ratings[k]+(k?-delta:delta);if(value<400)value=400;
            q=workspace_stmt(g_workspace,"UPDATE oc_ratings SET value=? WHERE owner=? AND category=?");sqlite3_bind_int(q,1,value);sqlite3_bind_text(q,2,r->owner[k],-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,3,category);sqlite3_step(q);sqlite3_finalize(q);
            q=workspace_stmt(g_workspace,"INSERT OR IGNORE INTO oc_rating_events(owner,category,value,created,source) VALUES(?,?,?,?,?)");sqlite3_bind_text(q,1,r->owner[k],-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,2,category);sqlite3_bind_int(q,3,value);sqlite3_bind_int64(q,4,time(NULL));sqlite3_bind_text(q,5,r->game_id,-1,SQLITE_TRANSIENT);sqlite3_step(q);sqlite3_finalize(q);
            int seat=r->seat[k];if(seat>=0&&sessions[seat].used){sessions[seat].pvp_rating=value;}
        }
    }
    sqlite3_exec(g_workspace->db,"COMMIT",NULL,NULL,NULL);
    if(inserted&&r->rated)for(int k=0;k<2;k++){int seat=r->seat[k];if(seat>=0&&sessions[seat].used)accounts_set_pvp(g_accounts,sessions[seat].user_id,sessions[seat].pvp_rating);}
}
