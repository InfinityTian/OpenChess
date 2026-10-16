#include "../src/gui.h"
#include "../src/paths.h"
#include "../src/fen.h"
#include "../src/move.h"
#include "../src/ai.h"
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void capture(Gui *g,SDL_Renderer *r,const char *name){gui_render(g,r);SDL_RenderSetViewport(r,NULL);SDL_RenderSetScale(r,1,1);int width,height;SDL_GetRendererOutputSize(r,&width,&height);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);if(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch)!=0)fprintf(stderr,"ReadPixels: %s\n",SDL_GetError());char path[256];snprintf(path,sizeof path,"/tmp/oc-v21-%s.png",name);IMG_SavePNG(s,path);SDL_FreeSurface(s);}
static void click(Gui *g,int x,int y){SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;gui_handle_event(g,&e);}
static void keypress(Gui *g,SDL_Keycode key){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;gui_handle_event(g,&e);}
static void puzzle_fixture(Gui *g,const char *id,const char *fen,const char *moves){
 Ui21 *u=g->ui;PuzzleEntry *e=&u->entries[0];memset(e,0,sizeof *e);snprintf(e->puzzle.id,sizeof e->puzzle.id,"%s",id);snprintf(e->puzzle.fen,sizeof e->puzzle.fen,"%s",fen);snprintf(e->puzzle.moves,sizeof e->puzzle.moves,"%s",moves);e->puzzle.rating=1500;e->step=1;snprintf(e->attempt,sizeof e->attempt,"%s",id);workspace_issue(u->store,u->owner,id,&e->puzzle,false,0);
 u->current_entry=-1;u->entry_count=1;u->reviewing=false;g->scene=SCENE_GAME;g->mode=MODE_PUZZLE;click(g,g->board_x+30,g->board_y+8*g->sq+40);u->reviewing=false;g->puzzle_counted=false;
}
static void square_click(Gui *g,const char *name){int file=name[0]-'a',rank=name[1]-'1';click(g,g->board_x+(g->flipped?7-file:file)*g->sq+g->sq/2,g->board_y+(g->flipped?rank:7-rank)*g->sq+g->sq/2);}
static void promotion_click(Gui *g,int choice){int file=g->promo_to%8,rank=g->promo_to/8;int sx=g->board_x+(g->flipped?7-file:file)*g->sq,sy=g->board_y+(g->flipped?rank:7-rank)*g->sq;int x=sx-8,y=sy-90;if(y<g->board_y)y=sy+g->sq+10;if(x<g->board_x)x=sx;if(x+216>g->board_x+8*g->sq)x=g->board_x+8*g->sq-216;click(g,x+choice*54+25,y+23);}
int main(void)
{SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("OPENCHESS_CONFIG","/tmp/openchess-ui21-test/chess.conf",1);SDL_setenv("XDG_DATA_HOME","/tmp/openchess-ui21-test",1);if(SDL_Init(SDL_INIT_VIDEO)||TTF_Init())return 1;IMG_Init(IMG_INIT_PNG);
 SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,WIN_W,WIN_H,32,SDL_PIXELFORMAT_RGBA32);SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);Gui *g=gui_create();gui_init_assets(g,r);if(!g->ui||!g->ui->store||!g->font_ui||!g->puzzles)return 1;
 capture(g,r,"home");g->ui->zh=true;capture(g,r,"home-zh");g->ui->nav_hover=2;capture(g,r,"submenu");g->ui->nav_hover=-1;
 g->scene=SCENE_ANALYSIS_HOME;snprintf(g->ui->input,sizeof g->ui->input,"1. e4 Qh9");click(g,g->panel_x+40,260);if(g->scene!=SCENE_ANALYSIS_HOME||!g->ui->input[0]||!g->ui->status[0])return 7;g->ui->input[0]=g->ui->status[0]=0;capture(g,r,"analysis");click(g,g->panel_x+70,335);if(g->scene!=SCENE_POSITION_SETUP)return 2;capture(g,r,"setup");
 g->scene=SCENE_MENU;click(g,1150,800);if(g->mode!=MODE_PUZZLE||g->ui->current_entry<0)return 3;capture(g,r,"daily");
 int daily=g->ui->current_entry;SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_n;gui_handle_event(g,&key);if(g->ui->current_entry!=daily)return 23;key.key.keysym.sym=SDLK_r;gui_handle_event(g,&key);key.key.keysym.sym=SDLK_n;gui_handle_event(g,&key);if(g->ui->current_entry<=daily||!g->ui->entries[g->ui->current_entry].day||g->ui->entries[g->ui->current_entry].rated)return 4;
 g->mode=MODE_ANALYSIS;board_reset(&g->board);g->ply=0;g->flipped=false;g->state=NO_GAME_OVER;g->selected=-1;g->targets.count=0;click(g,g->board_x+4*g->sq+g->sq/2,g->board_y+6*g->sq+g->sq/2);if(g->targets.count!=2)return 5;capture(g,r,"legal-indicators");
 g->scene=SCENE_PROFILE;capture(g,r,"profile");g->scene=SCENE_SOCIAL;capture(g,r,"social");g->ui->peer=12;g->ui->messages=cJSON_Parse("{\"rows\":[{\"id\":42}]}");click(g,800,710);if(g->ui->message_cursor!=42)return 8;click(g,1000,710);if(g->ui->message_cursor!=0)return 9;cJSON_Delete(g->ui->messages);g->ui->messages=NULL;g->scene=SCENE_PREFERENCES;capture(g,r,"preferences");

 /* Theme selection, independent dark toggle and persisted preferences. */
 g->ui->dark_mode=false;g->ui->app_theme=0;g->scene=SCENE_PREFERENCES;click(g,1000,420);if(!g->ui->dark_mode)return 10;click(g,500,620);if(g->ui->app_theme!=6)return 11;capture(g,r,"dark-preferences");
 gui_save_config(g);g->ui->dark_mode=false;g->ui->app_theme=0;gui_load_config(g,path_config());if(!g->ui->dark_mode||g->ui->app_theme!=6)return 12;
 for(int mode=0;mode<2;mode++)for(int theme=0;theme<10;theme++){g->ui->dark_mode=mode!=0;g->ui->app_theme=theme;g->scene=SCENE_MENU;char name[64];snprintf(name,sizeof name,"theme-%d-%d",theme,mode);capture(g,r,name);}
 g->ui->app_theme=0;g->ui->dark_mode=false;g->scene=SCENE_APPEARANCE;g->ape_tab=1;capture(g,r,"piece-previews");g->ape_tab=2;g->ape_started=SDL_GetTicks()-300;capture(g,r,"animation-previews");
 /* Both pickers browse independently without silently applying a new style. */
 for(int tab=0;tab<2;tab++){
   g->scene=SCENE_APPEARANCE;g->ape_tab=tab;g->ape_page[tab]=0;
   int selected=g->ape_sel[tab],applied=tab?g->piece_index:g->board_index;
   click(g,1040,672);if(g->ape_page[tab]!=1||g->ape_sel[tab]!=selected||(tab?g->piece_index:g->board_index)!=applied)return 13;
   capture(g,r,tab?"pieces-page-two":"boards-page-two");
   for(int i=0;i<10;i++)click(g,1040,672);
   int last=((tab?g->pieces.count:g->boards.count)-1)/18;if(g->ape_page[tab]!=last)return 14;
   for(int i=0;i<10;i++)click(g,550,672);if(g->ape_page[tab]!=0)return 15;
 }
 g->scene=SCENE_APPEARANCE;key.key.keysym.sym=SDLK_4;gui_handle_event(g,&key);if(g->ape_tab!=3||g->font_index!=0)return 24;capture(g,r,"font-options");
 key.key.keysym.sym=SDLK_RIGHT;gui_handle_event(g,&key);if(g->font_index!=1)return 25;
 gui_save_config(g);g->font_index=0;gui_load_config(g,path_config());if(g->font_index!=1)return 26;
 key.key.keysym.sym=SDLK_LEFT;gui_handle_event(g,&key);if(g->font_index!=0)return 27;
 g->ui->dark_mode=true;g->scene=SCENE_SETTINGS;capture(g,r,"dark-settings");g->scene=SCENE_GAME;capture(g,r,"dark-analysis");g->ui->dark_mode=false;g->scene=SCENE_GAME;capture(g,r,"light-analysis");
 g->ui->smart=false;g->scene=SCENE_PUZZLE_SETUP;capture(g,r,"puzzle-difficulty");
 /* Dropdown, settings access and live scoring changes. */
 click(g,800,420);if(!g->ui->theme_open)return 16;capture(g,r,"puzzle-theme-dropdown");
 click(g,800,448+4*26+10);if(g->ui->theme_open||g->puzzle_theme!=4)return 17;
 g->puzzle_theme=0;g->offline_mode=true;g->account_local=true;g->ui->rated=true;
 click(g,740,540);if(g->scene!=SCENE_GAME||g->mode!=MODE_PUZZLE)return 18;
 int current=g->ui->current_entry;if(current<0||g->ui->entries[current].day)return 19;
 click(g,g->win_w-130,g->board_y+120);if(g->ui->rated||g->ui->entries[current].rated)return 20;
 click(g,g->win_w-130,g->board_y+120);if(!g->ui->rated||!g->ui->entries[current].rated)return 21;
 capture(g,r,"puzzle-rating-card");g->san_open=true;capture(g,r,"puzzle-move-input");g->san_open=false;
 g->scene=SCENE_SETTINGS;g->ui->dark_mode=false;click(g,g->win_w-120,40);if(!g->ui->dark_mode)return 22;capture(g,r,"settings-dark-toggle");
 /* Promotion menus, retry navigation, next guard, complete answers and clean analysis roots. */
 g->ui->dark_mode=false;
 const char *start_fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
 if(g->pgntree)mt_free(g->pgntree);g->pgntree=mt_new_root(&g->board);g->tree_cur=g->pgntree; /* A previous analysis must not leak into puzzle rows. */
 puzzle_fixture(g,"ui-white-promotion","8/P6k/8/8/8/8/7K/8 b - - 0 1","h7h6 a7a8n");
 int old_entry=g->ui->current_entry;keypress(g,SDLK_n);if(g->ui->current_entry!=old_entry)return 28;
 square_click(g,"a7");capture(g,r,"promotion-white-dot");square_click(g,"a8");if(g->promo_from<0)return 29;capture(g,r,"puzzle-promotion-menu");
 promotion_click(g,0);if(!g->puzzle_wrong||g->puzzle_badge_kind!=-1)return 30;g->anim_count=0;capture(g,r,"puzzle-incorrect-icon");
 keypress(g,SDLK_n);if(g->ui->current_entry!=old_entry)return 31;
 keypress(g,SDLK_LEFT);if(g->puzzle_wrong||g->board.board[algebraic_to_sq("a7")]!=WP||g->puzzle_step!=1)return 32;
 square_click(g,"a7");square_click(g,"a8");promotion_click(g,3);if(!g->puzzle_done||g->board.board[algebraic_to_sq("a8")]!=WN)return 33;g->anim_count=0;capture(g,r,"puzzle-correct-icon");
 keypress(g,SDLK_n);if(g->ui->current_entry==old_entry||g->puzzle_step!=1||g->puzzle_badge_kind)return 34;capture(g,r,"next-puzzle-clean-moves");if(g->move_hit_count!=1)return 35;
 puzzle_fixture(g,"ui-black-promotion","8/7k/8/8/8/8/p6K/8 w - - 0 1","h2h3 a2a1r");
 square_click(g,"a2");square_click(g,"a1");if(g->promo_from<0)return 36;promotion_click(g,1);if(!g->puzzle_done||g->board.board[algebraic_to_sq("a1")]!=BR)return 37;
 puzzle_fixture(g,"ui-reveal",start_fen,"e2e4 e7e5 g1f3 b8c6");keypress(g,SDLK_r);capture(g,r,"puzzle-answer-rows");if(!g->puzzle_solution_visible||g->ply!=4||g->move_hit_count!=4||g->msg[0])return 38;
 int hit=-1;for(int i=0;i<g->move_hit_count;i++)if(g->move_hit_ply[i]==2)hit=i;if(hit<0)return 39;
 SDL_Rect row=g->move_hit_rect[hit];click(g,row.x+row.w/2,row.y+10);capture(g,r,"puzzle-answer-rewind");if(g->puzzle_step!=1||g->move_hit_count!=4)return 40;
 Board root;fen_parse(start_fen,&root);Move first;uci_to_move(&root,"e2e4",&first);make_move_plumb(&root,first);
 click(g,g->panel_x+60,g->board_y+580);if(g->mode!=MODE_ANALYSIS||g->ply||!g->pgntree||!board_rep_equal(&root,&g->pgntree->board))return 41;capture(g,r,"puzzle-to-analysis");
 gui_destroy(g);SDL_DestroyRenderer(r);SDL_FreeSurface(surface);
 SDL_Window *window=SDL_CreateWindow("UI test",0,0,1024,720,SDL_WINDOW_HIDDEN);r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);g=gui_create();gui_init_assets(g,r);SDL_SetWindowSize(window,1024,720);g->board_driven_resize=false;SDL_Event resize={0};resize.type=SDL_WINDOWEVENT;resize.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;resize.window.data1=1024;resize.window.data2=720;gui_handle_event(g,&resize);g->scene=SCENE_MENU;capture(g,r,"compact-top");g->mouse.x=600;SDL_Event wheel={0};wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=-8;gui_handle_event(g,&wheel);if(g->ui->home_scroll<=0)return 6;capture(g,r,"compact-scroll");gui_destroy(g);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);
 TTF_Quit();SDL_Quit();puts("V2.1 UI screens rendered and navigation passed.");return 0;}
