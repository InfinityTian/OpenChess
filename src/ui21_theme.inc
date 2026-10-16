/* Application chrome palettes are independent of chessboard/piece themes. */
static SDL_Color ui_ink={30,54,78,255},ui_muted={89,113,136,255};
static SDL_Color ui_bg={234,243,251,255},ui_surface={250,253,255,255},ui_sidebar={218,234,247,255};
static SDL_Color ui_control={226,237,248,255},ui_hover={192,216,240,255},ui_border={166,189,211,255},ui_accent={42,115,206,255};
static bool ui_dark;
static const char *ui_theme_names[]={"Sky","Ocean","Mint","Forest","Lavender","Rose","Peach","Sand","Slate","Aqua"};
static const char *ui_theme_zh[]={"浅蓝","海洋","薄荷","森林","薰衣草","玫瑰","蜜桃","暖沙","岩灰","青碧"};
static const SDL_Color ui_accents[]={{42,115,206,255},{29,97,164,255},{29,121,100,255},{61,116,70,255},{111,79,177,255},{172,65,103,255},{169,82,39,255},{132,102,41,255},{84,105,128,255},{16,120,136,255}};
static SDL_Color ui_mix(SDL_Color a,SDL_Color b,float t){return (SDL_Color){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,255};}
static void ui_apply_theme(Gui *g)
{
    int index=g->ui?clampi(g->ui->app_theme,0,9):0;ui_dark=g->ui&&g->ui->dark_mode;SDL_Color accent=ui_accents[index];
    ui_accent=accent;
    if(ui_dark){
        ui_bg=ui_mix((SDL_Color){19,24,32,255},accent,0.07f);ui_surface=ui_mix((SDL_Color){29,36,46,255},accent,0.08f);
        ui_sidebar=ui_mix((SDL_Color){24,31,42,255},accent,0.10f);ui_control=ui_mix((SDL_Color){43,52,65,255},accent,0.12f);
        ui_hover=ui_mix((SDL_Color){52,68,85,255},accent,0.20f);ui_border=(SDL_Color){79,97,116,255};
        ui_ink=(SDL_Color){233,240,248,255};ui_muted=(SDL_Color){174,190,208,255};ui_accent=ui_mix(accent,(SDL_Color){90,160,240,255},0.18f);
    }else{
        ui_bg=index?ui_mix((SDL_Color){250,251,252,255},accent,0.075f):(SDL_Color){234,243,251,255};
        ui_surface=(SDL_Color){250,253,255,255};ui_sidebar=ui_mix((SDL_Color){241,247,252,255},accent,0.10f);
        ui_control=ui_mix((SDL_Color){241,247,252,255},accent,0.075f);ui_hover=ui_mix((SDL_Color){230,240,249,255},accent,0.16f);
        ui_border=ui_mix((SDL_Color){189,205,220,255},accent,0.18f);ui_ink=(SDL_Color){30,54,78,255};ui_muted=(SDL_Color){79,101,123,255};
        if(index==0){ui_sidebar=(SDL_Color){218,234,247,255};ui_control=(SDL_Color){226,237,248,255};ui_hover=(SDL_Color){192,216,240,255};ui_border=(SDL_Color){166,189,211,255};}
    }
}
static SDL_Color ui_surface_color(SDL_Color c)
{
    SDL_Color result=c;
    if(c.r==250&&c.g==253&&c.b==255)result=ui_surface;
    else if(c.r==218&&c.g==234&&c.b==247)result=ui_sidebar;
    else if(c.r==226&&c.g==237&&c.b==248)result=ui_control;
    else if(c.r==192&&c.g==216&&c.b==240)result=ui_hover;
    else if(c.r==166&&c.g==189&&c.b==211)result=ui_border;
    else if(c.r==42&&c.g==115&&c.b==206)result=ui_accent;
    else if(c.r==242&&c.g==248&&c.b==253)result=ui_surface;
    else if(ui_dark&&c.a==255&&c.r>170&&c.g>170&&c.b>170){
        if(c.g>c.r+12&&c.g>c.b+12)result=(SDL_Color){43,84,61,255};
        else if(c.r>c.b+30)result=(SDL_Color){99,77,42,255};
        else result=ui_control;
    }
    result.a=c.a;return result;
}
static SDL_Color ui_legacy_text_color(Gui *g,TTF_Font *font,SDL_Color c)
{
    if(!g->ui||!g->ui->legacy_paint||font==g->font_piece[0]||font==g->font_piece[1])return c;
    int high=c.r>c.g?c.r:c.g;if(c.b>high)high=c.b;int low=c.r<c.g?c.r:c.g;if(c.b<low)low=c.b;
    float luminance=0.2126f*c.r+0.7152f*c.g+0.0722f*c.b;
    if(high-low<42){SDL_Color out=(ui_dark?luminance<150:luminance<165)?ui_muted:ui_ink;out.a=c.a;return out;}
    if(!ui_dark&&luminance>135){float scale=110.0f/luminance;c.r*=scale;c.g*=scale;c.b*=scale;}
    else if(ui_dark&&luminance<150){SDL_Color out=ui_mix(c,(SDL_Color){235,242,250,255},0.63f);out.a=c.a;return out;}
    return c;
}
