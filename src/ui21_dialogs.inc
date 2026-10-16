/* Small dialogs share the same typed controls and UTF-8 editing rules. */
static SDL_Rect ui_dialog_rect(Gui *g){return (SDL_Rect){208+(g->win_w-208-560)/2,170,560,570};}
static void ui_metadata_begin(Gui *g)
{
    Ui21 *u=g->ui;u->metadata_open=true;u->dialog_field=-1;

}
static void ui_metadata_apply(Gui *g)
{
    Ui21 *u=g->ui;snprintf(g->pgn_hdr.event,sizeof g->pgn_hdr.event,"%s",u->metadata[0]);snprintf(g->pgn_hdr.site,sizeof g->pgn_hdr.site,"%s",u->metadata[1]);snprintf(g->pgn_hdr.date,sizeof g->pgn_hdr.date,"%s",u->metadata[2]);snprintf(g->pgn_hdr.white,sizeof g->pgn_hdr.white,"%s",u->metadata[3]);snprintf(g->pgn_hdr.black,sizeof g->pgn_hdr.black,"%s",u->metadata[4]);snprintf(g->pgn_hdr.result,sizeof g->pgn_hdr.result,"%s",u->metadata[5]);
}
static void ui_modal_render(Gui *g)
{
    Ui21 *u=g->ui;if(!u||(!u->challenge_open&&!u->metadata_open))return;
    SDL_SetRenderDrawBlendMode(g->ren,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(g->ren,24,48,76,85);SDL_RenderFillRect(g->ren,&(SDL_Rect){0,0,g->win_w,g->win_h});SDL_SetRenderDrawBlendMode(g->ren,SDL_BLENDMODE_NONE);
    SDL_Rect box=ui_dialog_rect(g);ui_card(g,box);render_text(g,g->font_ui,ui_tr(g,u->metadata_open?"Game tags":"Challenge player",u->metadata_open?"棋谱资料":"发起约战"),box.x+24,box.y+22,ui_ink);
    if(u->metadata_open){const char *labels[]={"Event","Site","Date","White","Black","Result"},*zh[]={"赛事","地点","日期","白方","黑方","结果"};
        for(int k=0;k<6;k++){ui_text(g,u->zh?zh[k]:labels[k],box.x+24,box.y+84+k*60,ui_muted);ui_button(g,(SDL_Rect){box.x+120,box.y+70+k*60,416,44},u->metadata[k][0]?u->metadata[k]:"—",-1,u->dialog_field==k);}
        ui_button(g,(SDL_Rect){box.x+24,box.y+478,240,46},ui_tr(g,"Done","完成"),5,true);ui_button(g,(SDL_Rect){box.x+284,box.y+478,252,46},ui_tr(g,"Cancel","取消"),-1,false);
    }else{
        ui_text(g,ui_tr(g,"Time in minutes","基础时间（分钟）"),box.x+24,box.y+88,ui_ink);ui_button(g,(SDL_Rect){box.x+290,box.y+72,246,44},u->challenge_minutes,-1,u->dialog_field==0);
        ui_text(g,ui_tr(g,"Increment in seconds","每步加秒"),box.x+24,box.y+152,ui_ink);ui_button(g,(SDL_Rect){box.x+290,box.y+136,246,44},u->challenge_increment,-1,u->dialog_field==1);
        const char *color=u->challenge_color<0?ui_tr(g,"Random colour","随机颜色"):u->challenge_color==0?ui_tr(g,"Play White","执白"):ui_tr(g,"Play Black","执黑");
        ui_button(g,(SDL_Rect){box.x+24,box.y+216,512,48},color,-1,false);ui_button(g,(SDL_Rect){box.x+24,box.y+286,512,48},ui_tr(g,u->challenge_rated?"Rated":"Unrated",u->challenge_rated?"计分对局":"不计分对局"),-1,u->challenge_rated);
        ui_text(g,ui_tr(g,"Invitation expires after 60 seconds.","邀请将在 60 秒后失效。"),box.x+24,box.y+374,ui_muted);
        ui_button(g,(SDL_Rect){box.x+24,box.y+478,240,46},ui_tr(g,"Send challenge","发送约战"),0,true);ui_button(g,(SDL_Rect){box.x+284,box.y+478,252,46},ui_tr(g,"Cancel","取消"),-1,false);
    }
}
static bool ui_modal_event(Gui *g,const SDL_Event *e)
{
    Ui21 *u=g->ui;if(!u->metadata_open&&!u->challenge_open)return false;SDL_Rect box=ui_dialog_rect(g);
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT){int mx=g->mouse.x,my=g->mouse.y;
        if(u->metadata_open){if(my>=box.y+70&&my<box.y+430&&mx>=box.x+120&&mx<=box.x+536){u->dialog_field=(my-box.y-70)/60;if(u->dialog_field==5){const char *results[]={"*","1-0","0-1","1/2-1/2"};int n=0;while(n<4&&strcmp(u->metadata[5],results[n]))n++;snprintf(u->metadata[5],128,"%s",results[(n+1)%4]);u->dialog_field=-1;}return true;}}
        else {
            if(my>=box.y+72&&my<box.y+180&&mx>box.x+290){u->dialog_field=my<box.y+120?0:1;return true;}
            if(my>=box.y+216&&my<box.y+264){u->challenge_color=u->challenge_color==1?-1:u->challenge_color+1;return true;}
            if(my>=box.y+286&&my<box.y+334){u->challenge_rated=!u->challenge_rated;return true;}
        }
        if(my>=box.y+478&&my<box.y+524){if(mx>=box.x+284){u->metadata_open=u->challenge_open=false;u->dialog_field=-1;return true;}
            if(u->metadata_open){u->metadata_open=false;u->dialog_field=-1;return true;}
            int minutes=atoi(u->challenge_minutes),inc=atoi(u->challenge_increment);if(minutes<1||minutes>180||inc<0||inc>60){snprintf(u->status,256,"%s",ui_tr(g,"Use 1–180 minutes and 0–60 seconds increment.","基础时间须为 1–180 分钟，加秒须为 0–60 秒。"));return true;}
            cJSON *q=ui_args_int("peer",u->peer);cJSON_AddNumberToObject(q,"time",minutes*60000);cJSON_AddNumberToObject(q,"inc",inc*1000);cJSON_AddNumberToObject(q,"color",u->challenge_color);cJSON_AddBoolToObject(q,"rated",u->challenge_rated);ui_request(g,"challenge.create",q);u->challenge_open=false;u->dialog_field=-1;return true;
        }return true;
    }
    if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_ESCAPE){u->challenge_open=u->metadata_open=false;return true;}
    char *dst=NULL;size_t cap=0;
    if(u->dialog_field>=0){if(u->metadata_open&&u->dialog_field<5){dst=u->metadata[u->dialog_field];cap=sizeof u->metadata[0];}else if(u->challenge_open){dst=u->dialog_field==0?u->challenge_minutes:u->challenge_increment;cap=16;}}
    if(dst&&e->type==SDL_KEYDOWN){if(e->key.keysym.sym==SDLK_BACKSPACE)ui_backspace(dst);if((e->key.keysym.mod&KMOD_CTRL)&&e->key.keysym.sym==SDLK_a)dst[0]=0;return true;}
    if(dst&&e->type==SDL_TEXTINPUT){size_t n=strlen(dst);for(const unsigned char *p=(const unsigned char *)e->text.text;*p&&n+1<cap;p++){if(u->challenge_open&&(*p<'0'||*p>'9'))continue;if(u->metadata_open&&(*p=='"'||*p=='\\'||*p<32))continue;dst[n++]=(char)*p;}dst[n]=0;return true;}
    return true;
}
