/* Test hote de apps/settings/settings_view.c : rendu clair/sombre avec les vraies photos, IDEMPOTENCE des repeints
   partiels (5 repeints = memes pixels, survol reversible a l identique), clic complet sur chaque widget, geometrie
   a 3 tailles dont la plus petite (640x480). Le pilote graphique est compile sur l hote avec un faux framebuffer
   (voir CORRECTIFS.md). Les chemins /home/claude/... sont a adapter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui/ui.h"
#include "gui/theme.h"
#include "apps/settings/settings_view.h"
void console_write(const char*t){(void)t;}
extern unsigned char* g_fb;
#define W 1480
#define H 520
#define CHECK(c) do{ if(!(c)){printf("ECHEC: %s (ligne %d)\n",#c,__LINE__);return 1;} }while(0)
/* Stub : la vraie fonction est dans gui/desktop.c ; ici on echantillonne les vraies photos. */
static const char* NAMES[6]={"brume","collines","cameleon","lune","nebuleuse","satellite"};
void gui_paint_photo_area(u32 x,u32 y,u32 w,u32 h,u32 idx){
  char p[200]; snprintf(p,200,"/home/claude/mikea-os-main/assets/wallpapers/%s.rgb",NAMES[idx]);
  FILE*f=fopen(p,"rb"); if(!f) return; static unsigned char b[112*84*3]; fread(b,1,sizeof b,f); fclose(f);
  for(u32 j=0;j<h;j++)for(u32 i=0;i<w;i++){ unsigned char*s=&b[((j*84/h)*112+(i*112/w))*3]; gfx_fill_rect(x+i,y+j,1,1,(s[0]<<16)|(s[1]<<8)|s[2]); } }
static unsigned long hash(void){ unsigned long h=1469598103934665603UL; for(int i=0;i<5920*520;i++){h^=g_fb[i]; h*=1099511628211UL;} return h; }

static void frame(int win_x,int win_y,int win_w,int win_h,int dark){
  theme_set_dark(dark);
  int px=win_x*16,py=win_y*16,pw=win_w*16,ph=win_h*16;
  gfx_fill_rect(px-32,0,pw+64,H,theme_desktop_bg());
  gfx_fill_rounded_rect_blend(px,py,pw,ph,10,theme_panel(),theme_panel_opacity());
  gfx_fill_rect_blend(px,py,pw,20,theme_titlebar_bg(),95);
  gfx_fill_circle(px+14,py+10,5,0xE0564F); gfx_fill_circle(px+32,py+10,5,0xFEBC2E); gfx_fill_circle(px+50,py+10,5,0x28C840);
  u32 tw=gfx_text_width("Réglages",GFX_FONT_BODY); gfx_draw_text_ui_vcenter(px+(pw-tw)/2,py,20,"Réglages",theme_titlebar_text(),GFX_FONT_BODY);
}
int main(void){
  graphics_init();
  settings_model m={ "mikea",1,0,1,3,"Dégradé",880,1024,412,1024,2026,10,27,5,43 };
  settings_layout L; settings_actions act; ui_ctx ui;
  /* --- scene claire : survol du bouton Deconnexion ; scene sombre : appui sur une vignette --- */
  for(int s=0;s<2;s++){
    int wx = s? 48:2; int dark=s; m.dark=dark;
    frame(wx,2,44,27,dark); settings_view_layout(&L,wx,2,44,27); settings_view_paint_cards(&L);
    memset(&ui,0,sizeof ui);
    int mx = s? L.swatch[1].x+10 : L.logout_btn.x+20, my = s? L.swatch[1].y+10 : L.logout_btn.y+10;
    ui_begin(&ui,mx,my,0); settings_view_paint(&ui,&L,&m,&act);
    unsigned long h1=hash();
    for(int i=0;i<5;i++){ ui_begin(&ui,mx,my,0); settings_view_paint(&ui,&L,&m,&act); }
    CHECK(hash()==h1);                                   /* 5 repeints = memes pixels */
    /* survol ailleurs puis retour : retrouve exactement l'image initiale */
    ui_begin(&ui,L.theme_toggle.x+5,L.theme_toggle.y+5,0); settings_view_paint(&ui,&L,&m,&act);
    CHECK(hash()!=h1);                                   /* le survol change bien quelque chose */
    ui_begin(&ui,mx,my,0); settings_view_paint(&ui,&L,&m,&act);
    CHECK(hash()==h1);                                   /* ...et c'est reversible a l'identique */
    CHECK(settings_view_hit(&L,mx,my)== (s? SETTINGS_ID_SWATCH0+1 : SETTINGS_ID_LOGOUT));
  }
  /* --- actions : un clic complet sur chaque widget --- */
  frame(2,2,44,27,0); m.dark=0; settings_view_layout(&L,2,2,44,27); settings_view_paint_cards(&L);
  struct { ui_rect r; int id; } T[] = {
    {L.theme_toggle,SETTINGS_ID_THEME},{L.sound_toggle,SETTINGS_ID_SOUND},{L.pattern_btn,SETTINGS_ID_PATTERN},{L.logout_btn,SETTINGS_ID_LOGOUT},{L.swatch[4],SETTINGS_ID_SWATCH0+4} };
  for(int i=0;i<5;i++){ memset(&ui,0,sizeof ui); int cx=T[i].r.x+T[i].r.w/2, cy=T[i].r.y+T[i].r.h/2;
    ui_begin(&ui,cx,cy,0); settings_view_paint(&ui,&L,&m,&act);
    ui_begin(&ui,cx,cy,1); settings_view_paint(&ui,&L,&m,&act); CHECK(act.toggle_theme+act.toggle_sound+act.pattern+act.logout==0 && act.wallpaper==-1);
    ui_begin(&ui,cx,cy,0); settings_view_paint(&ui,&L,&m,&act);
    int want_theme=T[i].id==SETTINGS_ID_THEME, want_sound=T[i].id==SETTINGS_ID_SOUND, want_pat=T[i].id==SETTINGS_ID_PATTERN, want_out=T[i].id==SETTINGS_ID_LOGOUT;
    CHECK(act.toggle_theme==want_theme && act.toggle_sound==want_sound && act.pattern==want_pat && act.logout==want_out);
    CHECK(act.wallpaper == (T[i].id==SETTINGS_ID_SWATCH0+4 ? 4 : -1)); }
  /* --- geometrie : rien ne deborde de la fenetre, a la taille par defaut et a la taille minimale (640x480) --- */
  int sizes[][4]={{2,2,44,27},{2,2,36,26},{0,0,40,28}};
  for(int k=0;k<3;k++){ settings_view_layout(&L,sizes[k][0],sizes[k][1],sizes[k][2],sizes[k][3]);
    int x0=sizes[k][0]*16,y0=sizes[k][1]*16,x1=x0+sizes[k][2]*16,y1=y0+sizes[k][3]*16;
    ui_rect* all[]={&L.card_session,&L.card_appearance,&L.card_wallpaper,&L.card_system,&L.card_footer,&L.logout_btn,&L.pattern_btn,&L.theme_toggle,&L.sound_toggle,&L.swatch[0],&L.swatch[5]};
    for(int i=0;i<11;i++){ ui_rect r=*all[i]; CHECK(r.w>0&&r.h>0&&r.x>=x0&&r.y>=y0+20&&r.x+r.w<=x1&&r.y+r.h<=y1); } }
  printf("OK : idempotence, survol reversible, 5 clics, geometrie 3 tailles\n");
  /* rendu final : refaire les 2 scenes proprement pour l'image */
  for(int s=0;s<2;s++){ int wx=s?48:2; m.dark=s; frame(wx,2,44,27,s); settings_view_layout(&L,wx,2,44,27); settings_view_paint_cards(&L);
    memset(&ui,0,sizeof ui); int mx=s?L.swatch[1].x+10:L.logout_btn.x+20,my=s?L.swatch[1].y+10:L.logout_btn.y+10; ui_begin(&ui,mx,my,0); settings_view_paint(&ui,&L,&m,&act); }
  FILE*f=fopen("sv.ppm","wb"); fprintf(f,"P6\n%d %d\n255\n",W,H);
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){unsigned char*p=g_fb+y*5920+x*4;fputc(p[2],f);fputc(p[1],f);fputc(p[0],f);} fclose(f); return 0; }
