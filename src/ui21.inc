/* Shared SDL presentation and state for the 2.1 workspace. Included here so
 * existing board/analysis operations retain their single implementation. */
static const char *ui_tr(Gui *g,const char *en,const char *zh){return g->ui&&g->ui->zh?zh:en;}
static const char *ui_old_text(Gui *g,const char *s)
{
    if(!g->ui||!g->ui->zh)return s;
    static const char *pairs[][2]={{"All themes","全部主题"},{"Themes","应用主题"},{"Boards","棋盘"},{"Instant","即时"},{"Very Easy","非常简单"},{"Normal","标准"},{"Very Hard","非常困难"},{"Rating","评分"},{"Theme","主题"},{"Local profile","离线档案"},{"Log out","退出登录"},{"Close","关闭"},{"Create account","创建账号"},{"User","用户"},{"Online Matchmaking","在线匹配"},{"Server","服务器"},{"Room code (to join)","房间号（加入对局）"},{"Tab/Up/Down field    Enter choose    L login    R rated    Esc back","Tab/上下键选择 · 回车确认 · L 登录 · R 计分模式 · Esc 返回"},{"Paste PGN here (Ctrl+V), or use Upload.","在此粘贴 PGN（Ctrl+V），或选择文件。"},{"Draw offered: Ctrl+A accept, Ctrl+D decline","收到和棋提议：Ctrl+A 接受，Ctrl+D 拒绝"},{"Arrows","箭头"},{"Type a move (SAN):","输入走子（SAN）："},{"e.g. e4, Nf3, O-O-O","例如 e4、Nf3、O-O-O"},{"Ctrl+B board   Ctrl+P pieces   Ctrl+M anim","Ctrl+B 棋盘 · Ctrl+P 棋子 · Ctrl+M 动画"},{"Stockfish unavailable. Open Settings > Engine to install.","Stockfish 不可用，请在设置 → 引擎中安装。"},{"Left/Right change    Up/Down field    Enter start    Esc back","左右键调整 · 上下键选择 · 回车开始 · Esc 返回"},{"Host = White, Join = Black.  Start the host first.","房主执白，加入方执黑。请先创建主机。"},{"Tab/Up/Down field    type to edit    Enter activate    Esc back","Tab/上下键选择 · 输入编辑 · 回车确认 · Esc 返回"},{"Arrows select    Tab / 1-2-3 switch tab    Enter apply    Esc back","方向键选择 · Tab / 1-2-3 切换 · 回车应用 · Esc 返回"},{"Install Stockfish (click)","安装 Stockfish（点击）"},{"No engines found on PATH - enter a path below","未找到引擎，请在下方输入路径。"},{"Analysis settings","分析设置"},{"Custom engine path (click to edit, Enter to apply):","自定义引擎路径（点击编辑，回车应用）："},{"Tab / 1-2-3 switch tab    Up/Down select    Left/Right change    Enter open    Esc back","Tab / 1-2-3 切换 · 上下键选择 · 左右键调整 · 回车打开 · Esc 返回"},{"Game Report","对局报告"},{"Avg centipawn loss","平均厘兵损失"},{"Openings","开局"},{"type to search...","输入以搜索……"},{"move on board to browse  type to filter  Left/Right step  Enter analyse  Esc back","走子浏览 · 输入筛选 · 左右键步进 · 回车分析 · Esc 返回"},{"C / SDL chess board","国际象棋桌面应用"},{"Up/Down select    Enter choose    Ctrl+Q quit","上下键选择 · 回车确认 · Ctrl+Q 退出"},{"Save PGN as:","将棋谱保存为："},{"Enter save    Esc cancel","回车保存 · Esc 取消"},{"You: White","你执白"},{"You: Black","你执黑"},{"Spectating","正在观战"},{"Analysis: no engine","分析：未连接引擎"},{"Analyze to see the evaluation graph","运行分析以查看评分曲线"},{"Press Enter to type a move","按回车输入走子"},{"< Previous","< 上一步"},{"Next >","下一步 >"},{"Stalemate","逼和"},{"Draw (insufficient material)","和棋（子力不足）"},{"Draw (threefold repetition)","和棋（三次重复）"},{"Draw (fifty-move rule)","和棋（五十步规则）"},{"Solved!","已解出！"},{"Answer shown","已显示答案"},{"Your move","轮到你走棋"},{"Import PGN or FEN","导入 PGN 或 FEN"},{"Time control","时限"},{"Play as","执子颜色"},{"Connect","连接"},{"Disconnect","断开"},{"Room code","房间号"},{"Game Rating","对局评分"},{"Accuracy","准确率"},{"Best","最佳"},{"Brilliant","精彩"},{"Great","优秀"},{"Good","好棋"},{"Inaccuracy","不精确"},{"Mistake","失误"},{"Blunder","漏着"},{"Forced","迫招"},{"Book","定式"},{"White","白方"},{"Black","黑方"},{"You","你"},{"Hint","提示"},{"Reveal","查看答案"},{"Undo","悔棋"},{"Restart","重新开始"},{"Back","返回"},{"Start","开始"},{"Menu","主页"},{"Moves","步法"},{"Analysis","分析"},{"Settings","设置"},{"Engine","引擎"},{"Gameplay","对局"},{"Audio / Video","音视频"},{"Appearance","外观"},{"Save PGN","保存棋谱"},{"Load","加载"},{"Copy","复制"},{"Cancel","取消"},{"Paste","粘贴"},{"Upload","选择文件"},{"Import PGN","导入棋谱"},{"Rematch","再来一局"},{"Resign","认输"},{"Difficulty","难度"},{"Side","执子"},{"Easy","简单"},{"Medium","中等"},{"Hard","困难"},{"Expert","专家"},{"Opening Book","开局库"},{"Singleplayer","电脑对战"},{"Local Multiplayer","局域网对战"},{"Online","在线"},{"Offline","离线"},{"Register","注册"},{"Login","登录"},{"Logout","退出登录"},{"Username","账号"},{"Password","密码"},{"Next","下一题"},{"Lines","主变化"},{"Threads","线程"},{"Hash (MB)","哈希内存 (MB)"},{"Time (ms)","时间 (毫秒)"},{"Depth","深度"},{"unlimited","不限"},{"off","关闭"},{"Board","棋盘"},{"Pieces","棋子"},{"Animation","动画"},{"Mode","模式"},{"Appearance picker","外观选择"},{"Sound","音效"},{"Max FPS","帧率上限"},{"Audio & Video","音视频"},{"Any","全部主题"},{"Puzzles","战术解题"},{"Around my rating","接近我的水平"},{"Under 1000","低于 1000"},{"mateIn1","一步将杀"},{"mateIn2","两步将杀"},{"mateIn3","三步将杀"},{"fork","捉双"},{"pin","牵制"},{"skewer","串击"},{"discoveredAttack","闪击"},{"deflection","引离"},{"attraction","引入"},{"sacrifice","弃子"},{"defensiveMove","防守"},{"kingsideAttack","王翼进攻"},{"queensideAttack","后翼进攻"},{"endgame","残局"},{"Host","创建"},{"Join","加入"},{"Spectate","观战"},{"Analyze","分析"},{"Report","报告"},{"Import","导入"},{"Styles","外观"},{"White to move","白方行棋"},{"Black to move","黑方行棋"}};
    for(size_t k=0;k<sizeof pairs/sizeof pairs[0];k++)if(!strcmp(s,pairs[k][0]))return pairs[k][1];return s;
}
static void ui_text(Gui *g,const char *s,int x,int y,SDL_Color c){render_text(g,g->font_small,s,x,y,c);}
#include "ui21_theme.inc"
static void ui_round(SDL_Renderer *r,SDL_Rect rect,SDL_Color color,int radius)
{
    color=ui_surface_color(color);
    if(rect.w<=0||rect.h<=0)return;
    if(radius>rect.h/2)radius=rect.h/2;if(radius>rect.w/2)radius=rect.w/2;
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
#if SDL_VERSION_ATLEAST(2,0,18)
    /* Filled geometry survives fractional downscaling on SDL's software backend. */
    SDL_Vertex vertices[38]={0};int indices[108],n=1;
    vertices[0]=(SDL_Vertex){{rect.x+rect.w/2.0f,rect.y+rect.h/2.0f},color,{0,0}};
    for(int corner=0;corner<4;corner++){float cx=corner==0||corner==1?rect.x+rect.w-radius:rect.x+radius;
        float cy=corner==0||corner==3?rect.y+radius:rect.y+rect.h-radius;
        for(int step=0;step<=8;step++){float angle=(-90+corner*90+step*90.0f/8)*GUI_PI/180;
            vertices[n++]=(SDL_Vertex){{cx+radius*cosf(angle),cy+radius*sinf(angle)},color,{0,0}};}}
    int count=0;for(int k=1;k<n;k++){indices[count++]=0;indices[count++]=k;indices[count++]=k+1<n?k+1:1;}
    SDL_RenderGeometry(r,NULL,vertices,n,indices,count);
#else
    SDL_RenderFillRect(r,&rect);
#endif
}
static void ui_card(Gui *g,SDL_Rect r)
{
    SDL_SetRenderDrawBlendMode(g->ren,SDL_BLENDMODE_BLEND);
    SDL_Rect shadow=r;shadow.y+=4;ui_round(g->ren,shadow,(SDL_Color){46,89,125,22},14);
    ui_round(g->ren,r,(SDL_Color){250,253,255,255},14);
    SDL_SetRenderDrawBlendMode(g->ren,SDL_BLENDMODE_NONE);
}
static void ui_line(SDL_Renderer *r,float x1,float y1,float x2,float y2)
{
#if SDL_VERSION_ATLEAST(2,0,18)
    SDL_Color c;SDL_GetRenderDrawColor(r,&c.r,&c.g,&c.b,&c.a);float dx=x2-x1,dy=y2-y1,len=sqrtf(dx*dx+dy*dy);float nx=len?-dy/len*0.8f:0.8f,ny=len?dx/len*0.8f:0;
    if(!len){x2=x1+1;y2=y1+1;}
    SDL_Vertex v[]={{{x1+nx,y1+ny},c,{0,0}},{{x1-nx,y1-ny},c,{0,0}},{{x2-nx,y2-ny},c,{0,0}},{{x2+nx,y2+ny},c,{0,0}}};int indices[]={0,1,2,0,2,3};SDL_RenderGeometry(r,NULL,v,4,indices,6);
#else
    SDL_RenderDrawLine(r,x1,y1,x2,y2);
#endif
}
static void ui_outline(SDL_Renderer *r,const SDL_Rect *box){ui_line(r,box->x,box->y,box->x+box->w,box->y);ui_line(r,box->x+box->w,box->y,box->x+box->w,box->y+box->h);ui_line(r,box->x+box->w,box->y+box->h,box->x,box->y+box->h);ui_line(r,box->x,box->y+box->h,box->x,box->y);}
static void ui_arc(SDL_Renderer *r,float cx,float cy,float radius,float from,float to)
{
    /* Connected vector strokes avoid the dotted outlines of the old icons. */
    int segments=(int)ceilf(fabsf(to-from)/6);if(segments<1)segments=1;
    for(int k=0;k<segments;k++){float a=(from+(to-from)*k/segments)*GUI_PI/180,b=(from+(to-from)*(k+1)/segments)*GUI_PI/180;ui_line(r,cx+radius*cosf(a),cy+radius*sinf(a),cx+radius*cosf(b),cy+radius*sinf(b));}
}
static void ui_round_outline(SDL_Renderer *r,SDL_Rect box,SDL_Color color)
{
    color=ui_surface_color(color);SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    float radius=fminf(7,fminf(box.w/2.0f,box.h/2.0f));
    ui_line(r,box.x+radius,box.y,box.x+box.w-radius,box.y);
    ui_line(r,box.x+radius,box.y+box.h,box.x+box.w-radius,box.y+box.h);
    ui_line(r,box.x,box.y+radius,box.x,box.y+box.h-radius);
    ui_line(r,box.x+box.w,box.y+radius,box.x+box.w,box.y+box.h-radius);
    ui_arc(r,box.x+radius,box.y+radius,radius,180,270);
    ui_arc(r,box.x+box.w-radius,box.y+radius,radius,270,360);
    ui_arc(r,box.x+box.w-radius,box.y+box.h-radius,radius,0,90);
    ui_arc(r,box.x+radius,box.y+box.h-radius,radius,90,180);
}
static void ui_caret_color(SDL_Renderer *ren){SDL_SetRenderDrawColor(ren,ui_ink.r,ui_ink.g,ui_ink.b,255);}
static void ui_move_background(Gui *g,SDL_Rect rect,bool selected)
{
    ui_round(g->ren,rect,selected?ui_hover:ui_control,selected?5:0);
}
static void ui_icon(Gui *g,int kind,int x,int y,SDL_Color color)
{
    SDL_Renderer *r=g->ren;SDL_SetRenderDrawColor(r,color.r,color.g,color.b,255);
    if(kind==6){SDL_Rect box={x+1,y+4,20,14};ui_outline(r,&box);ui_line(r,x+1,y+4,x+11,y+12);ui_line(r,x+21,y+4,x+11,y+12);}
    else if(kind==7){ui_arc(r,x+7,y+6,4,0,360);ui_arc(r,x+16,y+7,3,0,360);ui_arc(r,x+7,y+20,7,180,360);ui_arc(r,x+17,y+20,5,270,360);}
    else if(kind==8){ui_arc(r,x+11,y+11,6,0,360);ui_arc(r,x+11,y+11,2,0,360);for(int k=0;k<8;k++){float a=k*GUI_PI/4;ui_line(r,x+11+7*cosf(a),y+11+7*sinf(a),x+11+10*cosf(a),y+11+10*sinf(a));}}
    else if(kind==9){ui_arc(r,x+11,y+9,7,180,360);ui_line(r,x+4,y+9,x+4,y+16);ui_line(r,x+18,y+9,x+18,y+16);ui_line(r,x+2,y+17,x+20,y+17);ui_arc(r,x+11,y+19,2,0,180);ui_line(r,x+11,y,x+11,y+2);}
    else if(kind==1){ui_arc(r,x+11,y+8,7,145,395);ui_line(r,x+5,y+12,x+7,y+17);ui_line(r,x+17,y+12,x+15,y+17);ui_line(r,x+7,y+17,x+15,y+17);ui_line(r,x+8,y+20,x+14,y+20);}
    else if(kind==2){ui_arc(r,x+9,y+9,7,0,360);ui_line(r,x+14,y+14,x+21,y+21);}
    else if(kind==3){SDL_Rect box={x+2,y+1,18,20};ui_outline(r,&box);ui_line(r,x+6,y+1,x+6,y+8);ui_line(r,x+6,y+8,x+15,y+8);ui_line(r,x+15,y+8,x+15,y+1);SDL_Rect label={x+6,y+13,10,8};ui_outline(r,&label);}
    else if(kind==4){ui_line(r,x+3,y+17,x+16,y+3);ui_line(r,x+6,y+20,x+19,y+6);ui_line(r,x+16,y+3,x+19,y+6);ui_line(r,x+3,y+17,x+2,y+22);ui_line(r,x+2,y+22,x+6,y+20);}
    else if(kind==5){ui_line(r,x+3,y+10,x+8,y+15);ui_line(r,x+8,y+15,x+19,y+3);}
    else if(kind==10){SDL_Rect board={x+1,y+1,20,20};ui_outline(r,&board);for(int k=1;k<4;k++){ui_line(r,x+1+k*5,y+1,x+1+k*5,y+21);ui_line(r,x+1,y+1+k*5,x+21,y+1+k*5);}}
    else if(kind==11){ui_line(r,x+3,y+20,x+19,y+20);ui_line(r,x+5,y+17,x+17,y+17);ui_line(r,x+6,y+17,x+8,y+10);ui_arc(r,x+11,y+7,4,0,360);ui_line(r,x+16,y+17,x+14,y+10);}
    else if(kind==12){ui_line(r,x+11,y+4,x+11,y+21);ui_line(r,x+1,y+2,x+1,y+18);ui_line(r,x+21,y+2,x+21,y+18);ui_line(r,x+1,y+2,x+11,y+5);ui_line(r,x+21,y+2,x+11,y+5);ui_line(r,x+1,y+18,x+11,y+21);ui_line(r,x+21,y+18,x+11,y+21);}
    else if(kind==13){ui_arc(r,x+11,y+13,11,210,330);ui_arc(r,x+11,y+7,11,30,150);ui_arc(r,x+11,y+10,3,0,360);}
    else if(kind==14){for(int k=0;k<3;k++)ui_arc(r,x+3+k*8,y+11,1,0,360);}
    else {ui_line(r,x+3,y+11,x+19,y+11);ui_line(r,x+11,y+3,x+11,y+19);}
}
static bool ui_hit(Gui *g,SDL_Rect r){return pt_in(&r,g->mouse.x,g->mouse.y);}
static void ui_button(Gui *g,SDL_Rect r,const char *label,int icon,bool primary)
{
    bool hover=ui_hit(g,r),icon_only=icon>=0&&(r.w<120||r.h<34);SDL_Color c=primary?ui_accent:ui_control;
    if(hover)c=primary?ui_mix(ui_accent,(SDL_Color){0,0,0,255},0.12f):ui_hover;
    ui_round(g->ren,(SDL_Rect){r.x,r.y+3,r.w,r.h},ui_border,10);ui_round(g->ren,r,c,10);
    SDL_Color ink=primary?(SDL_Color){255,255,255,255}:ui_ink;
    if(icon_only){ui_icon(g,icon,r.x+(r.w-22)/2,r.y+(r.h-22)/2,ink);if(hover){snprintf(g->ui->tooltip,sizeof g->ui->tooltip,"%s",label);g->ui->tooltip_anchor=r;}return;}
    TTF_Font *font=g->font_small;int available=r.w-(icon>=0?48:20),w=0,h=0;if(available<1)return;
    TTF_SizeUTF8(font,label,&w,&h);float scale=eff_scale(g);if(w/scale>available||h/scale>r.h-10){font=g->font_tiny;TTF_SizeUTF8(font,label,&w,&h);}
    SDL_Surface *surface=TTF_RenderUTF8_Blended(font,label,ink);if(!surface)return;SDL_Texture *text=SDL_CreateTextureFromSurface(g->ren,surface);
    float fit=fminf(1,fminf(available*scale/surface->w,(r.h-8)*scale/surface->h));int width=(int)lroundf(surface->w/scale*fit),height=(int)lroundf(surface->h/scale*fit);
    int group=width+(icon>=0?30:0),left=r.x+(r.w-group)/2;if(icon>=0)ui_icon(g,icon,left,r.y+(r.h-22)/2,ink);
    SDL_Rect dest={left+(icon>=0?30:0),r.y+(r.h-height)/2,width,height};SDL_RenderCopy(g->ren,text,NULL,&dest);SDL_DestroyTexture(text);SDL_FreeSurface(surface);
}
static void ui_tooltip_render(Gui *g)
{
    if(!g->ui||!g->ui->tooltip[0])return;int w=0,h=0;TTF_SizeUTF8(g->font_small,g->ui->tooltip,&w,&h);w=(int)(w/eff_scale(g))+24;h=(int)(h/eff_scale(g))+16;
    SDL_Rect anchor=g->ui->tooltip_anchor,box={clampi(anchor.x,212,g->win_w-w-12),anchor.y-h-8,w,h};if(box.y<8)box.y=anchor.y+anchor.h+8;ui_card(g,box);bool legacy=g->ui->legacy_paint;g->ui->legacy_paint=false;ui_text(g,g->ui->tooltip,box.x+12,box.y+8,ui_ink);g->ui->legacy_paint=legacy;
}
static bool ui_active_game(Gui *g){return g->online&&online_state(g->online)==ONLINE_PLAYING&&!g->online_over&&!online_spectating(g->online);}
static void ui_background(Gui *g,SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r,ui_bg.r,ui_bg.g,ui_bg.b,255);SDL_RenderClear(r);
    if(!g->ui||!g->ui->background)return;
    int w,h;SDL_QueryTexture(g->ui->background,NULL,NULL,&w,&h);if(w<=0||h<=0)return;
    float sx=(float)g->win_w/w,sy=(float)g->win_h/h;
    float scale=g->ui->background_fit?fminf(sx,sy):fmaxf(sx,sy);
    SDL_Rect dst={(g->win_w-(int)(w*scale))/2,(g->win_h-(int)(h*scale))/2,(int)(w*scale),(int)(h*scale)};
    SDL_RenderCopy(r,g->ui->background,NULL,&dst);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,ui_bg.r,ui_bg.g,ui_bg.b,(Uint8)g->ui->background_mask);SDL_Rect all={0,0,g->win_w,g->win_h};SDL_RenderFillRect(r,&all);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
}
static void ui_mini_board(Gui *g,const Board *board,SDL_Rect r,bool flipped)
{
    if(g->tex_board)SDL_RenderCopy(g->ren,g->tex_board,NULL,&r);
    int cell=r.w/8;
    for(int row=0;row<8;row++)for(int col=0;col<8;col++){
        SDL_Rect square={r.x+col*cell,r.y+row*cell,cell,cell};
        if(!g->tex_board){SDL_Color c=(row+col)%2?(SDL_Color){135,178,203,255}:(SDL_Color){227,240,248,255};SDL_SetRenderDrawColor(g->ren,c.r,c.g,c.b,255);SDL_RenderFillRect(g->ren,&square);}
        int sq=flipped?row*8+7-col:(7-row)*8+col;Piece pc=board->board[sq];
        if(pc!=EMPTY)render_piece_box(g,g->ren,pc,square.x+cell/2.0f,square.y+cell/2.0f,cell*0.88f,1);
    }
}
static void ui_preview_position(const Puzzle *p,Board *board)
{
    board_reset(board);if(!fen_parse(p->fen,board))return;char uci[8];if(sscanf(p->moves,"%7s",uci)==1){Move m;if(uci_to_move(board,uci,&m))make_move_plumb(board,m);}
}
static cJSON *ui_args_int(const char *key,int value){cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,key,value);return j;}
static const char *ui_social_op(Ui21 *u){return u->social_tab==0?"friends":u->social_tab==1?"people":"blocks";}
static void ui_request(Gui *g,const char *op,cJSON *args)
{
    if(!g->online||!online_logged_in(g->online)){cJSON_Delete(args);snprintf(g->ui->status,sizeof g->ui->status,"%s",ui_tr(g,"Log in to use online features.","请先登录以使用在线功能。"));return;}
    online_request(g->online,op,args);
}
static void ui_owner(Gui *g)
{
    Ui21 *u=g->ui;char owner[96];bool online=!g->offline_mode&&!g->account_local&&((g->online&&online_logged_in(g->online))||g->account_user[0]);
    snprintf(owner,sizeof owner,"%s%s",online?"online:":"local:",online?((g->online&&online_logged_in(g->online))?online_username(g->online):g->account_user):g->local_profile);
    if(!strcmp(owner,u->owner))return;
    snprintf(u->owner,sizeof u->owner,"%s",owner);u->entry_count=0;u->current_entry=u->suspended_entry=-1;u->reviewing=false;
    sqlite3_stmt *check=workspace_stmt(u->store,"SELECT 1 FROM oc_profiles WHERE owner=?");sqlite3_bind_text(check,1,owner,-1,SQLITE_TRANSIENT);bool fresh=sqlite3_step(check)!=SQLITE_ROW;sqlite3_finalize(check);
    workspace_profile(u->store,owner,g->local_rating);
    if(fresh&&!online){sqlite3_stmt *q=workspace_stmt(u->store,"UPDATE oc_ratings SET value=? WHERE owner=? AND category=4");sqlite3_bind_int(q,1,g->puzzle_rating);sqlite3_bind_text(q,2,owner,-1,SQLITE_TRANSIENT);sqlite3_step(q);sqlite3_finalize(q);q=workspace_stmt(u->store,"UPDATE oc_rating_events SET value=? WHERE owner=? AND category=4 AND source='initial'");sqlite3_bind_int(q,1,g->puzzle_rating);sqlite3_bind_text(q,2,owner,-1,SQLITE_TRANSIENT);sqlite3_step(q);sqlite3_finalize(q);}
    g->puzzle_rating=online&&g->online?online_puzzle_rating(g->online):workspace_rating(u->store,owner,4);
    snprintf(u->nickname,sizeof u->nickname,"%s",online?((g->online&&online_logged_in(g->online))?online_username(g->online):g->account_user):g->local_profile);
    cJSON_Delete(u->people);u->people=NULL;cJSON_Delete(u->messages);u->messages=NULL;cJSON_Delete(u->games);u->games=NULL;
    cJSON_Delete(u->profile);u->profile=NULL;cJSON_Delete(u->rating_history);u->rating_history=NULL;cJSON_Delete(u->calendar);u->calendar=NULL;cJSON_Delete(u->invitations);u->invitations=NULL;cJSON_Delete(u->headtohead);u->headtohead=NULL;cJSON_Delete(u->friends);u->friends=NULL;u->unread_messages=u->friend_requests=0;u->peer=0;u->message_cursor=0;u->input[0]=0;u->connected_before=false;
    if(online&&g->online&&online_logged_in(g->online)){online_request(g->online,"profile.get",NULL);online_request(g->online,"games",NULL);online_request(g->online,"daily.get",NULL);}
    else {sqlite3_stmt *q=workspace_stmt(u->store,"SELECT nickname,avatar FROM oc_profiles WHERE owner=?");sqlite3_bind_text(q,1,owner,-1,SQLITE_TRANSIENT);if(sqlite3_step(q)==SQLITE_ROW){const char *nick=(const char *)sqlite3_column_text(q,0);if(nick&&*nick)snprintf(u->nickname,sizeof u->nickname,"%s",nick);u->avatar=sqlite3_column_int(q,1);}sqlite3_finalize(q);}
}
static void ui_init(Gui *g)
{
    g->ui=calloc(1,sizeof *g->ui);if(!g->ui)return;Ui21 *u=g->ui;
    u->rated=true;u->background_mask=80;u->nav_hover=-1;u->nav_focus=0;u->current_entry=u->suspended_entry=-1;u->selected_day=workspace_today();u->month_day=u->selected_day;board_reset(&u->analysis_preview);
    const char *lang=getenv("LANG");u->zh=lang&&!strncmp(lang,"zh",2);
    char path[1200];snprintf(path,sizeof path,"%s/workspace.sqlite3",path_data_dir());path_make_parent(path);u->store=workspace_open(path);
    snprintf(path,sizeof path,"%s/official-cache.sqlite3",path_data_dir());u->cache=workspace_open(path);
    if(!g->puzzles){snprintf(path,sizeof path,"%s/puzzles/starter.jsonl",path_assets());g->puzzles=puzzles_load(path);}
    workspace_daily_fill(u->store,g->puzzles,workspace_today());
    u->preview_valid=puzzles_pick(g->puzzles,800,1800,NULL,&u->preview);
    u->opening_preview=g->book&&opening_line_count(g->book)?rand()%opening_line_count(g->book):0;
}
static void ui_close(Gui *g)
{
    Ui21 *u=g->ui;if(!u)return;
    workspace_close(u->store);workspace_close(u->cache);SDL_DestroyTexture(u->logo);SDL_DestroyTexture(u->background);
    cJSON_Delete(u->people);cJSON_Delete(u->messages);cJSON_Delete(u->profile);cJSON_Delete(u->rating_history);cJSON_Delete(u->games);cJSON_Delete(u->calendar);cJSON_Delete(u->invitations);cJSON_Delete(u->headtohead);cJSON_Delete(u->friends);cJSON_Delete(u->analyses);free(u->entries);free(u);g->ui=NULL;
}
static void ui_assets(Gui *g)
{
    if(!g->ui||!g->ren)return;ui_owner(g);char file[1200];snprintf(file,sizeof file,"%s/openchess.png",path_assets());g->ui->logo=IMG_LoadTexture(g->ren,file);
    if(g->ui->background_file[0])g->ui->background=IMG_LoadTexture(g->ren,g->ui->background_file);
}
static void ui_home(Gui *g)
{
    if(g->mode==MODE_ANALYSIS&&g->pgntree)ui_save_analysis(g,"last");
    if(ui_active_game(g)){g->scene=SCENE_MENU;return;}
    /* Account sessions must survive navigation so social updates keep flowing. */
    if(g->mode!=MODE_ONLINE)save_game(g);g->menu_index=g->saved.valid?0:MENU_ANALYSIS;clear_selection(g);g->scene=SCENE_MENU;g->ui->input_focus=false;
}
static void ui_open_analysis(Gui *g)
{
    if(g->mode==MODE_ANALYSIS&&g->pgntree)ui_save_analysis(g,"last");
    if(ui_active_game(g)){snprintf(g->ui->status,256,"%s",ui_tr(g,"Finish the current game before opening another board.","请先完成当前对局，再打开其他棋盘。"));return;}
    g->scene=SCENE_ANALYSIS_HOME;g->ui->input[0]=0;g->ui->input_focus=false;
}
static void ui_analysis_list(Gui *g)
{
    Ui21 *u=g->ui;sqlite3_stmt *q=workspace_stmt(u->store,"SELECT id,title,updated FROM oc_analyses WHERE owner=? AND id!='last' ORDER BY updated DESC LIMIT 50");sqlite3_bind_text(q,1,u->owner,-1,SQLITE_TRANSIENT);cJSON_Delete(u->analyses);u->analyses=workspace_rows(q);
}
static void ui_save_analysis(Gui *g,const char *name)
{
    if(g->mode!=MODE_ANALYSIS||!g->pgntree)return;Ui21 *u=g->ui;
    char id[80];if(!strcmp(name,"last"))snprintf(id,sizeof id,"last");else{unsigned char bytes[8];sqlite3_randomness(8,bytes);for(int k=0;k<8;k++)snprintf(id+k*2,3,"%02x",bytes[k]);}
    char nodes[8192]="";int indices[MAX_PLY],n=0;
    for(MoveNode *node=g->tree_cur;node&&node->parent&&n<MAX_PLY;node=node->parent){int idx=0;for(MoveNode *sib=node->parent->first;sib&&sib!=node;sib=sib->next)idx++;indices[n++]=idx;}
    size_t used=0;for(int k=n-1;k>=0&&used<sizeof nodes-12;k--)used+=(size_t)snprintf(nodes+used,sizeof nodes-used,"%d ",indices[k]);
    char *pgn=pgn_serialize(g->pgntree,&g->pgn_hdr);if(!pgn)return;
    sqlite3_stmt *q=workspace_stmt(u->store,"INSERT INTO oc_analyses VALUES(?,?,?,?,?,?) ON CONFLICT(owner,id) DO UPDATE SET pgn=excluded.pgn,node=excluded.node,title=excluded.title,updated=excluded.updated");
    sqlite3_bind_text(q,1,u->owner,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,2,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,3,g->pgn_hdr.event[0]?g->pgn_hdr.event:"Analysis",-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,4,pgn,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,5,nodes,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(q,6,time(NULL));sqlite3_step(q);sqlite3_finalize(q);free(pgn);ui_analysis_list(g);snprintf(u->status,256,"%s",ui_tr(g,"Analysis saved.","分析已保存。"));
}
static void ui_load_analysis(Gui *g,const char *name)
{
    Ui21 *u=g->ui;char nodes[8192]="";
    const char *sql=!strcmp(name,"saved")?"SELECT pgn,node FROM oc_analyses WHERE owner=? AND id!='last' ORDER BY updated DESC LIMIT 1":"SELECT pgn,node FROM oc_analyses WHERE owner=? AND id=?";
    sqlite3_stmt *q=workspace_stmt(u->store,sql);sqlite3_bind_text(q,1,u->owner,-1,SQLITE_TRANSIENT);if(strcmp(name,"saved"))sqlite3_bind_text(q,2,name,-1,SQLITE_TRANSIENT);
    bool found=sqlite3_step(q)==SQLITE_ROW;if(found){snprintf(g->pgn_text,sizeof g->pgn_text,"%s",sqlite3_column_text(q,0));snprintf(nodes,sizeof nodes,"%s",sqlite3_column_text(q,1));}sqlite3_finalize(q);
    if(!found){snprintf(u->status,256,"%s",ui_tr(g,"No saved analysis yet.","尚无已保存的分析。"));return;}
    pgn_import_load(g);MoveNode *node=g->pgntree;char *save=NULL;
    for(char *t=strtok_r(nodes," ",&save);t&&node;t=strtok_r(NULL," ",&save)){int idx=atoi(t);MoveNode *next=node->first;while(next&&idx-->0)next=next->next;if(!next)break;node=next;}
    if(node){g->tree_cur=node;sync_from_tree(g);analysis_refresh(g);}
}
static void ui_position_open(Gui *g)
{
    const char *tags[]={g->pgn_hdr.event,g->pgn_hdr.site,g->pgn_hdr.date,g->pgn_hdr.white,g->pgn_hdr.black,g->pgn_hdr.result};for(int k=0;k<6;k++)snprintf(g->ui->metadata[k],sizeof g->ui->metadata[k],"%s",tags[k]);
    position_begin(&g->ui->position,&g->ui->analysis_preview);fen_generate(&g->ui->position.board,g->ui->setup_fen,sizeof g->ui->setup_fen);g->scene=SCENE_POSITION_SETUP;g->ui->input_focus=false;g->ui->setup_fen_focus=false;
}
static void ui_position_sync(Gui *g){fen_generate(&g->ui->position.board,g->ui->setup_fen,sizeof g->ui->setup_fen);}
static void ui_entry_load(Gui *g,int index,bool review)
{
    Ui21 *u=g->ui;if(index<0||index>=u->entry_count)return;PuzzleEntry *e=&u->entries[index];
    if(u->current_entry>=0&&!u->reviewing&&u->entry_started){PuzzleEntry *old=&u->entries[u->current_entry];old->step=g->puzzle_step;old->elapsed+=(int)(SDL_GetTicks()-u->entry_started);}
    if(review&&!u->reviewing)u->suspended_entry=u->current_entry;
    u->current_entry=index;u->reviewing=review;u->entry_started=SDL_GetTicks();g->puzzle_cur=e->puzzle;puzzle_split_moves(g);g->mode=MODE_PUZZLE;g->scene=SCENE_GAME;
    fen_parse(e->puzzle.fen,&g->board);g->ply=0;reset_board_state(g);g->puzzle_step=0;
    int step=review?1:(e->step>0?e->step:1);
    for(int k=0;k<step&&k<g->puzzle_nmoves;k++){Move m;if(uci_to_move(&g->board,g->puzzle_moves[k],&m))push_move(g,m);g->puzzle_step++;}
    g->anim_count=0;g->human_color=g->board.side;g->flipped=g->human_color==BLACK;g->auto_flip=false;g->puzzle_done=false;g->puzzle_failed=false;g->puzzle_counted=review||e->settled;g->puzzle_delta=e->delta;clear_selection(g);fen_refresh(g);
    snprintf(g->white_name,sizeof g->white_name,"Puzzle");snprintf(g->black_name,sizeof g->black_name,"You");
}
static int ui_entry_add(Gui *g,const Puzzle *p,const char *attempt,int day,bool rated)
{
    Ui21 *u=g->ui;if(u->entry_count==u->entry_capacity){int cap=u->entry_capacity?u->entry_capacity*2:32;PuzzleEntry *entries=realloc(u->entries,cap*sizeof *entries);if(!entries)return -1;u->entries=entries;u->entry_capacity=cap;}
    int k=u->entry_count++;PuzzleEntry *e=&u->entries[k];memset(e,0,sizeof *e);e->puzzle=*p;e->day=day;e->rated=rated&&!day;e->step=1;snprintf(e->attempt,sizeof e->attempt,"%s",attempt);return k;
}
static bool ui_connected(Gui *g){return g->online&&online_logged_in(g->online)&&online_state(g->online)!=ONLINE_CLOSED&&!online_reconnecting(g->online)&&!g->offline_mode;}
static Workspace *ui_daily_store(Gui *g){return !strncmp(g->ui->owner,"online:",7)?g->ui->cache:g->ui->store;}
static void ui_cache_puzzle(Gui *g,int day,const Puzzle *p)
{
    cJSON *j=workspace_puzzle_json(p);char *snapshot=cJSON_PrintUnformatted(j);cJSON_Delete(j);sqlite3_stmt *q=workspace_stmt(g->ui->cache,"INSERT OR IGNORE INTO oc_daily VALUES(?,?)");sqlite3_bind_int(q,1,day);sqlite3_bind_text(q,2,snapshot,-1,SQLITE_TRANSIENT);sqlite3_step(q);sqlite3_finalize(q);free(snapshot);
}
static void ui_next_puzzle(Gui *g,int day)
{
    Ui21 *u=g->ui;if(ui_active_game(g)){snprintf(u->status,256,"%s",ui_tr(g,"Return to your active game first.","请先完成当前对局。"));return;}
    if(ui_connected(g)){cJSON *args=cJSON_CreateObject();cJSON_AddNumberToObject(args,"day",day);cJSON_AddBoolToObject(args,"smart",u->smart);cJSON_AddBoolToObject(args,"rated",u->rated);cJSON_AddNumberToObject(args,"difficulty",g->puzzle_band);if(g->puzzle_theme>0)cJSON_AddStringToObject(args,"theme",PUZZLE_THEMES[g->puzzle_theme]);online_request(g->online,"puzzle.next",args);return;}
    if(!strncmp(u->owner,"online:",7)){
        Puzzle cached;if(!day||!workspace_daily(u->cache,day,&cached)){snprintf(u->status,256,"%s",ui_tr(g,"Connect to load this official puzzle.","请联网获取这道官方每日题。"));return;}
        int idx=ui_entry_add(g,&cached,"cached-review",day,false);if(idx>=0){u->entries[idx].settled=true;ui_entry_load(g,idx,true);}snprintf(u->status,256,"%s",ui_tr(g,"Offline archive review · No scoring","离线历史回看 · 不计分"));return;
    }
    Puzzle p;char reason[128]="";bool ok;
    if(day){workspace_daily_fill(u->store,g->puzzles,workspace_today());ok=day<=workspace_today()&&workspace_daily(u->store,day,&p);}
    else if(u->smart)ok=workspace_pick(u->store,g->puzzles,u->owner,g->puzzle_rating,true,g->puzzle_theme>0?PUZZLE_THEMES[g->puzzle_theme]:NULL,&p,reason);
    else ok=workspace_pick_level(g->puzzles,g->puzzle_rating,g->puzzle_band,g->puzzle_theme>0?PUZZLE_THEMES[g->puzzle_theme]:NULL,&p);
    if(!ok){snprintf(u->status,256,"%s",ui_tr(g,"Puzzle unavailable.","该日期或条件下没有题目。"));return;}
    unsigned char bytes[16];sqlite3_randomness(16,bytes);char id[40];for(int k=0;k<16;k++)snprintf(id+2*k,3,"%02x",bytes[k]);
    bool rated=u->rated&&!day&&strncmp(reason,"Review:",7);workspace_issue(u->store,u->owner,id,&p,rated,day);int index=ui_entry_add(g,&p,id,day,rated);if(index>=0)ui_entry_load(g,index,false);snprintf(u->status,256,"%s",reason);
}
static void ui_calendar_refresh(Gui *g);
static void ui_puzzle_settle(Gui *g,bool solved)
{
    Ui21 *u=g->ui;if(!u||u->reviewing||u->current_entry<0)return;PuzzleEntry *e=&u->entries[u->current_entry];if(e->settled||e->pending)return;
    if(e->day&&!solved&&!e->reveal)return;
    e->played[0]=0;if(solved)for(int k=0;k<g->puzzle_step&&k<g->puzzle_nmoves;k++){if(k)strncat(e->played," ",sizeof e->played-strlen(e->played)-1);strncat(e->played,g->puzzle_moves[k],sizeof e->played-strlen(e->played)-1);}
    if(u->entry_started)e->elapsed+=(int)(SDL_GetTicks()-u->entry_started);u->entry_started=SDL_GetTicks();e->step=g->puzzle_step;
    if(!strncmp(u->owner,"online:",7)){cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"attempt",e->attempt);cJSON_AddStringToObject(q,"moves",e->played);cJSON_AddBoolToObject(q,"hint",e->hint);cJSON_AddBoolToObject(q,"reveal",e->reveal);cJSON_AddNumberToObject(q,"elapsed",e->elapsed);e->pending=true;if(ui_connected(g))online_request(g->online,"puzzle.submit",q);else cJSON_Delete(q);}
    else{cJSON *r=workspace_submit(u->store,u->owner,e->attempt,e->played,e->hint,e->reveal,e->elapsed);const cJSON *v=cJSON_GetObjectItem(r,"error");if(!v){e->settled=true;e->rated=cJSON_IsTrue(cJSON_GetObjectItem(r,"rated"));e->solved=cJSON_IsTrue(cJSON_GetObjectItem(r,"solved"));e->delta=cJSON_GetObjectItem(r,"delta")->valueint;g->puzzle_rating=cJSON_GetObjectItem(r,"rating")->valueint;g->puzzle_delta=e->delta;}cJSON_Delete(r);if(e->day)ui_calendar_refresh(g);}
}
static void ui_calendar_refresh(Gui *g)
{
    Ui21 *u=g->ui;char date[11];workspace_date(u->month_day,date);memcpy(date+8,"01",3);int first=workspace_day(date);int end=first+31;char last[11];workspace_date(end,last);memcpy(last+8,"01",3);end=workspace_day(last)-1;
    if(ui_connected(g)){cJSON *q=ui_args_int("first",first);cJSON_AddNumberToObject(q,"last",end);online_request(g->online,"daily.calendar",q);}
    else {cJSON_Delete(u->calendar);u->calendar=workspace_calendar(ui_daily_store(g),u->owner,first,end);workspace_streak(ui_daily_store(g),u->owner,workspace_today(),&u->daily_current,&u->daily_longest);}
}
static void ui_open_daily(Gui *g,int day)
{
    Ui21 *u=g->ui;u->selected_day=day;u->month_day=day;u->calendar_open=true;u->page=1;ui_calendar_refresh(g);
    ui_next_puzzle(g,day);
}
static void ui_receive(Gui *g,cJSON *r)
{
    Ui21 *u=g->ui;const cJSON *actor=cJSON_GetObjectItem(r,"actor");if(cJSON_IsString(actor)&&strcmp(actor->valuestring,g->account_user))return;const cJSON *opj=cJSON_GetObjectItem(r,"op");const char *op=cJSON_IsString(opj)?opj->valuestring:"";
    const cJSON *peer=cJSON_GetObjectItem(r,"peer");if((!strcmp(op,"messages")||!strcmp(op,"message.send")||!strcmp(op,"message.read"))&&cJSON_IsNumber(peer)&&peer->valueint!=u->peer)return;
    if(!strcmp(op,"messages")||!strcmp(op,"message.read")){const cJSON *offset=cJSON_GetObjectItem(r,"offset");if(cJSON_IsNumber(offset)&&offset->valueint!=u->message_cursor)return;}
    if(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok"))){const cJSON *err=cJSON_GetObjectItem(r,"error");snprintf(u->status,256,"%s",cJSON_IsString(err)?err->valuestring:"Request failed");return;}
    cJSON **target=NULL;
    if(!strcmp(op,"people")||!strcmp(op,"blocks")||(!strcmp(op,"friends")&&u->social_tab==0)){
        const cJSON *off=cJSON_GetObjectItem(r,"offset"),*filter=cJSON_GetObjectItem(r,"search");if((!cJSON_IsNumber(off)||off->valueint==u->offset)&&(!cJSON_IsString(filter)||!strcmp(filter->valuestring,u->search)))target=&u->people;
    }
    else if(!strcmp(op,"messages")||!strcmp(op,"message.send")||!strcmp(op,"message.read"))target=&u->messages;
    else if(!strcmp(op,"games"))target=&u->games;
    else if(!strcmp(op,"profile.get"))target=&u->profile;
    else if(!strcmp(op,"ratings.history"))target=&u->rating_history;
    else if(!strcmp(op,"challenge.list"))target=&u->invitations;
    else if(!strcmp(op,"headtohead"))target=&u->headtohead;
    if(target){cJSON_Delete(*target);*target=cJSON_Duplicate(r,true);}
    if(!strcmp(op,"friends")){cJSON_Delete(u->friends);u->friends=cJSON_Duplicate(r,true);}
    const cJSON *notify=cJSON_GetArrayItem(cJSON_GetObjectItem(r,"notifications"),0),*pending=cJSON_GetArrayItem(cJSON_GetObjectItem(r,"requests"),0);
    if(notify)u->unread_messages=cJSON_GetObjectItem(notify,"unread")->valueint;
    if(pending)u->friend_requests=cJSON_GetObjectItem(pending,"pending")->valueint;
    if((!strcmp(op,"messages")||!strcmp(op,"message.send"))&&g->scene==SCENE_SOCIAL){const cJSON *latest=cJSON_GetArrayItem(cJSON_GetObjectItem(r,"rows"),0);if(latest){cJSON *q=ui_args_int("peer",u->peer);cJSON_AddNumberToObject(q,"through",cJSON_GetObjectItem(latest,"id")->valueint);cJSON_AddNumberToObject(q,"offset",u->message_cursor);online_request(g->online,"message.read",q);}}
    if(cJSON_IsNumber(cJSON_GetObjectItem(r,"self")))u->self_id=cJSON_GetObjectItem(r,"self")->valueint;
    if(!strcmp(op,"daily.calendar")){cJSON_Delete(u->calendar);u->calendar=cJSON_Duplicate(cJSON_GetObjectItem(r,"rows"),true);u->daily_current=cJSON_GetObjectItem(r,"current")->valueint;u->daily_longest=cJSON_GetObjectItem(r,"longest")->valueint;
        const cJSON *row=NULL;cJSON_ArrayForEach(row,u->calendar){sqlite3_stmt *q=workspace_stmt(u->cache,"INSERT INTO oc_daily_progress(owner,day,completed,practiced) VALUES(?,?,?,?) ON CONFLICT(owner,day) DO UPDATE SET completed=excluded.completed,practiced=excluded.practiced");sqlite3_bind_text(q,1,u->owner,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,2,cJSON_GetObjectItem(row,"day")->valueint);sqlite3_bind_int(q,3,cJSON_GetObjectItem(row,"completed")->valueint);sqlite3_bind_int(q,4,cJSON_GetObjectItem(row,"practiced")->valueint);sqlite3_step(q);sqlite3_finalize(q);}
    }
    if(!strcmp(op,"daily.get")){Puzzle p;const cJSON *day=cJSON_GetObjectItem(r,"day");if(cJSON_IsNumber(day)&&workspace_puzzle_parse(cJSON_GetObjectItem(r,"puzzle"),&p))ui_cache_puzzle(g,day->valueint,&p);}
    if(!strcmp(op,"puzzle.next")){Puzzle p;const cJSON *a=cJSON_GetObjectItem(r,"attempt"),*d=cJSON_GetObjectItem(r,"day");if(workspace_puzzle_parse(cJSON_GetObjectItem(r,"puzzle"),&p)&&cJSON_IsString(a)){int day=cJSON_IsNumber(d)?d->valueint:0;if(day)ui_cache_puzzle(g,day,&p);int idx=ui_entry_add(g,&p,a->valuestring,day,cJSON_IsTrue(cJSON_GetObjectItem(r,"rated")));if(idx>=0)ui_entry_load(g,idx,false);}}
    if(!strcmp(op,"puzzle.mode")){
        const cJSON *attempt=cJSON_GetObjectItem(r,"attempt");
        if(cJSON_IsString(attempt))for(int k=0;k<u->entry_count;k++)if(!strcmp(u->entries[k].attempt,attempt->valuestring)&&cJSON_IsTrue(cJSON_GetObjectItem(r,"ok"))){u->entries[k].rated=cJSON_IsTrue(cJSON_GetObjectItem(r,"rated"));break;}
    }
    if(!strcmp(op,"puzzle.submit")){const cJSON *result=cJSON_GetObjectItem(r,"result"),*a=cJSON_GetObjectItem(result,"attempt");if(cJSON_IsString(a))for(int k=0;k<u->entry_count;k++){PuzzleEntry *e=&u->entries[k];if(strcmp(e->attempt,a->valuestring))continue;e->pending=false;if(cJSON_GetObjectItem(result,"error"))break;e->settled=true;e->rated=cJSON_IsTrue(cJSON_GetObjectItem(result,"rated"));e->solved=cJSON_IsTrue(cJSON_GetObjectItem(result,"solved"));e->delta=cJSON_GetObjectItem(result,"delta")->valueint;g->puzzle_rating=cJSON_GetObjectItem(result,"rating")->valueint;g->puzzle_delta=e->delta;break;}ui_calendar_refresh(g);}
    if(!strcmp(op,"game.get")){const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(r,"rows"),0),*pgn=cJSON_GetObjectItem(row,"pgn");if(cJSON_IsString(pgn)){snprintf(g->pgn_text,sizeof g->pgn_text,"%s",pgn->valuestring);pgn_import_load(g);}}
    if(!strcmp(op,"profile.set"))online_request(g->online,"profile.get",NULL);
    if(!strcmp(op,"friend.action"))online_request(g->online,ui_social_op(u),NULL);
    if(!strcmp(op,"profile.get")){const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(r,"rows"),0),*nick=cJSON_GetObjectItem(row,"nickname"),*av=cJSON_GetObjectItem(row,"avatar");if(cJSON_IsString(nick)&&*nick->valuestring)snprintf(u->nickname,sizeof u->nickname,"%s",nick->valuestring);if(cJSON_IsNumber(av))u->avatar=av->valueint;}
    if(!strcmp(op,"profile.get")){sqlite3_stmt *q=workspace_stmt(u->store,"UPDATE oc_profiles SET nickname=?,avatar=? WHERE owner=?");sqlite3_bind_text(q,1,u->nickname,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,2,u->avatar);sqlite3_bind_text(q,3,u->owner,-1,SQLITE_TRANSIENT);sqlite3_step(q);sqlite3_finalize(q);}

}
static void ui_local_history(Gui *g)
{
    Ui21 *u=g->ui;if(g->scene!=SCENE_GAME||(g->mode!=MODE_SINGLE&&g->mode!=MODE_LOCAL))return;
    if(g->state==NO_GAME_OVER&&!g->override_result[0]){u->local_game_saved=false;return;}
    if(u->local_game_saved||!g->ply)return;u->local_game_saved=true;
    const char *result=g->override_result[0]?g->override_result:pgn_result(&g->board,g->state);
    FILE *file=tmpfile();if(!file)return;pgn_write(file,(const char (*)[8])g->move_san,g->ply,"OpenChess","Local","????.??.??",1,g->white_name,g->black_name,result);long size=ftell(file);rewind(file);char *pgn=calloc((size_t)size+1,1);if(!pgn){fclose(file);return;}fread(pgn,1,(size_t)size,file);fclose(file);
    unsigned char random[16];sqlite3_randomness(16,random);char id[40];for(int k=0;k<16;k++)snprintf(id+k*2,3,"%02x",random[k]);
    sqlite3_stmt *q=workspace_stmt(u->store,"INSERT OR IGNORE INTO oc_games VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");const char *text[]={id,u->owner,"local-opponent",g->white_name,g->black_name,result,pgn};for(int k=0;k<7;k++)sqlite3_bind_text(q,k+1,text[k],-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,8,0);sqlite3_bind_int(q,9,0);sqlite3_bind_int(q,10,0);sqlite3_bind_int64(q,11,time(NULL));sqlite3_bind_int(q,12,0);sqlite3_bind_text(q,13,id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(q,14,g->ply);sqlite3_step(q);sqlite3_finalize(q);free(pgn);cJSON_Delete(u->games);u->games=NULL;
}
static void ui_game_history_open(Gui *g,const char *id)
{
    if(!strncmp(g->ui->owner,"online:",7)){cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"id",id);ui_request(g,"game.get",q);return;}
    sqlite3_stmt *q=workspace_stmt(g->ui->store,"SELECT pgn FROM oc_games WHERE id=? AND (white=? OR black=?)");sqlite3_bind_text(q,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,2,g->ui->owner,-1,SQLITE_TRANSIENT);sqlite3_bind_text(q,3,g->ui->owner,-1,SQLITE_TRANSIENT);bool found=sqlite3_step(q)==SQLITE_ROW;if(found)snprintf(g->pgn_text,sizeof g->pgn_text,"%s",sqlite3_column_text(q,0));sqlite3_finalize(q);if(found)pgn_import_load(g);
}
static void ui_tick(Gui *g,Uint32 now)
{
    if(!g->ui)return;
    if(!g->offline_mode&&!g->account_local&&g->account_token[0]&&!g->online)online_ensure(g);
    ui_owner(g);Ui21 *u=g->ui;ui_local_history(g);
    bool active_puzzle=g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE&&!u->reviewing&&u->current_entry>=0;
    if(active_puzzle&&!u->entry_started)u->entry_started=now;
    if(!active_puzzle&&u->entry_started){if(u->current_entry>=0&&!u->reviewing)u->entries[u->current_entry].elapsed+=(int)(now-u->entry_started);u->entry_started=0;}
    bool connected=ui_connected(g);
    if(connected&&!u->connected_before){online_request(g->online,"profile.get",NULL);online_request(g->online,"games",NULL);online_request(g->online,"daily.get",NULL);ui_calendar_refresh(g);
        for(int k=0;k<u->entry_count;k++){PuzzleEntry *entry=&u->entries[k];if(!entry->pending)continue;cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"attempt",entry->attempt);cJSON_AddStringToObject(q,"moves",entry->played);cJSON_AddBoolToObject(q,"hint",entry->hint);cJSON_AddBoolToObject(q,"reveal",entry->reveal);cJSON_AddNumberToObject(q,"elapsed",entry->elapsed);online_request(g->online,"puzzle.submit",q);}
    }
    u->connected_before=connected;
    if(g->online){cJSON *r;while((r=online_take_response(g->online))){ui_receive(g,r);cJSON_Delete(r);}}
    if(g->online&&online_logged_in(g->online)&&now-u->last_refresh>5000){u->last_refresh=now;online_request(g->online,"challenge.list",NULL);online_request(g->online,"friends",NULL);if(!workspace_daily(u->cache,workspace_today(),&(Puzzle){0}))online_request(g->online,"daily.get",NULL);if(g->scene==SCENE_SOCIAL){cJSON *args=ui_args_int("offset",u->offset);cJSON_AddStringToObject(args,"search",u->search);online_request(g->online,ui_social_op(u),args);if(u->peer){cJSON *q=ui_args_int("peer",u->peer);cJSON_AddNumberToObject(q,"offset",u->message_cursor);online_request(g->online,"messages",q);}}}
    if(u->current_entry>=0&&!u->reviewing&&g->mode==MODE_PUZZLE)u->entries[u->current_entry].step=g->puzzle_step;
}
static void ui_nav_action(Gui *g,int group,int item)
{
    Ui21 *u=g->ui;if(g->mode==MODE_ANALYSIS&&g->pgntree&&g->scene==SCENE_GAME)ui_save_analysis(g,"last");u->nav_hover=-1;u->input_focus=false;
    if(ui_active_game(g)&&(group==1||group==2||(group==0&&(item==1||item==2)))){snprintf(u->status,256,"%s",ui_tr(g,"Finish the active game before opening another board.","请先完成当前对局，再打开其他棋盘。"));return;}
    if(group==0){if(item==0){g->scene=SCENE_ONLINE;g->online_ui=1;online_ensure(g);}else if(item==1)g->scene=SCENE_SINGLE_SETUP;else if(item==2)g->scene=SCENE_HOSTJOIN;else {g->scene=SCENE_LIBRARY;ui_request(g,"games",NULL);}}
    else if(group==1){if(item==1){ui_open_daily(g,workspace_today());return;}u->smart=item==2;u->page=0;g->scene=SCENE_PUZZLE_SETUP;}
    else if(group==2){if(item==0)ui_open_analysis(g);else if(item==1)open_openings(g);else{g->scene=SCENE_LIBRARY;u->page=1;ui_analysis_list(g);}}
    else if(group==3){g->scene=SCENE_ONLINE;g->online_ui=0;online_ensure(g);}
    else if(group==4){if(item==2){g->scene=SCENE_PROFILE;ui_request(g,"profile.get",NULL);ui_request(g,"ratings.history",ui_args_int("category",u->rating_class));}else{g->scene=SCENE_SOCIAL;u->social_tab=item==0?0:1;ui_request(g,ui_social_op(u),NULL);}}
    else if(item==0)g->scene=SCENE_PREFERENCES;else if(item==1)open_settings_scene(g,SCENE_MENU);else if(item==2)open_appearance(g,SCENE_MENU);else open_account(g);
}
static const char *ui_nav_name(Gui *g,int group){static const char *en[]={"Play","Puzzles","Learn","Watch","Community","More"},*zh[]={"对弈","解题","学习","观战","社区","更多"};return g->ui->zh?zh[group]:en[group];}
static const char *ui_sub_name(Gui *g,int group,int item)
{
    static const char *en[6][4]={{"Play Online","Play Computer","Local Multiplayer","Game History"},{"Practice","Daily Puzzle","Smart Practice",NULL},{"Analysis","Opening Explorer","Saved Analysis",NULL},{"Spectate by Room Code",NULL},{"Friends & Messages","Online Players","My Profile",NULL},{"Preferences","Engine & Game Settings","Appearance","Account"}};
    static const char *zh[6][4]={{"在线对弈","电脑对战","局域网对战","历史对局"},{"普通练习","每日题","智能练习",NULL},{"分析","开局浏览","已保存分析",NULL},{"输入房间号观战",NULL},{"好友与私信","在线玩家","个人主页",NULL},{"偏好设置","引擎与对局设置","棋盘外观","账号"}};
    return g->ui->zh?zh[group][item]:en[group][item];
}
static void ui_shell(Gui *g)
{
    Ui21 *u=g->ui;SDL_RenderSetClipRect(g->ren,NULL);apply_render_scale(g);ui_round(g->ren,(SDL_Rect){0,0,208,g->win_h},(SDL_Color){218,234,247,255},0);
    if(u->logo)SDL_RenderCopy(g->ren,u->logo,NULL,&(SDL_Rect){18,22,36,36});
    render_text(g,g->font_ui,"OpenChess",60,27,ui_ink);
    ui_text(g,OPENCHESS_VERSION,24,68,ui_muted);
    for(int k=0;k<6;k++){SDL_Rect r={12,112+k*58,184,48};if(k==u->nav_hover)ui_round(g->ren,r,(SDL_Color){192,216,240,255},10);ui_icon(g,(int[]){10,11,12,13,7,14}[k],r.x+12,r.y+14,ui_muted);ui_text(g,ui_nav_name(g,k),r.x+42,r.y+14,ui_ink);}
    ui_round(g->ren,(SDL_Rect){16,g->win_h-140,40,40},(SDL_Color){123+(u->avatar%3)*28,166,207,255},12);
    render_piece_box(g,g->ren,(Piece)(WP+u->avatar%12),36,g->win_h-120,32,1);
    SDL_Rect clip={62,g->win_h-143,134,47};SDL_RenderSetClipRect(g->ren,&clip);ui_text(g,u->nickname,64,g->win_h-136,ui_ink);SDL_RenderSetClipRect(g->ren,NULL);
    ui_text(g,ui_tr(g,g->offline_mode?"Offline":"Online",g->offline_mode?"离线模式":"在线模式"),24,g->win_h-87,ui_muted);
    const char *icons_en[]={"Friends","Chat","Invites","Prefs"},*icons_zh[]={"好友","私信","约战","设置"};int icons[]={7,6,9,8};
    for(int k=0;k<4;k++){SDL_Rect hit={8+k*49,g->win_h-58,45,44};if(ui_hit(g,hit))ui_round(g->ren,hit,(SDL_Color){192,216,240,255},8);ui_icon(g,icons[k],hit.x+12,hit.y+3,ui_ink);render_text_centered(g,g->font_tiny,u->zh?icons_zh[k]:icons_en[k],hit.x+hit.w/2,hit.y+27,ui_muted);
        int badge=k==0?u->friend_requests:k==1?u->unread_messages:k==2?cJSON_GetArraySize(cJSON_GetObjectItem(u->invitations,"rows")):0;if(badge){ui_round(g->ren,(SDL_Rect){hit.x+29,hit.y-4,18,18},(SDL_Color){202,71,78,255},9);char n[8];snprintf(n,8,"%d",badge>99?99:badge);render_text(g,g->font_tiny,n,hit.x+32,hit.y-4,(SDL_Color){255,255,255,255});}}
    if(g->scene==SCENE_MENU&&g->saved.valid&&!ui_active_game(g))ui_button(g,(SDL_Rect){g->win_w-252,20,228,42},ui_tr(g,"Continue game","继续上次对局"),-1,true);
    if(ui_active_game(g)){int white,black;online_clocks(g->online,&white,&black);char label[100];snprintf(label,sizeof label,u->zh?"返回对局 %d:%02d / %d:%02d":"Return %d:%02d / %d:%02d",white/60000,(white/1000)%60,black/60000,(black/1000)%60);ui_button(g,(SDL_Rect){g->win_w-252,20,228,42},label,-1,true);}
    if(u->composition[0])ui_text(g,u->composition,240,g->win_h-60,ui_ink);
    if(u->status[0]){SDL_Rect status={230,g->win_h-30,g->win_w-250,26};SDL_RenderSetClipRect(g->ren,&status);ui_text(g,u->status,status.x,status.y,ui_muted);SDL_RenderSetClipRect(g->ren,NULL);}
    int group=u->nav_hover;if(group>=0){int count=0;while(count<4&&ui_sub_name(g,group,count))count++;SDL_Rect pop={204,112+group*58,272,count*50+12};ui_card(g,pop);for(int k=0;k<count;k++)ui_button(g,(SDL_Rect){212,pop.y+6+k*50,256,44},ui_sub_name(g,group,k),k%5,false);}
}
typedef struct {SDL_Rect match,cards[3],history,daily;int height;bool compact;} HomeLayout;
static HomeLayout ui_home_layout(Gui *g)
{
    int width=WIN_W,height=WIN_H;if(g->win)SDL_GetWindowSize(g->win,&width,&height);(void)height;
    HomeLayout l={0};l.compact=width<1200;l.match=(SDL_Rect){240,112,282,424};
    int columns=l.compact?2:3,cardw=(g->win_w-542-28)/columns;
    for(int k=0;k<3;k++)l.cards[k]=(SDL_Rect){542+(k%columns)*(cardw+6),112+(k/columns)*454,cardw-6,424};
    l.history=l.compact?(SDL_Rect){240,1020,g->win_w-268,280}:(SDL_Rect){240,566,800,280};
    l.daily=l.compact?(SDL_Rect){l.cards[1].x,566,l.cards[1].w,424}:(SDL_Rect){1060,566,g->win_w-1088,280};
    l.height=l.compact?1340:900;return l;
}
static void ui_home_render(Gui *g)
{
    Ui21 *u=g->ui;HomeLayout layout=ui_home_layout(g);u->home_scroll=clampi(u->home_scroll,0,layout.height-g->win_h);SDL_Rect viewport;SDL_RenderGetViewport(g->ren,&viewport);SDL_Rect content=viewport;content.y-=u->home_scroll;content.h+=u->home_scroll;SDL_RenderSetViewport(g->ren,&content);SDL_RenderSetScale(g->ren,eff_scale(g),eff_scale(g));int left=240,top=112;
    render_text(g,g->font_ui,ui_tr(g,"Your chess workspace","你的国际象棋空间"),left,32,ui_ink);
    ui_card(g,(SDL_Rect){left,top,282,424});ui_text(g,ui_tr(g,g->offline_mode?"Play offline":"Play online",g->offline_mode?"离线对弈":"在线对弈"),left+18,top+18,ui_ink);
    static const int times[][2]={{60000,0},{120000,1000},{180000,0},{180000,2000},{300000,0},{300000,3000},{600000,0},{600000,5000},{900000,10000},{1800000,0}};
    if(!g->offline_mode){for(int k=0;k<10;k++){char label[24];snprintf(label,24,"%d + %d",times[k][0]/60000,times[k][1]/1000);ui_button(g,(SDL_Rect){left+16+(k%2)*126,top+58+(k/2)*51,118,42},label,-1,false);}
        ui_button(g,(SDL_Rect){left+16,top+326,250,44},ui_tr(g,"Challenge a player","约战在线玩家"),0,true);
    }else{ui_button(g,(SDL_Rect){left+16,top+75,250,50},ui_tr(g,"Play computer","电脑对战"),0,true);ui_button(g,(SDL_Rect){left+16,top+145,250,50},ui_tr(g,"Local multiplayer","局域网对战"),-1,false);}
    ui_button(g,(SDL_Rect){left+16,top+378,250,34},ui_tr(g,g->offline_mode?"Switch online":"Switch offline",g->offline_mode?"切换在线模式":"切换离线模式"),-1,false);
    for(int k=0;k<3;k++){
        SDL_Rect card=layout.cards[k];int width=card.w+6,cx=card.x;top=card.y;ui_card(g,card);
        Board b;board_reset(&b);if(k==0&&u->preview_valid)ui_preview_position(&u->preview,&b);
        if(k==2&&g->book){char eco[12],name[128],moves[2048];if(opening_line_at(g->book,u->opening_preview,eco,sizeof eco,name,sizeof name,moves,sizeof moves)){char *save=NULL;int ply=0;for(char *t=strtok_r(moves," ",&save);t&&ply<12;t=strtok_r(NULL," ",&save))if(san_apply(&b,t))ply++;}}
        int size=((width-34)/8)*8;if(size>256)size=256;ui_mini_board(g,&b,(SDL_Rect){cx+12,top+16,size,size},false);
        const char *en[]={"Puzzles","Analysis","Opening"},*zh[]={"战术解题","棋局分析","开局探索"};render_text(g,g->font_ui,u->zh?zh[k]:en[k],cx+14,top+size+42,ui_ink);
        ui_button(g,(SDL_Rect){cx+12,top+354,width-30,48},ui_tr(g,"Open","打开"),k==1?2:0,k==0);
    }
    int lower=layout.history.y;ui_card(g,layout.history);ui_text(g,ui_tr(g,"Game History","历史对局"),left+20,lower+18,ui_ink);
    const cJSON *rows=u->games?cJSON_GetObjectItem(u->games,"rows"):NULL;
    if(!rows){sqlite3_stmt *s=workspace_stmt(u->store,"SELECT id,white_name,black_name,result,base,inc,created,plies FROM oc_games WHERE white=? OR black=? ORDER BY created DESC LIMIT 4");sqlite3_bind_text(s,1,u->owner,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,u->owner,-1,SQLITE_TRANSIENT);cJSON *j=workspace_rows(s);cJSON_Delete(u->games);u->games=cJSON_CreateObject();cJSON_AddItemToObject(u->games,"rows",j);rows=j;}
    int count=cJSON_GetArraySize(rows);if(!count)ui_text(g,ui_tr(g,"Completed games will appear here.","完成的对局将在这里显示。"),left+20,lower+76,ui_muted);
    for(int k=0;k<count&&k<4;k++){const cJSON *row=cJSON_GetArrayItem(rows,k);const cJSON *a=cJSON_GetObjectItem(row,"white_name"),*b=cJSON_GetObjectItem(row,"black_name"),*r=cJSON_GetObjectItem(row,"result");char label[220];snprintf(label,sizeof label,"%s  /  %s       %s",cJSON_IsString(a)?a->valuestring:"?",cJSON_IsString(b)?b->valuestring:"?",cJSON_IsString(r)?r->valuestring:"*");ui_button(g,(SDL_Rect){left+16,lower+59+k*48,layout.history.w-32,40},label,-1,false);}
    int dx=layout.daily.x,dw=layout.daily.w;lower=layout.daily.y;ui_card(g,layout.daily);ui_text(g,ui_tr(g,"Daily Puzzle · Unrated","每日题 · 不计分"),dx+16,lower+15,ui_ink);
    Puzzle daily;Board b;board_reset(&b);if(workspace_daily(ui_daily_store(g),workspace_today(),&daily))ui_preview_position(&daily,&b);
    ui_mini_board(g,&b,(SDL_Rect){dx+16,lower+48,160,160},false);char date[11];workspace_date(workspace_today(),date);ui_text(g,date,dx+190,lower+70,ui_muted);
    ui_button(g,(SDL_Rect){dx+16,lower+223,dw-32,42},ui_tr(g,"Solve & calendar","解题与历史日历"),0,true);
    SDL_RenderSetViewport(g->ren,&viewport);SDL_RenderSetScale(g->ren,eff_scale(g),eff_scale(g));
}
static void ui_analysis_render(Gui *g,bool setup)
{
    Ui21 *u=g->ui;Board copy=setup?u->position.board:u->analysis_preview;Board *b=&copy;if(setup&&u->position.dragging>=0)b->board[u->position.dragging]=EMPTY;
    ui_mini_board(g,b,(SDL_Rect){g->board_x,g->board_y,8*g->sq,8*g->sq},g->flipped);
    ui_text(g,ui_tr(g,"Black","黑方"),g->board_x,62,ui_muted);ui_text(g,ui_tr(g,"White","白方"),g->board_x,8*g->sq+g->board_y+14,ui_muted);
    int x=g->panel_x,w=g->win_w-x-24;ui_card(g,(SDL_Rect){x,88,w,setup?760:720});
    render_text(g,g->font_ui,ui_tr(g,setup?"Setup Position":"Analysis",setup?"自定义局面":"棋局分析"),x+20,104,ui_ink);
    if(!setup){
        SDL_Rect input={x+18,154,w-36,76};ui_round(g->ren,input,(SDL_Color){226,237,248,255},8);
        SDL_RenderSetClipRect(g->ren,&input);ui_text(g,u->input[0]?u->input:ui_tr(g,"Paste PGN or FEN...","粘贴 PGN 或 FEN……"),input.x+10,input.y+12,ui_muted);SDL_RenderSetClipRect(g->ren,NULL);
        ui_button(g,(SDL_Rect){x+18,242,(w-44)/2,44},ui_tr(g,"Load","加载"),0,true);
        ui_button(g,(SDL_Rect){x+26+(w-44)/2,242,(w-44)/2,44},ui_tr(g,"Choose file","选择文件"),3,false);
        const char *en[]={"Set Up Position","New Analysis","Saved Analysis","Load Previous","Game History","Opening Explorer"};
        const char *zh[]={"自定义局面","新分析","已保存分析","恢复上次分析","历史对局","开局浏览"};
        for(int k=0;k<6;k++)ui_button(g,(SDL_Rect){x+18,310+k*73,w-36,60},u->zh?zh[k]:en[k],k==0?4:k==1?2:3,k==0);
    }else{
        for(int k=0;k<12;k++){
            int col=k%6,row=k/6;SDL_Rect r={x+18+col*(w-36)/6,156+row*64,(w-40)/6,60};
            Piece pc=(Piece)(k<6?BP+k:WP+k-6);if(u->position.brush==pc)ui_round(g->ren,r,(SDL_Color){186,216,245,255},8);
            render_piece_box(g,g->ren,pc,r.x+r.w/2.0f,r.y+30,46,1);
        }
        ui_button(g,(SDL_Rect){x+18,296,w-36,40},ui_tr(g,b->side==WHITE?"White to move":"Black to move",b->side==WHITE?"白方行棋":"黑方行棋"),-1,false);
        const char *rights[]={"White O-O","White O-O-O","Black O-O","Black O-O-O"};
        for(int k=0;k<4;k++){char text[80];snprintf(text,sizeof text,"%s %s",b->castling&(1<<k)?"[x]":"[ ]",rights[k]);ui_button(g,(SDL_Rect){x+18+(k%2)*(w-34)/2,350+(k/2)*44,(w-42)/2,36},text,-1,false);}
        SDL_Rect fen={x+18,448,w-36,58};ui_round(g->ren,fen,(SDL_Color){226,237,248,255},8);SDL_RenderSetClipRect(g->ren,&fen);ui_text(g,u->setup_fen,fen.x+8,fen.y+12,ui_ink);SDL_RenderSetClipRect(g->ren,NULL);
        ui_text(g,ui_tr(g,"FEN: side, castling, en passant, halfmove, fullmove","FEN：行棋方、易位、过路兵与回合计数"),x+18,514,ui_muted);
        const char *en[]={"Initial","Clear","Undo","Redo","Erase","Flip"},*zh[]={"初始","清空","撤销","重做","擦除","翻转"};
        for(int k=0;k<6;k++)ui_button(g,(SDL_Rect){x+18+(k%3)*(w-30)/3,548+(k/3)*48,(w-44)/3,40},u->zh?zh[k]:en[k],-1,false);
        ui_button(g,(SDL_Rect){x+18,658,w-36,48},ui_tr(g,"Load into analysis","加载并分析"),2,true);
        ui_button(g,(SDL_Rect){x+18,718,w-36,40},ui_tr(g,"Cancel","取消"),-1,false);
        if(u->position.dragging!=-1){Piece pc=u->position.dragging==-2?u->position.brush:u->position.board.board[u->position.dragging];render_piece_box(g,g->ren,pc,g->mouse.x,g->mouse.y,g->sq,1);}
        ui_button(g,(SDL_Rect){x+18,810,w-36,30},ui_tr(g,"Game tags","棋谱资料"),4,false);
        if(u->position.error[0]){SDL_Rect clip={x+18,765,w-36,40};SDL_RenderSetClipRect(g->ren,&clip);ui_text(g,u->position.error,x+18,766,(SDL_Color){175,57,62,255});SDL_RenderSetClipRect(g->ren,NULL);}
    }
}
static void ui_puzzle_strip(Gui *g)
{
    Ui21 *u=g->ui;int y=g->board_y+8*g->sq+28,start=u->history_scroll;
    SDL_Rect clip={g->board_x,y,8*g->sq,42};SDL_RenderSetClipRect(g->ren,&clip);
    for(int k=start;k<u->entry_count;k++){
        PuzzleEntry *e=&u->entries[k];int x=g->board_x+(k-start)*68;if(x>=g->board_x+8*g->sq)break;
        SDL_Color col=!e->settled?(SDL_Color){57,132,208,255}:e->solved?(SDL_Color){74,145,82,255}:(SDL_Color){193,68,73,255};
        ui_round(g->ren,(SDL_Rect){x,y,60,34},col,16);char label[32];
        if(e->pending)snprintf(label,32,"...");else if(!e->settled)snprintf(label,32,"%s",k==u->current_entry?"•":"—");else if(e->rated)snprintf(label,32,"%+d",e->delta);else snprintf(label,32,"%s",e->solved?"✓":"×");
        ui_text(g,label,x+15,y+7,(SDL_Color){255,255,255,255});
        if(k==u->current_entry){SDL_SetRenderDrawColor(g->ren,40,80,124,255);SDL_RenderDrawLine(g->ren,x+10,y+39,x+50,y+39);}
    }
    SDL_RenderSetClipRect(g->ren,NULL);
    if(u->reviewing)ui_button(g,(SDL_Rect){g->board_x,y+44,8*g->sq,30},ui_tr(g,"Review · Return to current puzzle","不计分回看 · 返回当前题"),-1,true);
}
static void ui_puzzle_rating(Gui *g)
{
    Ui21 *u=g->ui;PuzzleEntry *entry=u->current_entry>=0?&u->entries[u->current_entry]:NULL;
    if(entry&&entry->day)return;
    int x=g->panel_x,y=g->board_y+62,w=g->win_w-x-24;
    ui_card(g,(SDL_Rect){x,y,w,142});
    ui_text(g,ui_tr(g,"Puzzle Rating","解题评分"),x+20,y+15,ui_muted);
    char rating[32];snprintf(rating,sizeof rating,"%d",g->puzzle_rating);
    SDL_Surface *surface=TTF_RenderUTF8_Blended(g->font_rating?g->font_rating:g->font_ui,rating,ui_ink);
    if(surface){SDL_Texture *texture=SDL_CreateTextureFromSurface(g->ren,surface);float scale=eff_scale(g);SDL_Rect dest={x+20,y+43,(int)(surface->w/scale),(int)(surface->h/scale)};SDL_RenderCopy(g->ren,texture,NULL,&dest);SDL_DestroyTexture(texture);SDL_FreeSurface(surface);}
    ui_button(g,(SDL_Rect){x+w-200,y+44,180,40},ui_tr(g,u->rated?"Rated":"Unrated",u->rated?"计分练习":"不计分练习"),-1,u->rated);
    ui_text(g,ui_tr(g,u->reviewing?"Review · No rating changes":entry&&entry->settled?"Scoring switch applies to next puzzle":"Switch scoring anytime",u->reviewing?"回看模式 · 不计分":entry&&entry->settled?"当前题已结算，切换应用于下一题":"可随时切换计分模式"),x+20,y+112,ui_muted);
}
static void ui_puzzle_toggle(Gui *g)
{
    Ui21 *u=g->ui;PuzzleEntry *e=u->current_entry>=0?&u->entries[u->current_entry]:NULL;
    if(e&&e->day)return;
    u->rated=!u->rated;
    if(!e||u->reviewing||e->settled||e->pending)return;
    if(ui_connected(g)){
        cJSON *args=cJSON_CreateObject();cJSON_AddStringToObject(args,"attempt",e->attempt);cJSON_AddBoolToObject(args,"rated",u->rated);online_request(g->online,"puzzle.mode",args);
    }else if(workspace_puzzle_mode(u->store,u->owner,e->attempt,u->rated))e->rated=u->rated;
}
static void ui_daily_panel(Gui *g)
{
    Ui21 *u=g->ui;if(u->current_entry<0)return;PuzzleEntry *entry=&u->entries[u->current_entry];if(!entry->day)return;
    int x=g->panel_x,w=g->win_w-x-24;ui_card(g,(SDL_Rect){x,88,w,650});
    render_text(g,g->font_ui,ui_tr(g,"Daily Puzzle · Unrated","每日题 · 不计分"),x+20,110,ui_ink);
    char date[11];workspace_date(entry->day,date);ui_button(g,(SDL_Rect){x+18,153,w-36,42},date,-1,false);
    if(u->calendar_open){char month[11];workspace_date(u->month_day,month);memcpy(month+8,"01",3);int first=workspace_day(month);month[7]=0;
        ui_button(g,(SDL_Rect){x+18,213,42,36},"<",-1,false);ui_text(g,month,x+88,223,ui_ink);ui_button(g,(SDL_Rect){x+w-60,213,42,36},">",-1,false);
        int cell=(w-40)/7;static const char *days[]={"M","T","W","T","F","S","S"},*days_zh[]={"一","二","三","四","五","六","日"};for(int k=0;k<7;k++)ui_text(g,u->zh?days_zh[k]:days[k],x+24+k*cell,262,ui_muted);
        int weekday=workspace_weekday(first);int today=workspace_today();
        for(int n=0;n<42;n++){int day=first+n-weekday;char d[11];workspace_date(day,d);if(memcmp(d,month,7))continue;
            bool exists=false,complete=false,practiced=false;const cJSON *row=NULL;cJSON_ArrayForEach(row,u->calendar){if(cJSON_GetObjectItem(row,"day")->valueint==day){exists=true;complete=cJSON_GetObjectItem(row,"completed")->valueint!=0;practiced=cJSON_GetObjectItem(row,"practiced")->valueint!=0;break;}}
            SDL_Rect r={x+18+(n%7)*cell,291+(n/7)*48,cell-5,42};SDL_Color col=complete?(SDL_Color){194,231,202,255}:practiced?(SDL_Color){247,228,180,255}:day==entry->day?(SDL_Color){180,212,246,255}:(SDL_Color){233,241,248,255};ui_round(g->ren,r,col,8);
            char label[10];snprintf(label,10,"%d",atoi(d+8));ui_text(g,label,r.x+12,r.y+11,exists&&day<=today?ui_ink:(SDL_Color){170,183,197,255});
            if(day==entry->day){SDL_SetRenderDrawColor(g->ren,41,115,204,255);ui_outline(g->ren,&r);}
            if(day==today){SDL_SetRenderDrawColor(g->ren,41,115,204,255);ui_line(g->ren,r.x+8,r.y+r.h-4,r.x+r.w-8,r.y+r.h-4);}
        }
    }
    char streak[160];snprintf(streak,sizeof streak,u->zh?"连续完成 %d 天 · 最长 %d 天":"Current streak: %d days · Best: %d",u->daily_current,u->daily_longest);ui_text(g,streak,x+20,605,ui_ink);
    ui_button(g,(SDL_Rect){x+18,650,w-36,42},ui_tr(g,"Back to today","返回今天"),-1,false);
    if(g->puzzle_done||g->puzzle_failed)ui_text(g,ui_tr(g,entry->reveal?"Practiced · Answer viewed":"Completed · No rating change",entry->reveal?"已练习 · 本次查看过答案":"已完成 · 不影响评分"),x+20,710,ui_muted);
}
static void ui_profile_render(Gui *g)
{
    Ui21 *u=g->ui;render_text(g,g->font_ui,ui_tr(g,"My Profile","个人主页"),240,40,ui_ink);
    ui_text(g,u->nickname,240,90,ui_muted);
    for(int k=0;k<5;k++){int rating=workspace_rating(u->store,u->owner,k);const cJSON *row=NULL;cJSON_ArrayForEach(row,cJSON_GetObjectItem(u->profile,"ratings"))if(cJSON_GetObjectItem(row,"category")->valueint==k)rating=cJSON_GetObjectItem(row,"value")->valueint;char label[60];snprintf(label,sizeof label,"%s · %d",workspace_class_name(k),rating);ui_button(g,(SDL_Rect){240+k*224,140,212,50},label,-1,k==u->rating_class);}
    const char *periods[]={"7d","30d","90d","365d","All"},*periods_zh[]={"7 天","30 天","90 天","365 天","全部"};for(int k=0;k<5;k++)ui_button(g,(SDL_Rect){240+k*112,210,102,38},u->zh?periods_zh[k]:periods[k],-1,k==u->rating_span);
    SDL_Rect graph={260,290,g->win_w-310,400};ui_card(g,(SDL_Rect){240,270,g->win_w-270,450});
    const cJSON *rows=u->rating_history?cJSON_GetObjectItem(u->rating_history,"rows"):NULL;int count=cJSON_GetArraySize(rows),lo=4000,hi=0;int64_t first=0,last=0;
    for(int k=0;k<count;k++){const cJSON *r=cJSON_GetArrayItem(rows,k);int v=cJSON_GetObjectItem(r,"value")->valueint;int64_t t=(int64_t)cJSON_GetObjectItem(r,"created")->valuedouble;if(v<lo)lo=v;if(v>hi)hi=v;if(!k)first=t;last=t;}
    if(count){lo-=50;hi+=50;if(last==first)last++;int px=0,py=0;SDL_SetRenderDrawColor(g->ren,48,122,202,255);
        for(int k=0;k<count;k++){const cJSON *r=cJSON_GetArrayItem(rows,k);int value=cJSON_GetObjectItem(r,"value")->valueint;int64_t t=(int64_t)cJSON_GetObjectItem(r,"created")->valuedouble;int x=graph.x+45+(int)((t-first)*(graph.w-65)/(last-first)),y=graph.y+graph.h-35-(value-lo)*(graph.h-55)/(hi-lo);if(k)SDL_RenderDrawLine(g->ren,px,py,x,y);fill_circle(g->ren,x,y,4);px=x;py=y;}
        char label[30];snprintf(label,30,"%d",hi);ui_text(g,label,graph.x,graph.y,ui_muted);snprintf(label,30,"%d",lo);ui_text(g,label,graph.x,graph.y+graph.h-40,ui_muted);
        char from[11],to[11];workspace_date((int)((first+28800)/86400),from);workspace_date((int)((last+28800)/86400),to);ui_text(g,from,graph.x+45,graph.y+graph.h-18,ui_muted);ui_text(g,to,graph.x+graph.w-100,graph.y+graph.h-18,ui_muted);
    }else ui_text(g,ui_tr(g,"Rating history starts with your first recorded result.","评分曲线从首次记录开始。"),graph.x+40,graph.y+130,ui_muted);
    ui_text(g,ui_tr(g,"Nickname","昵称"),240,760,ui_ink);ui_button(g,(SDL_Rect){355,744,350,46},u->input[0]?u->input:u->nickname,-1,false);ui_button(g,(SDL_Rect){720,744,200,46},ui_tr(g,"Save profile","保存资料"),3,true);ui_button(g,(SDL_Rect){935,744,220,46},ui_tr(g,"Change avatar","更换头像"),-1,false);
}
static void ui_social_render(Gui *g)
{
    Ui21 *u=g->ui;render_text(g,g->font_ui,ui_tr(g,"Friends & Community","好友与社区"),240,40,ui_ink);
    for(int k=0;k<3;k++){const char *en[]={"Friends","Players","Blocked"},*zh[]={"好友","玩家","已屏蔽"};ui_button(g,(SDL_Rect){240+k*148,96,138,44},u->zh?zh[k]:en[k],-1,u->social_tab==k);}
    ui_button(g,(SDL_Rect){240,154,338,40},u->search[0]?u->search:ui_tr(g,"Search nickname...","搜索昵称……"),2,false);ui_button(g,(SDL_Rect){590,154,84,40},ui_tr(g,"Find","搜索"),-1,false);
    const cJSON *rows=u->people?cJSON_GetObjectItem(u->people,"rows"):NULL;int count=cJSON_GetArraySize(rows);
    for(int k=0;k<count;k++){const cJSON *r=cJSON_GetArrayItem(rows,k),*name=cJSON_GetObjectItem(r,"nickname");if(!cJSON_IsString(name)||!name->valuestring[0])name=cJSON_GetObjectItem(r,"name");int peer=cJSON_GetObjectItem(r,"id")->valueint;char label[160];snprintf(label,sizeof label,"%s %s%s  %d",cJSON_IsTrue(cJSON_GetObjectItem(r,"online"))?"●":"○",cJSON_IsString(name)?name->valuestring:"?",cJSON_GetObjectItem(r,"state")->valueint==0?ui_tr(g," · Pending"," · 待处理"):"",cJSON_GetObjectItem(r,"unread")->valueint);ui_button(g,(SDL_Rect){240,210+k*47,434,40},label,-1,u->peer==peer);}
    ui_button(g,(SDL_Rect){240,704,210,38},ui_tr(g,"Previous","上一页"),-1,false);ui_button(g,(SDL_Rect){464,704,210,38},ui_tr(g,"Next","下一页"),-1,false);
    ui_card(g,(SDL_Rect){698,96,g->win_w-726,646});
    const char *en[]={"Add friend","Accept","Remove / Decline",u->social_tab==2?"Unblock":"Block","Challenge"},*zh[]={"加好友","接受申请","删除 / 拒绝",u->social_tab==2?"取消屏蔽":"屏蔽","约战"};
    for(int k=0;k<5;k++)ui_button(g,(SDL_Rect){716+(k%3)*218,116+(k/3)*48,202,40},u->zh?zh[k]:en[k],-1,k==4);
    const cJSON *score=cJSON_GetArrayItem(cJSON_GetObjectItem(u->headtohead,"rows"),0);
    if(score){int games=cJSON_GetObjectItem(score,"games")->valueint,wins=cJSON_GetObjectItem(score,"wins")->valueint,draws=cJSON_GetObjectItem(score,"draws")->valueint;char label[128];snprintf(label,sizeof label,u->zh?"交手比分：%d 胜 / %d 和 / %d 负":"Head to head: %d W / %d D / %d L",wins,draws,games-wins-draws);ui_text(g,label,716,214,ui_muted);}
    const cJSON *series=cJSON_GetArrayItem(cJSON_GetObjectItem(u->headtohead,"series"),0);if(series){int total=cJSON_GetObjectItem(series,"games")->valueint,wins=cJSON_GetObjectItem(series,"wins")->valueint,draws=cJSON_GetObjectItem(series,"draws")->valueint;char label[128];snprintf(label,sizeof label,u->zh?"本轮：%d 胜 / %d 和 / %d 负":"Current series: %d W / %d D / %d L",wins,draws,total-wins-draws);ui_text(g,label,716,238,ui_muted);}
    rows=u->messages?cJSON_GetObjectItem(u->messages,"rows"):NULL;count=cJSON_GetArraySize(rows);
    char transcript[8192]="";size_t used=0;
    for(int k=0;k<count&&k<10&&used<sizeof transcript-720;k++){const cJSON *row=cJSON_GetArrayItem(rows,count-1-k),*body=cJSON_GetObjectItem(row,"body");bool mine=cJSON_GetObjectItem(row,"sender")->valueint==u->self_id;
        used+=(size_t)snprintf(transcript+used,sizeof transcript-used,"%s%s: %s\n\n",mine&&cJSON_GetObjectItem(row,"read")->valueint?"✓ ":"",mine?ui_tr(g,"You","你"):ui_tr(g,"Friend","好友"),cJSON_IsString(body)?body->valuestring:"");}
    SDL_Rect conversation={716,270,g->win_w-760,343};SDL_RenderSetClipRect(g->ren,&conversation);
    int lines=render_wrapped_text(g,g->ren,transcript,conversation,u->message_scroll,ui_ink);SDL_RenderSetClipRect(g->ren,NULL);
    u->message_scroll_max=clampi(lines*(text_height(g,g->font_small)+3)+16-conversation.h,0,100000);u->message_scroll=clampi(u->message_scroll,0,u->message_scroll_max);
    ui_button(g,(SDL_Rect){716,638,g->win_w-870,44},u->input[0]?u->input:ui_tr(g,"Private message...","输入私信……"),-1,false);ui_button(g,(SDL_Rect){g->win_w-142,638,108,44},ui_tr(g,"Send","发送"),-1,true);
    ui_button(g,(SDL_Rect){716,694,212,34},ui_tr(g,"Older messages","更早的消息"),-1,false);ui_button(g,(SDL_Rect){944,694,212,34},ui_tr(g,"Latest messages","最新消息"),-1,false);
    rows=u->invitations?cJSON_GetObjectItem(u->invitations,"rows"):NULL;
    for(int k=0;k<cJSON_GetArraySize(rows)&&k<2;k++){const cJSON *row=cJSON_GetArrayItem(rows,k);char label[180];snprintf(label,sizeof label,ui_tr(g,"Invitation #%d · %d + %d","约战 #%d · %d + %d"),cJSON_GetObjectItem(row,"id")->valueint,cJSON_GetObjectItem(row,"time")->valueint/60000,cJSON_GetObjectItem(row,"inc")->valueint/1000);ui_text(g,label,240,770+k*42,ui_ink);ui_button(g,(SDL_Rect){700,756+k*42,150,34},ui_tr(g,"Accept","接受"),-1,true);ui_button(g,(SDL_Rect){864,756+k*42,150,34},ui_tr(g,"Decline","拒绝"),-1,false);}
}
static void ui_preferences_render(Gui *g)
{
    Ui21 *u=g->ui;render_text(g,g->font_ui,ui_tr(g,"Preferences","偏好设置"),240,40,ui_ink);ui_card(g,(SDL_Rect){240,110,g->win_w-280,700});
    const char *labels[]={ui_tr(g,"Language: English / 中文","界面语言：中文 / English"),ui_tr(g,"Choose background image","选择背景图片"),ui_tr(g,u->background_fit?"Image fit: Contain":"Image fit: Cover",u->background_fit?"背景缩放：适应":"背景缩放：铺满"),ui_tr(g,"Background overlay −","减弱背景遮罩"),ui_tr(g,"Background overlay +","增强背景遮罩"),ui_tr(g,"Reset background","恢复默认背景"),ui_tr(g,"Engine & game settings","引擎与对局设置"),ui_tr(g,"Board & pieces","棋盘与棋子外观"),ui_tr(g,"Account / Log in","账号与登录")};
    int width=(g->win_w-352)/2;
    for(int k=0;k<9;k++)ui_button(g,(SDL_Rect){264+(k%2)*(width+24),136+(k/2)*65,width,50},labels[k],k==1?4:-1,k==0);
    ui_button(g,(SDL_Rect){264+width+24,396,width,50},ui_tr(g,u->dark_mode?"Dark mode: On":"Dark mode: Off",u->dark_mode?"深色模式：开启":"深色模式：关闭"),-1,u->dark_mode);
    ui_text(g,ui_tr(g,"Application themes","应用主题"),264,486,ui_ink);int tile=(g->win_w-352)/5;
    for(int k=0;k<10;k++){SDL_Rect r={264+(k%5)*(tile+5),530+(k/5)*72,tile,54};ui_button(g,r,u->zh?ui_theme_zh[k]:ui_theme_names[k],-1,u->app_theme==k);SDL_Color swatch=ui_accents[k];SDL_SetRenderDrawColor(g->ren,swatch.r,swatch.g,swatch.b,255);SDL_RenderFillRect(g->ren,&(SDL_Rect){r.x+8,r.y+12,4,r.h-24});}
}
static void ui_library_render(Gui *g)
{
    Ui21 *u=g->ui;render_text(g,g->font_ui,ui_tr(g,u->page?"Saved Analysis":"Game History",u->page?"已保存分析":"历史对局"),240,40,ui_ink);
    if(u->page){
        ui_button(g,(SDL_Rect){240,100,520,42},ui_tr(g,"Restore previous analysis","恢复上次分析"),2,false);
        for(int k=0;k<cJSON_GetArraySize(u->analyses)&&k<10;k++){const cJSON *row=cJSON_GetArrayItem(u->analyses,k);const cJSON *title=cJSON_GetObjectItem(row,"title");ui_button(g,(SDL_Rect){240,166+k*57,g->win_w-280,48},cJSON_IsString(title)?title->valuestring:"Analysis",3,false);}return;
    }
    const cJSON *rows=u->games?cJSON_GetObjectItem(u->games,"rows"):NULL;for(int k=0;k<cJSON_GetArraySize(rows);k++){
        const cJSON *row=cJSON_GetArrayItem(rows,k),*a=cJSON_GetObjectItem(row,"white_name"),*b=cJSON_GetObjectItem(row,"black_name"),*res=cJSON_GetObjectItem(row,"result");char label[256];snprintf(label,sizeof label,"%s / %s   %s",cJSON_IsString(a)?a->valuestring:"?",cJSON_IsString(b)?b->valuestring:"?",cJSON_IsString(res)?res->valuestring:"*");ui_button(g,(SDL_Rect){240,120+k*58,g->win_w-280,48},label,-1,false);}
    ui_button(g,(SDL_Rect){240,740,190,44},ui_tr(g,"Previous","上一页"),-1,false);ui_button(g,(SDL_Rect){450,740,190,44},ui_tr(g,"Next","下一页"),-1,false);
}
static bool ui_render_page(Gui *g)
{
    if(!g->ui)return false;g->ui->legacy_paint=false;
    if(g->scene!=SCENE_MENU&&g->scene!=SCENE_ANALYSIS_HOME&&g->scene!=SCENE_POSITION_SETUP&&g->scene!=SCENE_PROFILE&&g->scene!=SCENE_SOCIAL&&g->scene!=SCENE_PREFERENCES&&g->scene!=SCENE_LIBRARY)return false;
    ui_background(g,g->ren);
    switch(g->scene){case SCENE_MENU:ui_home_render(g);break;case SCENE_ANALYSIS_HOME:ui_analysis_render(g,false);break;case SCENE_POSITION_SETUP:ui_analysis_render(g,true);break;case SCENE_PROFILE:ui_profile_render(g);break;case SCENE_SOCIAL:ui_social_render(g);break;case SCENE_PREFERENCES:ui_preferences_render(g);break;case SCENE_LIBRARY:ui_library_render(g);break;default:break;}return true;
}
static void ui_profile_request(Gui *g)
{
    Ui21 *u=g->ui;static const int days[]={7,30,90,365,0};int since=days[u->rating_span]?(int)time(NULL)-days[u->rating_span]*86400:0;
    if(g->online&&online_logged_in(g->online)){online_request(g->online,"profile.get",NULL);cJSON *q=ui_args_int("category",u->rating_class);cJSON_AddNumberToObject(q,"since",since);online_request(g->online,"ratings.history",q);}
    else {sqlite3_stmt *s=workspace_stmt(u->store,"SELECT category,value,created FROM oc_rating_events WHERE id IN(SELECT MAX(id) FROM oc_rating_events WHERE owner=? AND category=? AND created>=? GROUP BY CASE WHEN ?=0 THEN strftime('%Y-%m',created,'unixepoch','+8 hours') ELSE CAST((created+28800)/86400 AS TEXT) END) ORDER BY created");sqlite3_bind_text(s,1,u->owner,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,u->rating_class);sqlite3_bind_int(s,3,since);sqlite3_bind_int(s,4,since);cJSON_Delete(u->rating_history);u->rating_history=cJSON_CreateObject();cJSON_AddItemToObject(u->rating_history,"rows",workspace_rows(s));}
}
static void ui_import_text(Gui *g)
{
    Board b;Ui21 *u=g->ui;
    if(fen_parse(u->input,&b)){char err[160];if(!position_validate(&b,err)){snprintf(u->status,256,"%s",err);return;}start_analysis(g);load_fen(g,u->input);return;}
    if(strchr(u->input,'[')||strstr(u->input,"1.")){snprintf(g->pgn_text,sizeof g->pgn_text,"%s",u->input);pgn_import_load(g);return;}
    snprintf(u->status,256,"%s",ui_tr(g,"Enter a valid PGN or FEN.","请输入有效的 PGN 或 FEN。"));
}
static bool ui_pick_background(char path[1200])
{
#if defined(__APPLE__)
    FILE *f=popen("osascript -e 'POSIX path of (choose file of type {\"public.png\", \"public.jpeg\", \"org.webmproject.webp\"} with prompt \"Choose background image\")' 2>/dev/null","r");
#elif defined(_WIN32)
    FILE *f=popen("powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.OpenFileDialog; $d.Filter='Images|*.png;*.jpg;*.jpeg;*.webp'; if($d.ShowDialog() -eq 'OK'){$d.FileName}\" 2>NUL","r");
#else
    FILE *f=popen("zenity --file-selection --title='Choose background' --file-filter='Images | *.png *.jpg *.jpeg *.webp' 2>/dev/null","r");
#endif
    if(!f)return false;bool ok=fgets(path,1200,f)!=NULL;pclose(f);if(ok)path[strcspn(path,"\r\n")]=0;return ok&&path[0];
}
static void ui_background_choose(Gui *g)
{
    char source[1200];if(!ui_pick_background(source))return;SDL_Texture *texture=IMG_LoadTexture(g->ren,source);if(!texture){snprintf(g->ui->status,256,"%s",ui_tr(g,"Image could not be loaded.","图片无法加载。"));return;}
    char dest[1200];snprintf(dest,sizeof dest,"%s/background.image",path_data_dir());FILE *in=fopen(source,"rb"),*out=in?fopen(dest,"wb"):NULL;bool ok=in&&out;char bytes[16384];size_t n;
    if(ok)while((n=fread(bytes,1,sizeof bytes,in)))if(fwrite(bytes,1,n,out)!=n){ok=false;break;}if(in)fclose(in);if(out)fclose(out);
    if(ok){SDL_DestroyTexture(g->ui->background);g->ui->background=texture;g->ui->background_mask=80;snprintf(g->ui->background_file,sizeof g->ui->background_file,"%s",dest);g->config_dirty=true;}else SDL_DestroyTexture(texture);
}
static bool ui_new_scene(Scene scene){return scene==SCENE_MENU||scene>=SCENE_ANALYSIS_HOME;}
static void ui_social_action(Gui *g,const char *action)
{
    if(!g->ui->peer)return;cJSON *q=ui_args_int("peer",g->ui->peer);cJSON_AddStringToObject(q,"action",action);ui_request(g,"friend.action",q);
}
static void ui_backspace(char *text);
#include "ui21_dialogs.inc"

static bool ui_click(Gui *g)
{
    Ui21 *u=g->ui;int mx=g->mouse.x,my=g->mouse.y;
    if(mx<208){
        if(my<90){ui_home(g);return true;}
        if(my>=112&&my<460){u->nav_hover=(my-112)/58;return true;}
        if(my>g->win_h-58){if(mx<155){g->scene=SCENE_SOCIAL;u->social_tab=0;u->page=0;ui_request(g,"friends",NULL);if(mx>100)online_request(g->online,"challenge.list",NULL);}else g->scene=SCENE_PREFERENCES;return true;}
        if(my>g->win_h-150&&my<g->win_h-90){g->scene=SCENE_PROFILE;ui_profile_request(g);return true;}return true;
    }
    if(u->nav_hover>=0){int group=u->nav_hover,top=112+group*58;if(mx>=204&&mx<484&&my>=top){int item=(my-top-6)/50;if(item>=0&&item<4&&ui_sub_name(g,group,item)){ui_nav_action(g,group,item);return true;}}u->nav_hover=-1;}
    if(g->scene==SCENE_MENU&&g->saved.valid&&my<70&&mx>g->win_w-260){resume_game(g);return true;}
    if(ui_active_game(g)&&my<70&&mx>g->win_w-260){g->scene=SCENE_GAME;return true;}
    if(g->scene==SCENE_SETTINGS&&my>=22&&my<58){
        if(mx>=g->win_w-218&&mx<g->win_w-24){u->dark_mode=!u->dark_mode;g->config_dirty=true;return true;}
        if(mx>=g->win_w-360&&mx<g->win_w-230){g->scene=SCENE_PREFERENCES;return true;}
    }
    if(g->scene==SCENE_MENU){
        my+=u->home_scroll;
        if(mx>=240&&mx<522&&my>=112&&my<536){if(my>=490){if(ui_active_game(g)){g->scene=SCENE_GAME;return true;}toggle_offline_mode(g);g->config_dirty=true;return true;}
            if(g->offline_mode){g->scene=my<245?SCENE_SINGLE_SETUP:SCENE_HOSTJOIN;return true;}
            if(my>=438){g->scene=SCENE_SOCIAL;u->social_tab=1;ui_request(g,"people",NULL);return true;}
            if(my>=170&&my<425){int row=(my-170)/51,col=mx>=382?1:0,k=row*2+col;static const int times[][2]={{60000,0},{120000,1000},{180000,0},{180000,2000},{300000,0},{300000,3000},{600000,0},{600000,5000},{900000,10000},{1800000,0}};if(k<10&&!ui_active_game(g)&&online_ensure(g)){online_set_rated(g->online,online_logged_in(g->online));online_queue(g->online,times[k][0],times[k][1]);g->scene=SCENE_ONLINE;}return true;}
        }
        HomeLayout layout=ui_home_layout(g);
        for(int k=0;k<3;k++)if(pt_in(&layout.cards[k],mx,my)){if(k==0){u->page=0;if(u->preview_valid){int index=ui_entry_add(g,&u->preview,"preview-review",0,false);if(index>=0){u->entries[index].settled=true;ui_entry_load(g,index,true);}}else ui_next_puzzle(g,0);}else if(k==1)ui_open_analysis(g);else {open_openings(g);for(int index=0;index<g->opening_match_count;index++)if(g->opening_match[index]==u->opening_preview){g->opening_sel=index;break;}openings_load_selected(g);}return true;}
        if(pt_in(&layout.daily,mx,my)){ui_open_daily(g,workspace_today());return true;}
        if(pt_in(&layout.history,mx,my)&&my>=layout.history.y+59){int idx=(my-layout.history.y-59)/48;const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(u->games,"rows"),idx),*id=cJSON_GetObjectItem(row,"id");if(cJSON_IsString(id))ui_game_history_open(g,id->valuestring);return true;}
        return true;
    }
    if(g->scene==SCENE_ANALYSIS_HOME){int x=g->panel_x,w=g->win_w-x-24;if(mx<x)return true;
        if(my>=154&&my<230){u->input_focus=true;return true;}
        if(my>=242&&my<286){if(mx<x+w/2)ui_import_text(g);else{char path[1200];if(pick_pgn_file(path,sizeof path)&&pgn_import_read_file(g,path)){snprintf(u->input,sizeof u->input,"%s",g->pgn_text);ui_import_text(g);}}return true;}
        if(my>=310&&my<748){int k=(my-310)/73;if(k==0)ui_position_open(g);else if(k==1)start_analysis(g);else if(k==2){u->page=1;g->scene=SCENE_LIBRARY;ui_analysis_list(g);}else if(k==3)ui_load_analysis(g,"last");else if(k==4){u->page=0;g->scene=SCENE_LIBRARY;ui_request(g,"games",NULL);}else open_openings(g);return true;}return true;
    }
    if(g->scene==SCENE_POSITION_SETUP){int x=g->panel_x,w=g->win_w-x-24;PositionEditor *p=&u->position;
        int sq=sq_from_pos(g,mx,my);if(sq>=0){if(u->erase_tool){position_place(p,sq,EMPTY);ui_position_sync(g);}else if(p->brush!=EMPTY){position_place(p,sq,p->brush);ui_position_sync(g);}else if(p->board.board[sq]!=EMPTY)p->dragging=sq;else{position_place(p,sq,EMPTY);ui_position_sync(g);}return true;}
        if(mx<x)return true;
        u->setup_fen_focus=false;
        if(my>=156&&my<284){int col=(mx-x-18)/((w-36)/6),row=(my-156)/64;if(col>=0&&col<6&&row<2){Piece pc=(Piece)(row==0?BP+col:WP+col);p->brush=p->brush==pc?EMPTY:pc;u->erase_tool=false;if(p->brush!=EMPTY)p->dragging=-2;}return true;}
        if(my>=296&&my<336){Board b=p->board;b.side=b.side==WHITE?BLACK:WHITE;position_commit(p,&b);ui_position_sync(g);return true;}
        if(my>=350&&my<438){int k=((my-350)/44)*2+(mx>=x+w/2);Board b=p->board;b.castling^=1<<k;position_commit(p,&b);ui_position_sync(g);return true;}
        if(my>=448&&my<506){u->setup_fen_focus=true;return true;}
        if(my>=548&&my<644){int k=((my-548)/48)*3+(mx-x-18)/((w-30)/3);Board b=p->board;
            if(k==0){board_reset(&b);position_commit(p,&b);}else if(k==1){memset(&b,0,sizeof b);b.ep_square=-1;b.fullmove_number=1;position_commit(p,&b);}else if(k==2)position_undo(p);else if(k==3)position_redo(p);else if(k==4){p->brush=EMPTY;u->erase_tool=!u->erase_tool;}else if(k==5)g->flipped=!g->flipped;ui_position_sync(g);return true;}
        if(my>=658&&my<706){Board b;if(!fen_parse(u->setup_fen,&b)){snprintf(p->error,sizeof p->error,"%s",ui_tr(g,"Invalid FEN or missing kings.","FEN 无效或缺少双方的王。"));return true;}if(!position_validate(&b,p->error))return true;u->analysis_preview=b;start_analysis(g);ui_metadata_apply(g);load_fen(g,u->setup_fen);return true;}
        if(my>=718&&my<758){g->scene=SCENE_ANALYSIS_HOME;return true;}if(my>=810&&my<840){ui_metadata_begin(g);return true;}return true;
    }
    if(g->scene==SCENE_PREFERENCES){
        if(my>=530&&my<656){int tile=(g->win_w-352)/5,k=((my-530)/72)*5+(mx-264)/(tile+5);if(mx>=264&&k>=0&&k<10){u->app_theme=k;g->config_dirty=true;}return true;}
        if(my>=136&&my<446){int width=(g->win_w-352)/2,k=((my-136)/65)*2+(mx>=264+width+24);if(k==9){u->dark_mode=!u->dark_mode;g->config_dirty=true;return true;}
        if(k==0)u->zh=!u->zh;else if(k==1)ui_background_choose(g);else if(k==2)u->background_fit=!u->background_fit;else if(k==3)u->background_mask=clampi(u->background_mask-25,0,255);else if(k==4)u->background_mask=clampi(u->background_mask+25,0,255);else if(k==5){SDL_DestroyTexture(u->background);u->background=NULL;u->background_file[0]=0;}else if(k==6)open_settings_scene(g,SCENE_PREFERENCES);else if(k==7)open_appearance(g,SCENE_PREFERENCES);else open_account(g);g->config_dirty=true;}return true;
    }
    if(g->scene==SCENE_PROFILE){
        if(my>=140&&my<190){u->rating_class=clampi((mx-240)/224,0,4);ui_profile_request(g);}else if(my>=210&&my<248){u->rating_span=clampi((mx-240)/112,0,4);ui_profile_request(g);}else if(my>=744&&my<790){if(mx<710){u->input_focus=true;u->input[0]=0;}else if(mx<930){cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"nickname",u->input[0]?u->input:u->nickname);cJSON_AddNumberToObject(q,"avatar",u->avatar);if(g->online&&online_logged_in(g->online))ui_request(g,"profile.set",q);else{sqlite3_stmt *s=workspace_stmt(u->store,"UPDATE oc_profiles SET nickname=?,avatar=? WHERE owner=?");sqlite3_bind_text(s,1,u->input,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,u->avatar);sqlite3_bind_text(s,3,u->owner,-1,SQLITE_TRANSIENT);sqlite3_step(s);sqlite3_finalize(s);snprintf(u->nickname,sizeof u->nickname,"%s",u->input);cJSON_Delete(q);}u->input_focus=false;}else {u->avatar=(u->avatar+1)%12;g->config_dirty=true;}}return true;
    }
    if(g->scene==SCENE_SOCIAL){
        if(my>=96&&my<140&&mx<680){u->social_tab=clampi((mx-240)/148,0,2);u->offset=0;ui_request(g,ui_social_op(u),NULL);return true;}
        if(my>=154&&my<194&&mx<680){if(mx<580){u->input_focus=true;u->page=2;snprintf(u->input,sizeof u->input,"%s",u->search);}else{cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"search",u->search);ui_request(g,ui_social_op(u),q);}return true;}
        if(my>=210&&my<680&&mx<680){const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(u->people,"rows"),(my-210)/47);if(row){u->peer=cJSON_GetObjectItem(row,"id")->valueint;u->message_cursor=u->message_scroll=0;cJSON_Delete(u->messages);u->messages=NULL;ui_request(g,"messages",ui_args_int("peer",u->peer));ui_request(g,"headtohead",ui_args_int("peer",u->peer));u->input[0]=0;u->page=0;}return true;}
        if(mx>=716&&my>=116&&my<204){int k=((my-116)/48)*3+(mx-716)/218;if(k<4){const char *actions[]={"request","accept","remove",u->social_tab==2?"unblock":"block"};ui_social_action(g,actions[k]);}else if(k==4&&u->peer){u->challenge_open=true;u->dialog_field=-1;u->challenge_color=-1;u->challenge_rated=true;snprintf(u->challenge_minutes,16,"5");snprintf(u->challenge_increment,16,"0");}return true;}
        if(my>=638&&my<682&&mx>=716){if(mx<g->win_w-150){u->input_focus=true;u->page=0;}else{cJSON *q=ui_args_int("peer",u->peer);cJSON_AddStringToObject(q,"text",u->input);u->message_cursor=u->message_scroll=0;ui_request(g,"message.send",q);u->input[0]=0;}return true;}
        if(my>=694&&my<742){if(mx<680){u->offset=mx<450?clampi(u->offset-10,0,100000):u->offset+10;cJSON *q=ui_args_int("offset",u->offset);ui_request(g,ui_social_op(u),q);}else{const cJSON *rows=cJSON_GetObjectItem(u->messages,"rows"),*last=cJSON_GetArrayItem(rows,cJSON_GetArraySize(rows)-1);cJSON *q=ui_args_int("peer",u->peer);if(mx>=944)u->message_cursor=0;else if(last)u->message_cursor=cJSON_GetObjectItem(last,"id")->valueint;cJSON_AddNumberToObject(q,"offset",u->message_cursor);u->message_scroll=0;ui_request(g,"messages",q);}return true;}
        if(my>=756&&my<840&&mx>=700){int k=(my-756)/42;const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(u->invitations,"rows"),k);if(row){int from=cJSON_GetObjectItem(row,"from")->valueint;cJSON *q=ui_args_int("id",cJSON_GetObjectItem(row,"id")->valueint);ui_request(g,mx<855?"challenge.accept":from==u->self_id?"challenge.cancel":"challenge.decline",q);}return true;}return true;
    }
    if(g->scene==SCENE_LIBRARY){if(u->page){if(my>=100&&my<142)ui_load_analysis(g,"last");else if(my>=166&&my<736){const cJSON *row=cJSON_GetArrayItem(u->analyses,(my-166)/57),*id=cJSON_GetObjectItem(row,"id");if(cJSON_IsString(id))ui_load_analysis(g,id->valuestring);}}
        else if(my>=120&&my<700){const cJSON *row=cJSON_GetArrayItem(cJSON_GetObjectItem(u->games,"rows"),(my-120)/58),*id=cJSON_GetObjectItem(row,"id");if(cJSON_IsString(id))ui_game_history_open(g,id->valuestring);}
        else if(my>=740&&my<784){u->offset=mx<440?clampi(u->offset-10,0,100000):u->offset+10;ui_request(g,"games",ui_args_int("offset",u->offset));}return true;
    }
    if(g->scene==SCENE_PUZZLE_SETUP&&u->theme_open)return false;
    if(g->scene==SCENE_PUZZLE_SETUP&&my>=g->win_h-210&&my<g->win_h-165){u->rated=!u->rated;return true;}
    if(g->scene==SCENE_GAME&&g->mode==MODE_ANALYSIS&&my>=g->board_y+8*g->sq+28&&my<g->board_y+8*g->sq+70&&mx>=g->board_x&&mx<g->board_x+200){ui_save_analysis(g,"saved");return true;}
    if(g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE){int y=g->board_y+8*g->sq+28;
        PuzzleEntry *current=u->current_entry>=0?&u->entries[u->current_entry]:NULL;
        int card_w=g->win_w-g->panel_x-24,card_y=g->board_y+62;
        if((!current||!current->day)&&mx>=g->panel_x+card_w-200&&mx<g->panel_x+card_w-20&&my>=card_y+44&&my<card_y+84){ui_puzzle_toggle(g);return true;}
        if(mx>=g->board_x&&mx<g->board_x+8*g->sq&&my>=y&&my<y+42){int k=(mx-g->board_x)/68+u->history_scroll;if(k<u->entry_count)ui_entry_load(g,k,true);return true;}
        if(u->reviewing&&my>=y+44&&my<y+74){int idx=u->suspended_entry;if(idx>=0)ui_entry_load(g,idx,false);return true;}
        PuzzleEntry *e=u->current_entry>=0?&u->entries[u->current_entry]:NULL;
        if(e&&e->day&&mx>=g->panel_x){int x=g->panel_x,w=g->win_w-x-24;if(my>=153&&my<195){u->calendar_open=!u->calendar_open;ui_calendar_refresh(g);return true;}
            if(my>=213&&my<249){char d[11];workspace_date(u->month_day,d);memcpy(d+8,"01",3);int first=workspace_day(d);u->month_day=mx<x+w/2?first-1:first+32;ui_calendar_refresh(g);return true;}
            if(my>=291&&my<579&&u->calendar_open){char date[11];workspace_date(u->month_day,date);memcpy(date+8,"01",3);int first=workspace_day(date),cell=(w-40)/7,k=((my-291)/48)*7+(mx-x-18)/cell,day=first-workspace_weekday(first)+k;const cJSON *row=NULL;cJSON_ArrayForEach(row,u->calendar)if(cJSON_GetObjectItem(row,"day")->valueint==day){ui_open_daily(g,day);break;}return true;}
            if(my>=650&&my<692){ui_open_daily(g,workspace_today());return true;}
            if(my<738)return true;
        }
    }
    return ui_new_scene(g->scene);
}
static void ui_backspace(char *text){size_t n=strlen(text);if(!n)return;n--;while(n&&((unsigned char)text[n]&0xc0)==0x80)n--;text[n]=0;}
static bool ui_event(Gui *g,const SDL_Event *e)
{
    if(!g->ui||g->account_open)return false;Ui21 *u=g->ui;
    if(e->type==SDL_MOUSEMOTION||e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP){int x=e->type==SDL_MOUSEMOTION?e->motion.x:e->button.x,y=e->type==SDL_MOUSEMOTION?e->motion.y:e->button.y;set_mouse(g,x,y);}
    if(e->type==SDL_TEXTEDITING){snprintf(u->composition,sizeof u->composition,"%s",e->edit.text);return u->input_focus||u->setup_fen_focus||u->metadata_open;}
    if(e->type==SDL_TEXTINPUT)u->composition[0]=0;
    if(ui_modal_event(g,e))return true;
    if(e->type==SDL_MOUSEMOTION){int mx=g->mouse.x,my=g->mouse.y;if(mx<208&&my>=112&&my<460)u->nav_hover=(my-112)/58;else if(u->nav_hover>=0){int top=112+u->nav_hover*58;if(mx<204||mx>484||my<top||my>top+212)u->nav_hover=-1;}
        if(g->scene==SCENE_POSITION_SETUP)return true;return false;}
    if(e->type==SDL_MOUSEBUTTONDOWN){if(e->button.button==SDL_BUTTON_RIGHT&&g->scene==SCENE_POSITION_SETUP){int sq=sq_from_pos(g,g->mouse.x,g->mouse.y);if(sq>=0){position_place(&u->position,sq,EMPTY);ui_position_sync(g);}return true;}if(e->button.button==SDL_BUTTON_LEFT)return ui_click(g);}
    if(e->type==SDL_MOUSEBUTTONUP&&g->scene==SCENE_POSITION_SETUP){PositionEditor *p=&u->position;int sq=sq_from_pos(g,g->mouse.x,g->mouse.y);if(p->dragging==-2&&sq>=0){position_place(p,sq,p->brush);ui_position_sync(g);}else if(p->dragging>=0&&sq>=0&&sq!=p->dragging){Board b=p->board;b.board[sq]=b.board[p->dragging];b.board[p->dragging]=EMPTY;position_commit(p,&b);ui_position_sync(g);}p->dragging=-1;return true;}
    if(e->type==SDL_MOUSEWHEEL&&g->scene==SCENE_MENU&&g->mouse.x>=208){HomeLayout l=ui_home_layout(g);u->home_scroll=clampi(u->home_scroll-e->wheel.y*64,0,l.height-g->win_h);return true;}
    if(e->type==SDL_MOUSEWHEEL&&g->scene==SCENE_SOCIAL&&g->mouse.x>698){u->message_scroll=clampi(u->message_scroll-e->wheel.y*36,0,u->message_scroll_max);return true;}
    if(e->type==SDL_MOUSEWHEEL&&g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE&&g->mouse.y>g->board_y+8*g->sq){u->history_scroll=clampi(u->history_scroll-e->wheel.y,0,u->entry_count>8?u->entry_count-8:0);return true;}
    if(e->type==SDL_TEXTINPUT&&(u->input_focus||u->setup_fen_focus)){
        char *dst=u->setup_fen_focus?u->setup_fen:u->input;size_t cap=u->setup_fen_focus?sizeof u->setup_fen:sizeof u->input,n=strlen(dst),add=strlen(e->text.text);if(n+add<cap)memcpy(dst+n,e->text.text,add+1);if(u->page==2)snprintf(u->search,sizeof u->search,"%s",u->input);return true;
    }
    if(e->type==SDL_KEYDOWN){SDL_Keycode k=e->key.keysym.sym;bool ctrl=e->key.keysym.mod&KMOD_CTRL;
        if((e->key.keysym.mod&KMOD_ALT)&&k>=SDLK_1&&k<=SDLK_6){u->nav_hover=(int)(k-SDLK_1);u->nav_focus=0;return true;}
        if(u->nav_hover>=0&&!u->input_focus&&!u->setup_fen_focus){if(k==SDLK_DOWN){if(u->nav_focus<3&&ui_sub_name(g,u->nav_hover,u->nav_focus+1))u->nav_focus++;return true;}if(k==SDLK_UP){if(u->nav_focus>0)u->nav_focus--;return true;}if(k==SDLK_RETURN){ui_nav_action(g,u->nav_hover,u->nav_focus);return true;}}
        if(k==SDLK_ESCAPE&&(ui_new_scene(g->scene)||u->input_focus||u->setup_fen_focus)){u->input_focus=u->setup_fen_focus=false;u->nav_hover=-1;ui_home(g);return true;}
        if(u->input_focus||u->setup_fen_focus){char *dst=u->setup_fen_focus?u->setup_fen:u->input;size_t cap=u->setup_fen_focus?sizeof u->setup_fen:sizeof u->input;
            if(k==SDLK_BACKSPACE)ui_backspace(dst);else if(ctrl&&k==SDLK_v){char *clip=SDL_GetClipboardText();if(clip){size_t n=strlen(dst),add=strlen(clip);if(n+add<cap)memcpy(dst+n,clip,add+1);SDL_free(clip);}}
            else if(ctrl&&k==SDLK_a)dst[0]=0;else if(k==SDLK_RETURN){if(u->setup_fen_focus){Board b;if(fen_parse(dst,&b)){position_commit(&u->position,&b);ui_position_sync(g);u->setup_fen_focus=false;}else snprintf(u->position.error,160,"Invalid FEN");}else if(g->scene==SCENE_ANALYSIS_HOME)ui_import_text(g);}
            if(u->page==2)snprintf(u->search,sizeof u->search,"%s",u->input);return true;
        }
        if(g->scene==SCENE_POSITION_SETUP&&ctrl&&(k==SDLK_z||k==SDLK_y)){if(k==SDLK_z)position_undo(&u->position);else position_redo(&u->position);ui_position_sync(g);return true;}
        if(g->scene==SCENE_MENU&&(k==SDLK_RETURN||k==SDLK_UP||k==SDLK_DOWN||k==SDLK_SPACE))return false;
        if(ui_new_scene(g->scene))return k!=SDLK_q||!ctrl;
    }
    return false;
}
