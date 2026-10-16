#include "../src/gui.h"
#include "../src/paths.h"
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void capture(Gui *g,SDL_Renderer *r,const char *name){gui_render(g,r);SDL_RenderSetViewport(r,NULL);SDL_RenderSetScale(r,1,1);SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,WIN_W,WIN_H,32,SDL_PIXELFORMAT_RGBA32);if(SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch)!=0)fprintf(stderr,"ReadPixels: %s\n",SDL_GetError());char path[256];snprintf(path,sizeof path,"/tmp/oc-v21-%s.png",name);IMG_SavePNG(s,path);SDL_FreeSurface(s);}
static void click(Gui *g,int x,int y){SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;gui_handle_event(g,&e);}
int main(void){SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("XDG_DATA_HOME","/tmp/openchess-ui21-test",1);if(SDL_Init(SDL_INIT_VIDEO)||TTF_Init())return 1;IMG_Init(IMG_INIT_PNG);
 SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,WIN_W,WIN_H,32,SDL_PIXELFORMAT_RGBA32);SDL_Renderer *r=SDL_CreateSoftwareRenderer(surface);Gui *g=gui_create();gui_init_assets(g,r);if(!g->ui||!g->ui->store||!g->font_ui||!g->puzzles)return 1;
 capture(g,r,"home");g->ui->zh=true;capture(g,r,"home-zh");g->ui->nav_hover=2;capture(g,r,"submenu");g->ui->nav_hover=-1;
 g->scene=SCENE_ANALYSIS_HOME;capture(g,r,"analysis");click(g,g->panel_x+70,335);if(g->scene!=SCENE_POSITION_SETUP)return 2;capture(g,r,"setup");
 g->scene=SCENE_MENU;click(g,1150,800);if(g->mode!=MODE_PUZZLE||g->ui->current_entry<0)return 3;capture(g,r,"daily");
 g->scene=SCENE_PROFILE;capture(g,r,"profile");g->scene=SCENE_SOCIAL;capture(g,r,"social");g->scene=SCENE_PREFERENCES;capture(g,r,"preferences");
 gui_destroy(g);SDL_DestroyRenderer(r);SDL_FreeSurface(surface);TTF_Quit();SDL_Quit();puts("V2.1 UI screens rendered and navigation passed.");return 0;}
