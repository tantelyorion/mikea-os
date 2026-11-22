/* Test hote de gui/ui.c : logique de clic + rendu d une fenetre Reglages (clair/sombre) dans un faux
   framebuffer. Necessite le patch du pilote graphique pour l hote (voir le banc d essai decrit dans
   CORRECTIFS.md) ; sert de reference pour verifier un changement de gui/ui.c sans demarrer QEMU. */
#include <stdio.h>
#include <string.h>
#include "gui/ui.h"
#include "gui/theme.h"
void console_write(const char*t){(void)t;}
extern unsigned char* g_fb;
#define CHECK(c) do{ if(!(c)){printf("ECHEC: %s (ligne %d)\n",#c,__LINE__);return 1;} }while(0)

static void scene(ui_ctx* ui, int ox, int dark, int mx, int my, int down) {
  theme_set_dark(dark);
  gfx_fill_rect(ox,0,500,520,theme_desktop_bg());
  ui_begin(ui, mx, my, down);
  ui_rect win = { ox+20, 20, 460, 480 };
  ui_card(win);
  ui_rect a = ui_inset(win, 16);
  ui_label(ui_cut_top(&a, 34), "Réglages", GFX_FONT_TITLE, theme_text(), UI_ALIGN_LEFT);
  ui_separator(ui_cut_top(&a, 12));
  static int dm; dm = dark;
  ui_rect row = ui_cut_top(&a, UI_ROW_H);
  ui_label(ui_cut_left(&row, 300), "Mode sombre", GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
  ui_rect tg = { row.x + row.w - 44, row.y + (row.h-26)/2, 44, 26 };
  ui_toggle(ui, 10, tg, &dm);
  row = ui_cut_top(&a, UI_ROW_H);
  static int on = 1; ui_label(ui_cut_left(&row, 300), "Sons système", GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
  ui_rect tg2 = { row.x + row.w - 44, row.y + (row.h-26)/2, 44, 26 }; ui_toggle(ui, 11, tg2, &on);
  ui_cut_top(&a, 8);
  ui_label(ui_cut_top(&a, 24), "Fond d’écran", GFX_FONT_SMALL, theme_text(), UI_ALIGN_LEFT);
  ui_list_row(ui, 20, ui_cut_top(&a, 34), "Brume et collines", "N&B", 1);
  ui_list_row(ui, 21, ui_cut_top(&a, 34), "Nébuleuse d’Orion", "Photo", 0);
  ui_list_row(ui, 22, ui_cut_top(&a, 34), "Lune", "Photo", 0);
  ui_cut_top(&a, 10);
  ui_text_field(ui, 30, ui_cut_top(&a, 36), "Mikea", "Nom d’utilisateur", 1, 0);
  ui_cut_top(&a, 8);
  ui_text_field(ui, 31, ui_cut_top(&a, 36), "secret", "Mot de passe", 0, 1);
  ui_cut_top(&a, 12);
  ui_progress(ui_cut_top(&a, 8), 64);
  ui_cut_top(&a, 14);
  ui_rect btns = ui_cut_top(&a, UI_BTN_H);
  ui_rect b3 = ui_cut_right(&btns, 120); ui_cut_right(&btns, 8);
  ui_rect b2 = ui_cut_right(&btns, 110); ui_cut_right(&btns, 8);
  ui_rect b1 = ui_cut_right(&btns, 110);
  ui_button(ui, 40, b1, "Annuler", UI_BTN_NORMAL);
  ui_button(ui, 41, b2, "Supprimer", UI_BTN_DANGER);
  ui_button(ui, 42, b3, "Enregistrer", UI_BTN_PRIMARY);
}

int main(void){
  graphics_init();
  /* --- logique de clic --- */
  ui_ctx c; memset(&c,0,sizeof c); ui_rect r={10,10,100,30}; int clicks;
  clicks=0; ui_begin(&c,50,20,0); clicks+=ui_button(&c,1,r,"x",UI_BTN_NORMAL);       CHECK(clicks==0 && c.hot==1);
  ui_begin(&c,50,20,1); clicks+=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                 CHECK(clicks==0 && c.active==1);
  ui_begin(&c,50,20,1); clicks+=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                 CHECK(clicks==0);        /* maintenu : pas de clic */
  ui_begin(&c,50,20,0); clicks+=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                 CHECK(clicks==1);        /* relache dessus : clic */
  ui_begin(&c,50,20,0); clicks+=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                 CHECK(clicks==1);        /* pas de double clic */
  ui_begin(&c,50,20,1); ui_button(&c,1,r,"x",UI_BTN_NORMAL);                         /* appui... */
  ui_begin(&c,300,300,0); clicks=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                CHECK(clicks==0);        /* ...relache dehors : annule */
  ui_begin(&c,300,300,1); ui_button(&c,1,r,"x",UI_BTN_NORMAL);                       /* appui hors widget */
  ui_begin(&c,50,20,1);   clicks=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                CHECK(clicks==0 && c.active!=1); /* glisser dessus ne clique pas */
  ui_begin(&c,50,20,0);   clicks=ui_button(&c,1,r,"x",UI_BTN_NORMAL);                CHECK(clicks==0);
  { int v=0; ui_begin(&c,15,15,0); ui_rect t={10,10,44,26}; ui_toggle(&c,2,t,&v);
    ui_begin(&c,15,15,1); ui_toggle(&c,2,t,&v); ui_begin(&c,15,15,0); int ch=ui_toggle(&c,2,t,&v); CHECK(ch==1 && v==1); }
  { ui_rect q={0,0,100,100}; ui_rect t=ui_cut_top(&q,30); CHECK(t.h==30 && q.y==30 && q.h==70); ui_rect b=ui_cut_bottom(&q,200); CHECK(b.h==70 && q.h==0); }
  printf("OK : logique de clic, annulation, glisser, toggle, decoupe\n");
  /* --- rendu : clair (survol sur Enregistrer) / sombre (appui sur Supprimer) --- */
  ui_ctx u1,u2; memset(&u1,0,sizeof u1); memset(&u2,0,sizeof u2);
  scene(&u1, 0,   0, 0+20+460-16-120+60, 20+16+34+12+36*2+8+24+34*3+10+36+8+36+12+8+14+16, 0);
  scene(&u2, 500, 1, 500+20+460-16-120-8-110+50, 20+16+34+12+36*2+8+24+34*3+10+36+8+36+12+8+14+16, 0);
  FILE*f=fopen("ui.ppm","wb"); fprintf(f,"P6\n1000 520\n255\n");
  for(int y=0;y<520;y++)for(int x=0;x<1000;x++){unsigned char*p=g_fb+y*4000+x*4;fputc(p[2],f);fputc(p[1],f);fputc(p[0],f);} fclose(f); return 0; }
