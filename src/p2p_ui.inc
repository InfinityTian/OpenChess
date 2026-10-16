/* Clipboard signaling keeps invitations private and avoids a hosted lobby. */
static SDL_Rect p2p_rect(Gui *g,int index)
{
    int x=240,w=g->win_w-272;if(w>920)w=920;
    if(index==0)return (SDL_Rect){x,146,(w-16)/2,46};
    if(index==1)return (SDL_Rect){x+(w+16)/2,146,(w-16)/2,46};
    if(index==2)return (SDL_Rect){x,216,w,48};
    if(index==3)return (SDL_Rect){x,342,(w-16)/2,48};
    if(index==4)return (SDL_Rect){x+(w+16)/2,342,(w-16)/2,48};
    if(index==5)return (SDL_Rect){x,418,w,46};
    if(index==6)return (SDL_Rect){x,580,(w-16)/2,42};
    return (SDL_Rect){x+(w+16)/2,580,(w-16)/2,42};
}
static void p2p_paste(Gui *g)
{
    char *clip=SDL_GetClipboardText();
    if(!clip||strlen(clip)>65535){SDL_free(clip);p2p_status(g,"Invalid invitation.","邀请格式无效。");return;}
    bool offer;
    /* Parse first; a malformed paste must not destroy the current connection. */
    cJSON *json=!strncmp(clip,"OC-P2P1:",8)?cJSON_Parse(clip+8):NULL;
    const cJSON *type=cJSON_GetObjectItemCaseSensitive(json,"type");
    offer=cJSON_IsString(type)&&!strcmp(type->valuestring,"offer");
    bool answer=cJSON_IsString(type)&&!strcmp(type->valuestring,"answer");
    const cJSON *sdp=cJSON_GetObjectItemCaseSensitive(json,"sdp");
    bool valid=(offer||answer)&&cJSON_IsString(sdp)&&strlen(sdp->valuestring)<32768&&strstr(sdp->valuestring,"a=fingerprint:")&&strstr(sdp->valuestring,"a=candidate:");
    cJSON_Delete(json);
    if(!valid){SDL_free(clip);p2p_status(g,"Invalid invitation / response. Copy the complete text.","邀请或回应无效，请复制完整内容。");return;}
    if(offer){
        if(g->net&&transport_role(g->net)==NET_ROLE_HOST){SDL_free(clip);p2p_status(g,"You created an invitation; paste your friend's response instead.","你已创建邀请，请粘贴好友的回应。");return;}
        if(!g->net)p2p_create(g,false);
    }
    if(!net_p2p_import(g->net,clip))p2p_status(g,"Could not import. Check invitation direction, or start again.","无法导入，请检查邀请与回应方向，或重新创建连接。");
    else {g->p2p_connected_at=SDL_GetTicks();p2p_status(g,offer?"Preparing response…":"Connecting to your friend…",offer?"正在生成回应……":"正在连接好友……");}
    SDL_free(clip);
}
static void p2p_render(Gui *g)
{
    ui_background(g,g->ren);int x=240,w=g->win_w-272;if(w>920)w=920;
    ui_text(g,ui_tr(g,"P2P · Play with a friend","P2P · 好友对弈"),x,42,ui_ink);
    ui_text(g,ui_tr(g,"No account or game server · encrypted · unrated","无需账号或游戏服务器 · 加密直连 · 不计分"),x,82,ui_muted);
    static const char *labels[]={"Unlimited","1+0","2+1","3+0","3+2","5+0","5+3","10+0","10+5","15+10","30+0"};
    char text[256];snprintf(text,sizeof text,ui_tr(g,"Time: %s","时限：%s"),ui_old_text(g,labels[clampi(g->p2p_time,0,10)]));
    ui_button(g,p2p_rect(g,0),text,-1,false);
    const char *side=g->p2p_side==0?ui_tr(g,"White","白方"):g->p2p_side==1?ui_tr(g,"Black","黑方"):ui_tr(g,"Random","随机");
    snprintf(text,sizeof text,ui_tr(g,"Creator plays: %s","房主执子：%s"),side);ui_button(g,p2p_rect(g,1),text,-1,false);
    ui_button(g,p2p_rect(g,2),ui_tr(g,"Create invitation","创建邀请"),0,true);
    ui_text(g,ui_tr(g,"1. Send invitation; your friend imports it and returns a response.","1. 房主发送邀请 → 好友导入邀请，并回传回应。"),x,286,ui_ink);
    ui_text(g,ui_tr(g,"2. Import the response to start the game.","2. 房主导入回应 → 连接成功后自动开局。"),x,310,ui_ink);
    ui_button(g,p2p_rect(g,3),ui_tr(g,"Copy invitation / response","复制邀请／回应"),21,false);
    ui_button(g,p2p_rect(g,4),ui_tr(g,"Import from clipboard","从剪贴板导入"),4,true);
    SDL_Rect field=p2p_rect(g,5);ui_round(g->ren,field,ui_control,10);ui_panel_border(g->ren,field,10);
    SDL_Rect clip;SDL_RenderGetClipRect(g->ren,&clip);bool clipping=SDL_RenderIsClipEnabled(g->ren);
    SDL_Rect inner={field.x+10,field.y+4,field.w-20,field.h-8};SDL_RenderSetClipRect(g->ren,&inner);
    ui_text(g,g->p2p_ice[0]?g->p2p_ice:ui_tr(g,"No STUN / TURN (LAN only)","未使用 STUN／TURN（仅本地连接）"),field.x+12,field.y+14,ui_ink);
    SDL_RenderSetClipRect(g->ren,clipping?&clip:NULL);
    ui_text(g,ui_tr(g,"Optional STUN / UDP TURN · edit before creating / importing","STUN／TURN 地址 · 可选、可编辑 · 请在创建或导入前设置"),x,476,ui_muted);
    ui_text(g,ui_tr(g,"If direct connection fails, use UDP TURN or a virtual LAN.","受限网络可能无法直连，可配置自己的 TURN 服务或使用虚拟局域网。"),x,502,ui_muted);
    const char *status=!net_p2p_available()?ui_tr(g,"P2P dependency missing; rebuild with make p2p-deps.","当前构建缺少 P2P 依赖，请先构建依赖。"):g->p2p_status;
    if(*status)ui_text(g,status,x,542,ui_accent);
    ui_button(g,p2p_rect(g,6),ui_tr(g,"Reset connection","重置连接"),19,false);
    ui_button(g,p2p_rect(g,7),ui_tr(g,"Back","返回"),15,false);
    SDL_Rect focus=p2p_rect(g,g->p2p_control);ui_round_outline(g->ren,focus,ui_accent);
}
static void p2p_activate(Gui *g,int hit)
{
    g->p2p_control=hit;g->p2p_focus=hit==5;
        bool configuring=!g->net;
        if(hit==0&&configuring)g->p2p_time=(g->p2p_time+1)%11;
        else if(hit==1&&configuring)g->p2p_side=(g->p2p_side+1)%3;
        else if(hit==2)p2p_create(g,true);
        else if(hit==3){if(g->p2p_export[0]&&SDL_SetClipboardText(g->p2p_export)==0)p2p_status(g,"Copied. Send the complete text to your friend.","已复制，请把完整内容发送给好友。");else p2p_status(g,"Invitation / response is not ready yet.","邀请／回应尚未就绪，请稍候。");}
        else if(hit==4)p2p_paste(g);
        else if(hit==6){if(g->net){transport_close(g->net);g->net=NULL;}p2p_open(g);}
        else if(hit==7){if(g->net){transport_close(g->net);g->net=NULL;}g->scene=SCENE_MENU;}
}
static bool p2p_event(Gui *g,const SDL_Event *e)
{
    if(g->scene!=SCENE_P2P)return false;
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT){
        set_mouse(g,e->button.x,e->button.y);if(g->mouse.x<208)return false;
        int hit=-1;for(int k=0;k<8;k++)if(ui_hit(g,p2p_rect(g,k)))hit=k;
        p2p_activate(g,hit);
        return true;
    }
    if(e->type==SDL_TEXTINPUT){if(g->p2p_focus&&!g->net){size_t n=strlen(g->p2p_ice),add=strlen(e->text.text);if(n+add<sizeof g->p2p_ice)memcpy(g->p2p_ice+n,e->text.text,add+1);}return true;}
    if(e->type==SDL_KEYDOWN){
        SDL_Keycode k=e->key.keysym.sym;bool shortcut=e->key.keysym.mod&(KMOD_CTRL|KMOD_GUI);
        if(shortcut&&k==SDLK_q){g->quit=true;return true;}
        if(k==SDLK_ESCAPE){if(g->net){transport_close(g->net);g->net=NULL;}g->scene=SCENE_MENU;return true;}
        if(g->p2p_focus&&!g->net){
            if(k==SDLK_BACKSPACE){size_t n=strlen(g->p2p_ice);if(n)g->p2p_ice[n-1]=0;}
            else if(shortcut&&k==SDLK_a)g->p2p_ice[0]=0;
            else if(shortcut&&k==SDLK_v){char *text=SDL_GetClipboardText();if(text&&strlen(text)<sizeof g->p2p_ice&&!strchr(text,'\n')&&!strchr(text,'\r'))snprintf(g->p2p_ice,sizeof g->p2p_ice,"%s",text);SDL_free(text);}
            else if(k==SDLK_RETURN)g->p2p_focus=false;
        }else if(shortcut&&k==SDLK_v)p2p_paste(g);
        else if(shortcut&&k==SDLK_c&&g->p2p_export[0])SDL_SetClipboardText(g->p2p_export);
        else if(k==SDLK_RETURN)p2p_activate(g,g->p2p_control);
        if(k==SDLK_TAB){g->p2p_control=(g->p2p_control+((e->key.keysym.mod&KMOD_SHIFT)?7:1))%8;g->p2p_focus=g->p2p_control==5;}
        if(!g->p2p_focus&&(k==SDLK_LEFT||k==SDLK_RIGHT)&&(g->p2p_control==0||g->p2p_control==1)&&!g->net){int d=k==SDLK_LEFT?-1:1;if(!g->p2p_control)g->p2p_time=(g->p2p_time+d+11)%11;else g->p2p_side=(g->p2p_side+d+3)%3;}
        return true;
    }
    return false;
}
