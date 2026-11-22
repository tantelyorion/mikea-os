/* Test hote de apps/mikea_bar/mikea_bar_view.c (geometrie, clics par etat, idempotence, rendu des 4 etats). Meme banc
   d essai graphique que settings_view_host_test.c (faux framebuffer, voir CORRECTIFS.md). Chemins a adapter. */
#include <stdio.h>
#include <string.h>
#include "gui/ui.h"
#include "gui/theme.h"
#include "apps/mikea_bar/mikea_bar_view.h"
void console_write(const char*t){(void)t;}
extern unsigned char* g_fb; extern unsigned long long g_ram_top;
#define W 1400
#define H 720
#define SZ (5600*720)
#define CHECK(c) do{ if(!(c)){printf("ECHEC: %s (ligne %d)\n",#c,__LINE__);return 1;} }while(0)
static unsigned long hash(void){ unsigned long h=1469598103934665603UL; for(int i=0;i<SZ;i++){h^=g_fb[i]; h*=1099511628211UL;} return h; }
static void base_model(bar_model* m){ memset(m,0,sizeof *m); m->model_name="llama3.2:3b"; m->llm_online=1; m->message_ok=1;
  m->suggestion[0]="Ouvre la calculatrice"; m->suggestion_kind[0]="Application"; m->suggestion[1]="Passe en mode sombre"; m->suggestion_kind[1]="Réglage";
  m->suggestion[2]="Infos système"; m->suggestion_kind[2]="Info"; m->suggestion[3]="Liste mes fichiers"; m->suggestion_kind[3]="Fichiers"; m->suggestion[4]="Aide"; m->suggestion_kind[4]="Aide"; }
static void scene(int ox,int oy,int dark,bar_model* m,int mx,int my){   /* fond + panneau, dans un quadrant 700x360 */
  theme_set_dark(dark); gfx_fill_rect(ox,oy,700,360,theme_desktop_bg());
  bar_layout L; bar_view_layout(&L,700,360*8/1);   /* hauteur d'ecran fictive : on recale le panneau en haut du quadrant */
  L.panel.x+=ox; L.field.x+=ox; L.body.x+=ox; L.message.x+=ox; L.btn_confirm.x+=ox; L.btn_cancel.x+=ox; L.btn_undo.x+=ox; L.footer_hint.x+=ox; L.footer_status.x+=ox; for(int i=0;i<5;i++) L.suggestion[i].x+=ox;
  int dy=oy+12-L.panel.y; L.panel.y+=dy; L.field.y+=dy; L.body.y+=dy; L.message.y+=dy; L.btn_confirm.y+=dy; L.btn_cancel.y+=dy; L.btn_undo.y+=dy; L.footer_hint.y+=dy; L.footer_status.y+=dy; for(int i=0;i<5;i++) L.suggestion[i].y+=dy;
  ui_ctx ui; memset(&ui,0,sizeof ui); bar_actions a; bar_view_paint_frame(&L); ui_begin(&ui,mx,my,0); bar_view_paint(&ui,&L,m,&a); }
int main(void){
  g_ram_top=1<<20; graphics_init();
  /* --- geometrie a 640x480 et 1280x800 : tout dans le panneau, panneau dans l'ecran --- */
  int scr[][2]={{640,480},{800,600},{1280,800},{1920,1080}};
  for(int k=0;k<4;k++){ bar_layout L; bar_view_layout(&L,scr[k][0],scr[k][1]);
    CHECK(L.panel.x>=0 && L.panel.y>=0 && L.panel.x+L.panel.w<=scr[k][0] && L.panel.y+L.panel.h<=scr[k][1]);
    ui_rect* all[]={&L.field,&L.message,&L.btn_confirm,&L.btn_cancel,&L.btn_undo,&L.footer_hint,&L.footer_status,&L.suggestion[0],&L.suggestion[4]};
    for(int i=0;i<9;i++){ ui_rect r=*all[i]; CHECK(r.w>0&&r.h>0&&r.x>=L.panel.x&&r.y>=L.panel.y&&r.x+r.w<=L.panel.x+L.panel.w&&r.y+r.h<=L.panel.y+L.panel.h); } }
  /* --- logique : clics dans chaque etat --- */
  bar_layout L; bar_view_layout(&L,1280,800); bar_model m; bar_actions a; ui_ctx ui;
  base_model(&m); m.state=BAR_IDLE;
  for(int i=0;i<5;i++){ memset(&ui,0,sizeof ui); int cx=L.suggestion[i].x+30, cy=L.suggestion[i].y+10;
    CHECK(bar_view_hit(&L,&m,cx,cy)==BAR_ID_SUGGESTION0+i);
    ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,1); bar_view_paint(&ui,&L,&m,&a); CHECK(a.suggestion==-1);
    ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); CHECK(a.suggestion==i && !a.confirm && !a.cancel && !a.undo); }
  base_model(&m); m.state=BAR_CONFIRM; strcpy(m.message,"Mettre « notes.txt » à la corbeille ?"); m.confirm_strong=1;
  { int cx=L.btn_confirm.x+10, cy=L.btn_confirm.y+10; memset(&ui,0,sizeof ui); CHECK(bar_view_hit(&L,&m,cx,cy)==BAR_ID_CONFIRM);
    ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,1); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); CHECK(a.confirm==1 && a.cancel==0);
    cx=L.btn_cancel.x+10; cy=L.btn_cancel.y+10; memset(&ui,0,sizeof ui); ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,1); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); CHECK(a.cancel==1 && a.confirm==0);
    /* en CONFIRM, les suggestions ne sont PAS cliquables (elles sont cachees) */
    CHECK(bar_view_hit(&L,&m,L.suggestion[1].x+30,L.suggestion[1].y+10)==0); }
  base_model(&m); m.state=BAR_RESULT; strcpy(m.message,"Mode sombre activé."); m.can_undo=1;
  { int cx=L.btn_undo.x+20, cy=L.btn_undo.y+10; memset(&ui,0,sizeof ui); CHECK(bar_view_hit(&L,&m,cx,cy)==BAR_ID_UNDO);
    ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,1); bar_view_paint(&ui,&L,&m,&a); ui_begin(&ui,cx,cy,0); bar_view_paint(&ui,&L,&m,&a); CHECK(a.undo==1);
    m.can_undo=0; CHECK(bar_view_hit(&L,&m,cx,cy)==0); }                       /* pas d'annulation possible : pas de bouton */
  /* --- idempotence des repeints partiels (survol reversible) --- */
  base_model(&m); m.state=BAR_IDLE; strcpy(m.input,"ouvre la calc");
  bar_view_paint_frame(&L); memset(&ui,0,sizeof ui); int hx=L.suggestion[2].x+30, hy=L.suggestion[2].y+10;
  ui_begin(&ui,hx,hy,0); bar_view_paint(&ui,&L,&m,&a); unsigned long h1=hash();
  for(int i=0;i<5;i++){ ui_begin(&ui,hx,hy,0); bar_view_paint(&ui,&L,&m,&a); } CHECK(hash()==h1);
  ui_begin(&ui,L.suggestion[4].x+30,L.suggestion[4].y+10,0); bar_view_paint(&ui,&L,&m,&a); CHECK(hash()!=h1);
  ui_begin(&ui,hx,hy,0); bar_view_paint(&ui,&L,&m,&a); CHECK(hash()==h1);
  /* texte tres long : ne plante pas, reste dans le panneau (le champ affiche la fin) */
  memset(m.input,'w',95); m.input[95]=0; ui_begin(&ui,0,0,0); bar_view_paint(&ui,&L,&m,&a);
  base_model(&m); m.state=BAR_RESULT; { char big[320]; memset(big,'x',319); for(int i=7;i<319;i+=8) big[i]=' '; big[319]=0; strcpy(m.message,big); ui_begin(&ui,0,0,0); bar_view_paint(&ui,&L,&m,&a); }
  printf("OK : geometrie 4 resolutions, clics par etat (suggestions/valider/annuler/annuler-action), idempotence, textes extremes\n");
  /* --- rendu : 4 etats --- */
  g_ram_top=1<<20; graphics_init(); gfx_fill_rect(0,0,1400,720,0x808080);
  base_model(&m); m.state=BAR_IDLE; strcpy(m.input,"ouvre la calc"); m.can_undo=1; scene(0,0,0,&m,300,130);
  strcpy(m.recent[0],"set_theme sombre"); base_model(&m); m.state=BAR_RESULT; m.message_ok=1; m.from_llm=0; m.can_undo=1; strcpy(m.message,"Mode sombre activé."); strcpy(m.input,"passe en mode sombre"); scene(700,0,0,&m,0,0);
  base_model(&m); m.state=BAR_CONFIRM; m.confirm_strong=1; strcpy(m.message,"Mettre « notes.txt » à la corbeille ?"); strcpy(m.input,"supprime notes.txt"); m.llm_online=0; scene(0,360,1,&m,0,0);
  base_model(&m); m.state=BAR_RESULT; m.from_llm=1; m.message_ok=1; strcpy(m.message,"Paris est la capitale de la France. Elle compte environ deux millions d’habitants et abrite la tour Eiffel, le Louvre et Notre-Dame."); strcpy(m.input,"quelle est la capitale de la France"); scene(700,360,1,&m,0,0);
  FILE*f=fopen("mb.ppm","wb"); fprintf(f,"P6\n%d %d\n255\n",W,H); for(int y=0;y<H;y++)for(int x=0;x<W;x++){unsigned char*p=g_fb+y*5600+x*4;fputc(p[2],f);fputc(p[1],f);fputc(p[0],f);} fclose(f); return 0; }
