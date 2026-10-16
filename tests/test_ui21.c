#include "../src/gui.h"
#include "../src/paths.h"
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void capture(Gui *g,SDL_Renderer *r,const char *name){gui_render(g,r);SDL_RenderSetViewport(r,NULL);SDL_RenderSetScale(r,1,1);int width,height;SDL_GetRendererOutputSize(r,&width,&height);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);if(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch)!=0)fprintf(stderr,"ReadPixels: %s\n",SDL_GetError());char path[256];snprintf(path,sizeof path,"/tmp/oc-v21-%s.png",name);IMG_SavePNG(s,path);SDL_FreeSurface(s);}
static void click(Gui *g,int x,int y){SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;gui_handle_event(g,&e);}
int main(void)
{SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("XDG_DATA_HOME","/tmp/openchess-ui21-test",1);if(SDL_Init(SDL_INIT_VIDEO)||TTF_Init())return 1;IMG_Init(IMG_INIT_PNG);
 SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,WIN_W,WIN_H,32,SDL_PIXELFORMAT_RGBA32);SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);Gui *g=gui_create();gui_init_assets(g,r);if(!g->ui||!g->ui->store||!g->font_ui||!g->puzzles)return 1;
 capture(g,r,"home");g->ui->zh=true;capture(g,r,"home-zh");g->ui->nav_hover=2;capture(g,r,"submenu");g->ui->nav_hover=-1;
 g->scene=SCENE_ANALYSIS_HOME;snprintf(g->ui->input,sizeof g->ui->input,"1. e4 Qh9");click(g,g->panel_x+40,260);if(g->scene!=SCENE_ANALYSIS_HOME||!g->ui->input[0]||!g->ui->status[0])return 7;g->ui->input[0]=g->ui->status[0]=0;capture(g,r,"analysis");click(g,g->panel_x+70,335);if(g->scene!=SCENE_POSITION_SETUP)return 2;capture(g,r,"setup");
 g->scene=SCENE_MENU;click(g,1150,800);if(g->mode!=MODE_PUZZLE||g->ui->current_entry<0)return 3;capture(g,r,"daily");
 int daily=g->ui->current_entry;SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_n;gui_handle_event(g,&key);if(g->ui->current_entry<=daily||!g->ui->entries[g->ui->current_entry].day||g->ui->entries[g->ui->current_entry].rated)return 4;
 g->mode=MODE_ANALYSIS;board_reset(&g->board);g->ply=0;g->flipped=false;g->state=NO_GAME_OVER;g->selected=-1;g->targets.count=0;click(g,g->board_x+4*g->sq+g->sq/2,g->board_y+6*g->sq+g->sq/2);if(g->targets.count!=2)return 5;capture(g,r,"legal-indicators");
 g->scene=SCENE_PROFILE;capture(g,r,"profile");g->scene=SCENE_SOCIAL;capture(g,r,"social");g->scene=SCENE_PREFERENCES;capture(g,r,"preferences");
 gui_destroy(g);SDL_DestroyRenderer(r);SDL_FreeSurface(surface);
 SDL_Window *window=SDL_CreateWindow("UI test",0,0,1024,720,SDL_WINDOW_HIDDEN);r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);g=gui_create();gui_init_assets(g,r);SDL_SetWindowSize(window,1024,720);g->board_driven_resize=false;SDL_Event resize={0};resize.type=SDL_WINDOWEVENT;resize.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;resize.window.data1=1024;resize.window.data2=720;gui_handle_event(g,&resize);g->scene=SCENE_MENU;capture(g,r,"compact-top");g->mouse.x=600;SDL_Event wheel={0};wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=-8;gui_handle_event(g,&wheel);if(g->ui->home_scroll<=0)return 6;capture(g,r,"compact-scroll");gui_destroy(g);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);
 TTF_Quit();SDL_Quit();puts("V2.1 UI screens rendered and navigation passed.");return 0;}
