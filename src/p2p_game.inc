/* P2P session protocol. The creator referees clocks; both peers validate moves.
 * Generations + ply indexes make duplicate/stale messages harmless. */
static const int P2P_TIMES[][2]={{0,0},{60,0},{120,1},{180,0},{180,2},{300,0},{300,3},{600,0},{600,5},{900,10},{1800,0}};
static void p2p_status(Gui *g,const char *en,const char *zh){snprintf(g->p2p_status,sizeof g->p2p_status,"%s",ui_tr(g,en,zh));}
static bool p2p_finished(const Gui *g){return g->state!=NO_GAME_OVER||g->override_result[0];}
static bool p2p_active(const Gui *g){return net_is_p2p(g->net)&&g->p2p_started&&g->mode==MODE_LOCAL&&!p2p_finished(g);}
static void p2p_disconnect(Gui *g)
{
    if(g->net){transport_close(g->net);g->net=NULL;}
    if(g->p2p_started&&g->mode==MODE_LOCAL&&!p2p_finished(g))snprintf(g->override_result,sizeof g->override_result,"*");
    g->p2p_ready=false;clear_selection(g);
    p2p_status(g,"Connection closed. Check the network and exchange a new invitation.","连接已断开，请检查网络并重新交换邀请。");
    set_msg(g,ui_tr(g,"Peer disconnected · game stopped","对方已断开连接 · 对局已停止"),NULL);
}
static bool p2p_send(Gui *g,const char *message)
{
    if(transport_send(g->net,message))return true;
    p2p_disconnect(g);return false;
}
static void p2p_open(Gui *g)
{
    if(p2p_active(g)){g->scene=SCENE_GAME;return;}
    if(net_is_p2p(g->net)&&!g->p2p_started){g->scene=SCENE_P2P;return;}
    if(g->net){transport_close(g->net);g->net=NULL;}
    g->p2p_started=g->p2p_ready=false;g->p2p_generation=0;
    g->p2p_vote=g->p2p_peer_vote=false;g->p2p_export[0]=0;g->p2p_status[0]=0;
    g->p2p_score[0]=g->p2p_score[1]=g->p2p_score[2]=0;g->p2p_peer_name[0]=0;g->p2p_control=2;
    if(!g->p2p_ice[0])snprintf(g->p2p_ice,sizeof g->p2p_ice,"stun:stun.cloudflare.com:3478");
    g->scene=SCENE_P2P;g->p2p_focus=false;
}
static void p2p_create(Gui *g,bool host)
{
    if(g->net){transport_close(g->net);g->net=NULL;}
    g->p2p_started=g->p2p_ready=false;g->p2p_generation=0;
    g->p2p_vote=g->p2p_peer_vote=false;g->p2p_export[0]=0;
    g->net=net_p2p_create(host,g->p2p_ice);g->p2p_connected_at=SDL_GetTicks();
    if(!g->net)p2p_status(g,"P2P is unavailable in this build.","当前构建未包含 P2P 支持。");
    else p2p_status(g,host?"Preparing invitation…":"Paste the invitation from your friend.",host?"正在生成邀请……":"请粘贴好友发来的邀请。");
}
static void p2p_begin(Gui *g,int generation,int seconds,int inc,int host_white,Uint32 now)
{
    bool host=transport_role(g->net)==NET_ROLE_HOST;
    ui_local_history(g);
    g->p2p_generation=generation;g->p2p_base=seconds*1000;g->p2p_increment=inc*1000;
    g->p2p_host_white=host_white;g->p2p_remaining[0]=g->p2p_remaining[1]=seconds*1000;
    enter_local_game(g,(host==!!host_white)?WHITE:BLACK);
    snprintf(g->white_name,sizeof g->white_name,"%s",ui_tr(g,host_white?"Host":"Guest",host_white?"房主":"访客"));
    snprintf(g->black_name,sizeof g->black_name,"%s",ui_tr(g,host_white?"Guest":"Host",host_white?"访客":"房主"));
    g->p2p_started=true;g->p2p_ready=false;g->p2p_vote=g->p2p_peer_vote=false;g->p2p_scored=false;
    g->p2p_last_tick=g->p2p_last_rx=g->p2p_last_ping=g->p2p_clock_sent=now;
    g->draw_offered=false;g->p2p_draw_sent=false;
    const char *name=g->ui&&g->ui->nickname[0]?g->ui->nickname:"Player";
    char hello[128];snprintf(hello,sizeof hello,"NAME %s",name);
    for(char *s=hello;*s;s++)if((unsigned char)*s<32)*s=' ';
    transport_send(g->net,hello);
    snprintf(g->local_color==WHITE?g->white_name:g->black_name,sizeof g->white_name,"%.63s",name);
    if(g->p2p_peer_name[0])snprintf(g->local_color==WHITE?g->black_name:g->white_name,sizeof g->white_name,"%.63s",g->p2p_peer_name);
}
static void p2p_host_start(Gui *g,Uint32 now)
{
    int index=clampi(g->p2p_time,0,10),generation=g->p2p_generation+1;
    int white=g->p2p_started?!g->p2p_host_white:(g->p2p_side==2?rand()%2:g->p2p_side==0);
    char message[128];snprintf(message,sizeof message,"START %d %d %d %d",generation,P2P_TIMES[index][0],P2P_TIMES[index][1],white);
    if(p2p_send(g,message))p2p_begin(g,generation,P2P_TIMES[index][0],P2P_TIMES[index][1],white,now);
}
static void p2p_result(Gui *g,const char *result)
{
    snprintf(g->override_result,sizeof g->override_result,"%s",result);g->p2p_ready=false;clear_selection(g);
}
static void p2p_action(Gui *g)
{
    if(!net_is_p2p(g->net)||!g->p2p_started)return;
    char message[64];
    if(p2p_finished(g)){
        g->p2p_vote=true;snprintf(message,sizeof message,"REMATCH %d",g->p2p_generation);
        if(p2p_send(g,message))set_msg(g,ui_tr(g,"Rematch requested","已请求再来一局"),NULL);
    }else{
        snprintf(message,sizeof message,"RESIGN %d",g->p2p_generation);
        if(p2p_send(g,message))p2p_result(g,g->local_color==WHITE?"0-1":"1-0");
    }
}
static void p2p_draw(Gui *g,bool accept)
{
    if(!p2p_active(g))return;char message[64];
    if(accept&&!g->draw_offered)return;
    snprintf(message,sizeof message,"%s %d",accept?"DRAW_ACCEPT":"DRAW_OFFER",g->p2p_generation);
    if(p2p_send(g,message)){
        if(accept)p2p_result(g,"1/2-1/2");else g->p2p_draw_sent=true;
    }
}
static void p2p_clock_update(Gui *g,Uint32 now)
{
    Uint32 elapsed=now-g->p2p_last_tick;g->p2p_last_tick=now;
    if(g->p2p_ready&&g->p2p_base&&!p2p_finished(g)){
        int side=g->board.side;
        g->p2p_remaining[side]=elapsed>=(Uint32)g->p2p_remaining[side]?0:g->p2p_remaining[side]-(int)elapsed;
        if(!g->p2p_remaining[side]&&transport_role(g->net)==NET_ROLE_HOST){
            char message[80];snprintf(message,sizeof message,"TIMEOUT %d %d",g->p2p_generation,side);
            if(p2p_send(g,message))p2p_result(g,side==WHITE?"0-1":"1-0");
        }
    }
}
static void p2p_tick(Gui *g,Uint32 now)
{
    if(!net_is_p2p(g->net))return;
    bool host=transport_role(g->net)==NET_ROLE_HOST;
    NetState state=transport_state(g->net);
    if(state==NET_STATE_CLOSED){p2p_disconnect(g);return;}
    if(!g->p2p_started){
        if(state==NET_STATE_CONNECTED&&host)p2p_host_start(g,now);
        else if(now-g->p2p_connected_at>600000){p2p_disconnect(g);return;}
        else if(!g->p2p_export[0]&&net_p2p_export(g->net,g->p2p_export,sizeof g->p2p_export))
            p2p_status(g,host?"Invitation ready. Copy and send it, then paste the response.":"Response ready. Copy and return it to your friend.",host?"邀请已就绪。复制发送给好友，再粘贴好友的回应。":"回应已就绪。复制并回传给好友。");
    }
    if(g->p2p_started&&g->mode==MODE_LOCAL)p2p_clock_update(g,now);
    char message[1024];int r;
    while(g->net&&(r=transport_poll(g->net,message,sizeof message))==1){
        g->p2p_last_rx=now;
        int gen=0,ply=0,a=0,b=0,c=0;char uci[8],extra;
        if(!strncmp(message,"NAME ",5)){
            if(strlen(message+5)<sizeof g->p2p_peer_name){snprintf(g->p2p_peer_name,sizeof g->p2p_peer_name,"%s",message+5);if(g->p2p_started)snprintf(g->local_color==WHITE?g->black_name:g->white_name,sizeof g->white_name,"%.63s",g->p2p_peer_name);}
        }else if(sscanf(message,"START %d %d %d %d %c",&gen,&a,&b,&c,&extra)==4){
            if(host||gen!=g->p2p_generation+1||a<0||a>86400||b<0||b>300||(c!=0&&c!=1)||
               (g->p2p_started&&(!p2p_finished(g)||!g->p2p_vote||!g->p2p_peer_vote))){p2p_disconnect(g);return;}
            p2p_begin(g,gen,a,b,c,now);g->p2p_ready=true;
            snprintf(message,sizeof message,"READY %d",gen);if(!p2p_send(g,message))return;
        }else if(sscanf(message,"READY %d %c",&gen,&extra)==1){
            if(host&&gen==g->p2p_generation&&!g->p2p_ready){g->p2p_ready=true;g->p2p_last_tick=now;}
        }else if(sscanf(message,"MOVE2 %d %d %7s %c",&gen,&ply,uci,&extra)==3){
            if(gen!=g->p2p_generation||ply<g->ply)continue;
            Move m;
            if(!g->p2p_ready||p2p_finished(g)||ply!=g->ply||g->board.side==g->local_color||!ai_uci_to_move(&g->board,uci,&m)){p2p_disconnect(g);return;}
            Color mover=g->board.side;push_move(g,m);g->net_sent_ply=g->ply;
            if(host&&g->p2p_base)g->p2p_remaining[mover]+=g->p2p_increment;
        }else if(sscanf(message,"CLOCK %d %d %d %d %c",&gen,&ply,&a,&b,&extra)==4){
            if(!host&&gen==g->p2p_generation&&ply==g->ply&&a>=0&&b>=0){g->p2p_remaining[0]=a;g->p2p_remaining[1]=b;}
        }else if(sscanf(message,"RESIGN %d %c",&gen,&extra)==1){
            if(gen==g->p2p_generation&&!p2p_finished(g))p2p_result(g,g->local_color==WHITE?"1-0":"0-1");
        }else if(sscanf(message,"TIMEOUT %d %d %c",&gen,&a,&extra)==2){
            if(!host&&gen==g->p2p_generation&&(a==WHITE||a==BLACK)&&!p2p_finished(g))p2p_result(g,a==WHITE?"0-1":"1-0");
        }else if(sscanf(message,"REMATCH %d %c",&gen,&extra)==1){
            if(gen==g->p2p_generation&&p2p_finished(g)){g->p2p_peer_vote=true;set_msg(g,ui_tr(g,"Opponent requests a rematch","对方请求再来一局"),NULL);}
        }else if(sscanf(message,"DRAW_OFFER %d %c",&gen,&extra)==1){
            if(gen==g->p2p_generation&&!p2p_finished(g)){g->draw_offered=true;set_msg(g,ui_tr(g,"Draw offered","对方提议和棋"),NULL);}
        }else if(sscanf(message,"DRAW_ACCEPT %d %c",&gen,&extra)==1){
            if(gen==g->p2p_generation&&g->p2p_draw_sent&&!p2p_finished(g))p2p_result(g,"1/2-1/2");
        }else if(!strcmp(message,"PING")){if(!p2p_send(g,"PONG"))return;}
        else if(!strcmp(message,"BYE")){p2p_disconnect(g);return;}
    }
    if(!g->net)return;
    if(g->p2p_started&&g->mode==MODE_LOCAL){
        while(g->net_sent_ply<g->ply){
            char uci[8];move_to_uci(g->history[g->net_sent_ply],uci);
            snprintf(message,sizeof message,"MOVE2 %d %d %s",g->p2p_generation,g->net_sent_ply,uci);
            if(!p2p_send(g,message))return;
            if(g->p2p_base)g->p2p_remaining[g->before[g->net_sent_ply].side]+=g->p2p_increment;
            g->net_sent_ply++;
        }
        if(p2p_finished(g)&&!g->p2p_scored){
            const char *result=g->override_result[0]?g->override_result:pgn_result(&g->board,g->state);
            if(strcmp(result,"*")){bool white=!strcmp(result,"1-0"),draw=!strcmp(result,"1/2-1/2");
                g->p2p_score[draw?1:((white==(g->local_color==WHITE))?0:2)]++;g->p2p_scored=true;}
        }
        if(host&&g->p2p_vote&&g->p2p_peer_vote){p2p_host_start(g,now);return;}
        if(host&&now-g->p2p_clock_sent>=500){g->p2p_clock_sent=now;
            snprintf(message,sizeof message,"CLOCK %d %d %d %d",g->p2p_generation,g->ply,g->p2p_remaining[0],g->p2p_remaining[1]);if(!p2p_send(g,message))return;}
        if(now-g->p2p_last_rx>30000){p2p_disconnect(g);return;}
        if(now-g->p2p_last_ping>=3000){g->p2p_last_ping=now;p2p_send(g,"PING");}
    }
}
