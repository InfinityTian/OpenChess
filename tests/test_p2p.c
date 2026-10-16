#include "../src/gui.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);failures++;}}while(0)
static void pump(Gui *a,Gui *b,int ms){Uint32 start=SDL_GetTicks();do{Uint32 frame=SDL_GetTicks();gui_tick(a,frame);SDL_Delay(2);gui_tick(b,frame);SDL_Delay(3);}while(SDL_GetTicks()-start<(Uint32)ms);}
static void key(Gui *g,SDL_Keycode code,Uint16 mod){SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=code;e.key.keysym.mod=mod;gui_handle_event(g,&e);}
static void move(Gui *g,const char *san){key(g,SDLK_SPACE,0);SDL_Event e;SDL_zero(e);e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof e.text.text,"%s",san);gui_handle_event(g,&e);key(g,SDLK_RETURN,0);}
static bool export_wait(Transport *t,char *ticket,size_t size){for(int k=0;k<1000;k++){if(net_p2p_export(t,ticket,size))return true;SDL_Delay(10);}return false;}
int main(int argc,char **argv)
{
    (void)argc;(void)argv;
    SDL_setenv("XDG_DATA_HOME","/tmp/openchess-p2p-test-data",1);SDL_Init(0);
    if(!net_p2p_available()){puts("SKIP: WebRTC dependency not built");return getenv("OPENCHESS_REQUIRE_P2P")?1:0;}
    Gui *host=gui_create(),*guest=gui_create();CHECK(host&&guest);if(!host||!guest)return 1;
    host->scene=guest->scene=SCENE_P2P;host->p2p_time=8;host->p2p_side=1;
    host->net=net_p2p_create(true,"");guest->net=net_p2p_create(false,"");
    CHECK(host->net&&guest->net);if(!host->net||!guest->net)return 1;
    char invitation[65536],response[65536];
    CHECK(!net_p2p_import(guest->net,"malformed"));
    CHECK(export_wait(host->net,invitation,sizeof invitation));
    CHECK(!net_p2p_import(host->net,invitation)); /* reject wrong direction */
    CHECK(net_p2p_import(guest->net,invitation));
    CHECK(!net_p2p_import(guest->net,invitation)); /* no renegotiation injection */
    CHECK(export_wait(guest->net,response,sizeof response));
    CHECK(net_p2p_import(host->net,response));
    for(int k=0;k<4000&&(!host->p2p_ready||!guest->p2p_ready);k++)pump(host,guest,5);
    if(!host->p2p_ready||!guest->p2p_ready)fprintf(stderr,"P2P handshake: host(state=%d gen=%d scene=%d status=%s) guest(state=%d gen=%d scene=%d status=%s)\n",transport_state(host->net),host->p2p_generation,host->scene,host->p2p_status,transport_state(guest->net),guest->p2p_generation,guest->scene,guest->p2p_status);
    CHECK(host->p2p_ready&&guest->p2p_ready);CHECK(host->local_color==BLACK&&guest->local_color==WHITE);
    CHECK(host->p2p_base==600000&&guest->p2p_increment==5000);
    if(!host->p2p_ready||!guest->p2p_ready){gui_destroy(host);gui_destroy(guest);return 1;}
    int rating=host->puzzle_rating;
    move(guest,"e4");pump(host,guest,150);CHECK(host->ply==1&&guest->ply==1);CHECK(!memcmp(host->board.board,guest->board.board,sizeof host->board.board));
    /* Home navigation keeps the transport and clock running. */
    host->scene=SCENE_MENU;int old=host->p2p_remaining[BLACK];pump(host,guest,120);CHECK(host->p2p_remaining[BLACK]<old);
    host->scene=SCENE_GAME;move(host,"e5");pump(host,guest,150);CHECK(host->ply==2&&guest->ply==2);
    CHECK(transport_send(host->net,"MOVE2 1 1 e7e5"));pump(host,guest,70);CHECK(guest->ply==2&&guest->net);
    key(host,SDLK_r,KMOD_CTRL);pump(host,guest,100);CHECK(!strcmp(host->override_result,"1-0"));CHECK(!strcmp(guest->override_result,"1-0"));
    CHECK(host->p2p_score[2]==1&&guest->p2p_score[0]==1);
    key(host,SDLK_r,KMOD_CTRL);pump(host,guest,50);CHECK(host->p2p_generation==1);
    key(guest,SDLK_r,KMOD_CTRL);pump(host,guest,150);
    CHECK(host->p2p_generation==2&&guest->p2p_generation==2);CHECK(!host->ply&&!guest->ply);
    CHECK(host->local_color==WHITE&&guest->local_color==BLACK);CHECK(host->p2p_ready&&guest->p2p_ready);
    CHECK(host->puzzle_rating==rating);
    key(host,SDLK_d,KMOD_CTRL);pump(host,guest,80);CHECK(guest->draw_offered);
    key(guest,SDLK_a,KMOD_CTRL);pump(host,guest,100);CHECK(!strcmp(host->override_result,"1/2-1/2"));CHECK(!strcmp(guest->override_result,"1/2-1/2"));
    key(guest,SDLK_r,KMOD_CTRL);key(host,SDLK_r,KMOD_CTRL);pump(host,guest,150);CHECK(host->p2p_generation==3);
    host->p2p_remaining[WHITE]=5;pump(host,guest,70);
    CHECK(!strcmp(host->override_result,"0-1")&&!strcmp(guest->override_result,"0-1"));
    key(guest,SDLK_r,KMOD_CTRL);key(host,SDLK_r,KMOD_CTRL);pump(host,guest,150);CHECK(host->p2p_generation==4);
    /* Invalid future move stops the receiver instead of changing its board. */
    CHECK(transport_send(host->net,"MOVE2 4 42 e2e4"));pump(host,guest,150);CHECK(!guest->net);CHECK(!guest->ply);CHECK(!strcmp(guest->override_result,"*"));
    gui_destroy(host);gui_destroy(guest);SDL_Quit();
    if(failures){fprintf(stderr,"%d FAILURES\n",failures);return 1;}
    puts("P2P invitation, encrypted loopback, colors, clocks, background, moves, duplicates, resign, rematch, draw and malformed moves: OK");return 0;
}
