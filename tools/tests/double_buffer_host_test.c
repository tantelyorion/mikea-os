/* Test hote du DOUBLE BUFFERING (kernel/drivers/graphics) : memoire video intacte pendant le dessin, rendu final
   identique octet par octet au mode direct, present partiel, filet de securite gfx_tick, gfx_set_direct (panique),
   repli si RAM insuffisante. Le pilote est compile sur l hote avec une fausse memoire video, un faux tampon et une
   fausse lecture CMOS (voir CORRECTIFS.md). Les chemins /home/claude/... sont a adapter. */
#include <stdio.h>
#include <string.h>
#include "gui/ui.h"
#include "gui/theme.h"
#include "apps/settings/settings_view.h"
void console_write(const char*t){ (void)t; }
extern unsigned char* g_fb; extern unsigned char* g_backmem; extern unsigned long long g_ram_top;
#define SZ (5920*520)
#define CHECK(c) do{ if(!(c)){printf("ECHEC: %s (ligne %d)\n",#c,__LINE__);return 1;} }while(0)
static const char* NAMES[6]={"brume","collines","cameleon","lune","nebuleuse","satellite"};
void gui_paint_photo_area(u32 x,u32 y,u32 w,u32 h,u32 idx){ char p[200]; snprintf(p,200,"/home/claude/mikea-os-main/assets/wallpapers/%s.rgb",NAMES[idx]);
  FILE*f=fopen(p,"rb"); if(!f) return; static unsigned char b[112*84*3]; fread(b,1,sizeof b,f); fclose(f);
  for(u32 j=0;j<h;j++)for(u32 i=0;i<w;i++){ unsigned char*s=&b[((j*84/h)*112+(i*112/w))*3]; gfx_fill_rect(x+i,y+j,1,1,(s[0]<<16)|(s[1]<<8)|s[2]); } }
static int all_zero(const unsigned char*p,int n){ for(int i=0;i<n;i++) if(p[i]) return 0; return 1; }
static void scene(void){                      /* fenetre Reglages complete : rects, cercles, texte lisse, photos, melanges alpha */
  settings_model m={"mikea",1,0,1,3,"Degrade",880,1024,412,1024,2026,10,27,5,43};
  settings_layout L; settings_actions act; ui_ctx ui; memset(&ui,0,sizeof ui);
  theme_set_dark(0); gfx_fill_rect(0,0,1480,520,theme_desktop_bg());
  gfx_fill_rounded_rect_blend(32,32,704,432,10,theme_panel(),theme_panel_opacity());
  gfx_draw_text(40,40,"console 16px",0x112233,2); gfx_fill_circle(300,40,8,0xE0564F);
  settings_view_layout(&L,2,2,44,27); settings_view_paint_cards(&L); ui_begin(&ui,L.logout_btn.x+5,L.logout_btn.y+5,0); settings_view_paint(&ui,&L,&m,&act);
  gfx_scroll_up_region(100,16,0x202020);       /* defilement de console */
}
int main(void){
  static unsigned char ref[SZ];
  /* === 1. REFERENCE : mode direct (RAM jugee insuffisante) === */
  g_ram_top = 1<<20; graphics_init(); scene(); memcpy(ref,g_fb,SZ);
  CHECK(!all_zero(ref,SZ));                                   /* le mode direct dessine bien dans la memoire video */
  /* === 2. TAMPON ACTIF : la memoire video ne doit PAS bouger pendant le dessin === */
  g_ram_top = (unsigned long long)g_backmem + (64<<20); graphics_init(); scene();
  CHECK(all_zero(g_fb,SZ));                                   /* aucune primitive ne contourne le tampon */
  gfx_present();
  CHECK(memcmp(g_fb,ref,SZ)==0);                              /* apres present : identique octet par octet au mode direct */
  printf("OK 1-2 : tampon actif ne touche pas la video avant present ; rendu final == mode direct (%d octets)\n",SZ);
  /* === 3. present partiel : seules les lignes modifiees === */
  graphics_init(); CHECK(all_zero(g_fb,SZ));
  gfx_fill_rect(10,100,50,10,0xFF0000); gfx_present();
  CHECK(all_zero(g_fb,5920*100));                              /* lignes 0..99 intactes */
  CHECK(g_fb[100*5920+10*4+2]==0xFF);                          /* ligne 100 affichee */
  CHECK(all_zero(g_fb+110*5920,SZ-110*5920));                  /* lignes >= 110 intactes */
  printf("OK 3   : present partiel (lignes 100-109 seulement)\n");
  /* === 4. filet de securite gfx_tick === */
  graphics_init(); gfx_fill_rect(0,0,20,20,0x00FF00);
  gfx_tick(); CHECK(all_zero(g_fb,SZ));                        /* tick 1 : ca vient de dessiner -> on attend */
  gfx_fill_rect(0,30,20,20,0x00FF00); gfx_tick(); CHECK(all_zero(g_fb,SZ));   /* dessin entre 2 ticks : toujours rien */
  gfx_tick(); CHECK(!all_zero(g_fb,SZ));                       /* calme -> tout est affiche */
  CHECK(g_fb[0*5920+0+1]==0xFF && g_fb[35*5920+1]==0xFF);      /* les deux rectangles sont la */
  unsigned char keep=g_fb[7]; g_fb[7]^=0x55; gfx_tick(); gfx_tick(); CHECK(g_fb[7]!=keep);  /* deja affiche : le filet ne recopie pas pour rien */
  printf("OK 4   : filet de securite (attend le calme, affiche tout, ne recopie pas inutilement)\n");
  /* === 5. present explicite puis tick : pas de recopie === */
  graphics_init(); gfx_fill_rect(5,5,5,5,0xABCDEF); gfx_present(); g_fb[3]=0x77; gfx_tick(); gfx_tick(); CHECK(g_fb[3]==0x77);
  printf("OK 5   : apres present, le filet ne repasse pas\n");
  /* === 6. gfx_set_direct : affiche tout puis ecriture directe === */
  graphics_init(); gfx_fill_rect(0,200,30,30,0x123456); gfx_set_direct();
  CHECK(g_fb[200*5920+4]==0x56);                               /* contenu en attente affiche */
  gfx_fill_rect(0,300,30,30,0x654321); CHECK(g_fb[300*5920+4]==0x21);   /* puis dessin direct immediat */
  printf("OK 6   : gfx_set_direct (panique) affiche et passe en direct\n");
  /* === 7. repli : RAM insuffisante -> ecriture directe comme avant === */
  g_ram_top = 8<<20; graphics_init(); gfx_fill_rect(0,0,10,10,0xFFFFFF); CHECK(g_fb[0]==0xFF);
  g_ram_top = 0; graphics_init(); gfx_fill_rect(0,0,10,10,0x0000FF); CHECK(g_fb[0]==0xFF);  /* CMOS muet -> direct */
  printf("OK 7   : repli en ecriture directe (RAM insuffisante ou CMOS muet)\n");
  return 0; }
