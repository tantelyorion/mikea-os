/* Test hote de l agent IA (kernel/ai/agent*.c). Compiler depuis la racine du depot :
     gcc -O1 -I. -Iinclude tools/tests/agent_host_test.c kernel/ai/agent.c kernel/ai/agent_builtin.c -o /tmp/t && /tmp/t
   Fuzzers : ajouter -fsanitize=address,undefined. Les chemins /tmp/ag/... du premier test sont a adapter. */
#include <stdio.h>
#include <string.h>
#include "kernel/ai/agent.h"
#include "kernel/ai/agent_builtin.h"

/* ---------- fausse plateforme ---------- */
static int dark=0, sound=1, logged_out=0; static u32 wp=0; static char opened[32];
typedef struct { char name[32]; char content[128]; int used, trashed; } F; static F files[16];
static F* find(const char*n){ for(int i=0;i<16;i++) if(files[i].used && !strcmp(files[i].name,n)) return &files[i]; return 0; }
static int p_open(const char*a){ strcpy(opened,a); return 1; }
static int p_gd(void){return dark;} static void p_sd(int d){dark=d;}
static int p_gs(void){return sound;} static void p_ss(int s){sound=s;}
static u32 p_wc(void){return 6;} static u32 p_gw(void){return wp;} static void p_sw(u32 i){wp=i;}
static void p_info(const char*w,char*o,u32 m){ snprintf(o,m,"info:%s",w); }
static u32 p_list(const char*d,char*o,u32 m){ (void)d; o[0]=0; u32 n=0; for(int i=0;i<16;i++) if(files[i].used&&!files[i].trashed){ if(n) strncat(o,", ",m-strlen(o)-1); strncat(o,files[i].name,m-strlen(o)-1); n++; } return n; }
static int p_read(const char*n,char*o,u32 m){ F*f=find(n); if(!f||f->trashed) return 0; snprintf(o,m,"%s",f->content); return 1; }
static int p_exists(const char*n){ F*f=find(n); return f!=0 && !f->trashed; }   /* comme le vrai file_exists : corbeille = absent */
static int p_create(const char*n,const char*c){ if(find(n)) return 0;   /* nom deja pris, meme par un fichier a la corbeille */ for(int i=0;i<16;i++) if(!files[i].used){ files[i].used=1; files[i].trashed=0; snprintf(files[i].name,32,"%s",n); snprintf(files[i].content,128,"%s",c); return 1; } return 0; }
static int p_trash(const char*n){ F*f=find(n); if(!f||f->trashed) return 0; f->trashed=1; return 1; }
static int p_restore(const char*n){ F*f=find(n); if(!f||!f->trashed) return 0; f->trashed=0; return 1; }
static void p_logout(void){ logged_out=1; }
static const agent_platform PLAT={p_open,p_gd,p_sd,p_gs,p_ss,p_wc,p_gw,p_sw,p_info,p_list,p_read,p_exists,p_create,p_trash,p_restore,p_logout};

static int fails=0, checks=0;
#define CK(c) do{ checks++; if(!(c)){ fails++; printf("  ECHEC ligne %d : %s\n",__LINE__,#c); } }while(0)

static const char* an(int a){ return a==AGENT_ACT_UNDO?"UNDO":a==AGENT_ACT_REPLY?"REPLY":a<0?"NONE":agent_action_at(a)->name; }
static void rule(const char* phrase,const char* act,const char* a1,const char* a2){
  agent_call c; int r=agent_plan_rules(phrase,&c);
  if(!act){ if(r){ fails++; printf("  ECHEC \"%s\" ne devrait RIEN deviner, a donne %s(%s)\n",phrase,an(c.action),c.arg1);} checks++; return; }
  checks++;
  if(!r || strcmp(an(c.action),act) || (a1&&strcmp(c.arg1,a1)) || (a2&&strcmp(c.arg2,a2))){ fails++;
    printf("  ECHEC \"%s\" -> %s(%s,%s) attendu %s(%s,%s)\n",phrase,r?an(c.action):"RIEN",c.arg1,c.arg2,act,a1?a1:"*",a2?a2:"*"); } }

int main(void){
  agent_builtin_install(&PLAT);
  printf("== 1. planificateur par regles (francais / anglais / accents / ponctuation)\n");
  rule("ouvre la calculatrice","open_app","calculatrice",0);
  rule("Ouvre la Calculatrice !","open_app","calculatrice",0);
  rule("lance les réglages","open_app","parametres",0);
  rule("j'aimerais ouvrir l'explorateur","open_app","fichiers",0);
  rule("open the terminal please","open_app","terminal",0);
  rule("corbeille","open_app","corbeille",0);
  rule("démarre valiha","open_app","valiha",0);
  rule("passe en mode sombre","set_theme","sombre",0);
  rule("Mets le thème clair","set_theme","clair",0);
  rule("dark mode","set_theme","sombre",0);
  rule("change le thème","set_theme","basculer",0);
  rule("coupe le son","set_sound","off",0);
  rule("mute","set_sound","off",0);
  rule("active le son","set_sound","on",0);
  rule("remets le son","set_sound","on",0);
  rule("désactive le son","set_sound","off",0);
  rule("fond d'écran 3","set_wallpaper","3",0);
  rule("change le fond d’écran","set_wallpaper","suivant",0);
  rule("quelle mémoire reste-t-il","system_info","memoire",0);
  rule("combien d'espace disque","system_info","disque",0);
  rule("quelle heure est-il","system_info","heure",0);
  rule("infos système","system_info","tout",0);
  rule("liste les fichiers","list_files","",0);
  rule("montre les fichiers du dossier docs","list_files","docs",0);
  rule("crée un fichier notes.txt","create_file","notes.txt","");
  rule("crée un fichier notes.txt avec le texte bonjour le monde","create_file","notes.txt","bonjour le monde");
  rule("nouveau fichier \"todo.md\" contenant acheter du pain","create_file","todo.md","acheter du pain");
  rule("create a file named report.txt","create_file","report.txt",0);
  rule("fais une note","create_file","note.txt",0);
  rule("supprime notes.txt","delete_file","notes.txt",0);
  rule("efface le fichier vieux.log","delete_file","vieux.log",0);
  rule("restaure notes.txt","restore_file","notes.txt",0);
  rule("lis le fichier notes.txt","read_file","notes.txt",0);
  rule("affiche notes.txt","read_file","notes.txt",0);
  rule("annule","UNDO",0,0);
  rule("annule la dernière action","UNDO",0,0);
  rule("aide","REPLY",0,0);
  rule("que sais-tu faire ?","REPLY",0,0);
  rule("déconnecte-moi","logout",0,0);
  rule("quelle est la capitale de la France",0,0,0);
  rule("raconte-moi une blague",0,0,0);
  rule("c'est clair merci",0,0,0);
  rule("",0,0,0);
  rule("   ",0,0,0);
  rule("supprime",0,0,0);            /* pas de nom : ne devine pas */
  rule("supprime ../../etc/passwd",0,0,0);   /* nom invalide : jamais transmis */

  printf("== 2. lecture du JSON d'un modele (tolerante, mais stricte sur l'action)\n");
  agent_call c;
  CK(agent_parse_json("{\"action\":\"open_app\",\"arg1\":\"calculatrice\",\"arg2\":\"\",\"say\":\"ok\"}",&c) && !strcmp(an(c.action),"open_app") && !strcmp(c.arg1,"calculatrice") && !strcmp(c.say,"ok") && c.source==AGENT_SRC_LLM);
  CK(agent_parse_json("Voici : ```json\n{ \"action\" : \"set_theme\" , \"arg1\" : \"sombre\" }\n``` fin",&c) && !strcmp(an(c.action),"set_theme"));
  CK(agent_parse_json("{\"ACTION\":\"Set_Theme\",\"Arg1\":\"clair\"}",&c) && !strcmp(an(c.action),"set_theme"));
  CK(agent_parse_json("{\"action\":\"set_wallpaper\",\"args\":{\"arg1\":3}}",&c) && !strcmp(c.arg1,"3"));
  CK(agent_parse_json("{\"action\":\"create_file\",\"filename\":\"a.txt\",\"content\":\"ligne1\\nligne2 \\\"cite\\\"\"}",&c) && !strcmp(c.arg1,"a.txt") && !strcmp(c.arg2,"ligne1\nligne2 \"cite\""));
  CK(agent_parse_json("{\"action\":\"reply\",\"say\":\"Paris.\"}",&c) && c.action==AGENT_ACT_REPLY && !strcmp(c.say,"Paris."));
  CK(agent_parse_json("{\"action\":\"undo\"}",&c) && c.action==AGENT_ACT_UNDO);
  CK(!agent_parse_json("{\"action\":\"reply\",\"say\":\"\"}",&c));                 /* reponse vide */
  CK(!agent_parse_json("{\"action\":\"rm -rf /\",\"arg1\":\"/\"}",&c));             /* action inventee */
  CK(!agent_parse_json("{\"action\":\"shell\",\"arg1\":\"format c:\"}",&c));
  CK(!agent_parse_json("{\"action\":\"shutdown\"}",&c));                            /* pas dans le registre */
  CK(!agent_parse_json("{\"arg1\":\"calculatrice\"}",&c));                          /* pas d'action */
  CK(!agent_parse_json("pas de json du tout",&c));
  CK(!agent_parse_json("",&c));
  CK(!agent_parse_json("{\"action\":",&c));                                         /* tronque */
  CK(!agent_parse_json("{\"action\":\"open_app",&c) || c.action>=0);                /* tronque en plein mot : ne plante pas */
  { char big[3000]; memset(big,'a',2999); big[2999]=0; char js[3200]; snprintf(js,3200,"{\"action\":\"open_app\",\"arg1\":\"%s\"}",big); CK(agent_parse_json(js,&c) && strlen(c.arg1)<64); }  /* debordement tronque */

  printf("== 3. garde-fous : permissions, arguments pieges, confirmation obligatoire\n");
  char reason[96], out[256]; agent_call k;
  #define MK(a,x,y) do{ memset(&k,0,sizeof k); k.action=agent_find(a); strcpy(k.arg1,x); strcpy(k.arg2,y); k.source=AGENT_SRC_LLM; }while(0)
  MK("open_app","calculatrice",""); CK(agent_decide(&k,reason,96)==AGENT_DECISION_RUN);
  MK("system_info","memoire","");   CK(agent_decide(&k,reason,96)==AGENT_DECISION_RUN);
  MK("create_file","a.txt","x");    CK(agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM);
  MK("delete_file","a.txt","");     CK(agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM_STRONG);
  MK("logout","","");               CK(agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM_STRONG);
  const char* bad[]={"../etc/passwd","a/b","/root","..","...",".cache","a..b","dossier\\x","nom avec espace","a;rm -rf","a|b","$(id)","`id`","nom\nbis","", "0123456789012345678901234567890123456789"};
  for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){ MK("create_file",bad[i],""); CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT); MK("delete_file",bad[i],""); CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT); MK("read_file",bad[i],""); CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT); }
  MK("create_file","ok.txt","avec\x01controle"); CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("create_file","ok.txt","retour\nligne");   CK(agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM);   /* \n permis dans le contenu */
  MK("set_wallpaper","0","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("set_wallpaper","7","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("set_wallpaper","-1","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("set_wallpaper","6","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_RUN);
  MK("set_theme","rouge","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("open_app","../../bin/sh","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  MK("open_app","installer","");  CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);        /* l'installeur n'est PAS ouvrable par l'agent */
  memset(&k,0,sizeof k); k.action=999; CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  memset(&k,0,sizeof k); k.action=AGENT_ACT_NONE; CK(agent_decide(&k,reason,96)==AGENT_DECISION_REJECT);
  /* execution : une action sensible sans confirmation est REFUSEE, et ne fait rien */
  MK("create_file","secret.txt","x"); CK(!agent_execute(&k,0,out,256) && !find("secret.txt"));
  CK(agent_execute(&k,1,out,256) && find("secret.txt"));
  MK("delete_file","secret.txt",""); CK(!agent_execute(&k,0,out,256) && !find("secret.txt")->trashed);      /* pas confirme : intact */
  CK(agent_execute(&k,1,out,256) && find("secret.txt")->trashed);
  MK("logout","",""); logged_out=0; CK(!agent_execute(&k,0,out,256) && !logged_out); CK(agent_execute(&k,1,out,256) && logged_out);
  MK("create_file","secret.txt","y"); CK(!agent_execute(&k,1,out,256));          /* existe deja (meme a la corbeille) : pas d'ecrasement */
  MK("delete_file","absent.txt",""); CK(!agent_execute(&k,1,out,256));
  MK("create_file","../evil","x"); CK(!agent_execute(&k,1,out,256) && !find("../evil"));       /* validation re-faite a l'execution */

  printf("== 4. annulation (pile), journal\n");
  while(agent_undo_available()) agent_undo_last(out,256);
  dark=0; sound=1; wp=0; memset(files,0,sizeof files);
  MK("set_theme","sombre","");   CK(agent_execute(&k,0,out,256) && dark==1);
  MK("set_sound","off","");      CK(agent_execute(&k,0,out,256) && sound==0);
  MK("set_wallpaper","4","");    CK(agent_execute(&k,0,out,256) && wp==3);
  MK("create_file","n.txt","hi");CK(agent_execute(&k,1,out,256) && find("n.txt") && !find("n.txt")->trashed);
  CK(agent_undo_last(out,256) && find("n.txt")->trashed);     /* LIFO : la creation d'abord */
  CK(agent_undo_last(out,256) && wp==0);
  CK(agent_undo_last(out,256) && sound==1);
  CK(agent_undo_last(out,256) && dark==0);
  CK(!agent_undo_available() && !agent_undo_last(out,256));    /* pile vide : message, pas de plantage */
  MK("create_file","d.txt","");  agent_execute(&k,1,out,256);
  MK("delete_file","d.txt","");  agent_execute(&k,1,out,256); CK(find("d.txt")->trashed);
  CK(agent_undo_last(out,256) && !find("d.txt")->trashed);     /* supprimer -> annuler = restaure */
  { /* journal : plus recente en premier, marque "annule" */ const agent_log_entry*e=agent_log_at(0); CK(e && e->action==agent_find("delete_file") && e->undone==1); }
  CK(agent_log_at(agent_log_count())==0);
  for(int i=0;i<40;i++){ MK("system_info","tout",""); agent_execute(&k,0,out,256); }
  CK(agent_log_count()==16 && agent_log_at(0)->seq > agent_log_at(15)->seq);   /* anneau : borne a 16 */
  for(int i=0;i<30;i++){ MK("set_theme","basculer",""); agent_execute(&k,0,out,256); }
  { int n=0; while(agent_undo_available()){ agent_undo_last(out,256); n++; } CK(n==8); }   /* pile bornee a 8 */

  printf("== 5. chaine complete : phrase -> regles -> garde-fou -> execution -> annulation\n");
  memset(files,0,sizeof files);
  CK(agent_plan_rules("crée un fichier notes.txt avec le texte bonjour",&k));
  CK(agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM);
  agent_describe(&k,out,256); CK(!strcmp(out,"Créer le fichier « notes.txt » ?"));
  CK(agent_execute(&k,1,out,256) && !strcmp(find("notes.txt")->content,"bonjour"));
  CK(agent_plan_rules("supprime notes.txt",&k) && agent_decide(&k,reason,96)==AGENT_DECISION_CONFIRM_STRONG && agent_execute(&k,1,out,256) && find("notes.txt")->trashed);
  CK(agent_plan_rules("annule",&k) && agent_execute(&k,0,out,256) && !find("notes.txt")->trashed);
  CK(agent_plan_rules("lis le fichier notes.txt",&k) && agent_execute(&k,0,out,256) && strstr(out,"bonjour"));
  CK(agent_plan_rules("ouvre les réglages",&k) && agent_execute(&k,0,out,256) && !strcmp(opened,"parametres"));
  agent_call r2; CK(agent_parse_json("{\"action\":\"reply\",\"say\":\"Bonjour !\"}",&r2) && agent_execute(&r2,0,out,256) && !strcmp(out,"Bonjour !"));

  printf("== 6. consignes et schema pour le modele\n");
  char prompt[2400], schema[600]; agent_build_system_prompt(prompt,sizeof prompt); agent_build_schema(schema,sizeof schema);
  CK(strlen(prompt)<2000); CK(strstr(prompt,"open_app") && strstr(prompt,"delete_file") && strstr(prompt,"reply"));
  CK(strstr(schema,"\"enum\":[\"open_app\"") && strstr(schema,"\"reply\"]"));
  for(u32 i=0;i<agent_action_count();i++) CK(strstr(schema,agent_action_at(i)->name));
  FILE*f=fopen("/tmp/ag/schema.json","w"); fputs(schema,f); fclose(f); f=fopen("/tmp/ag/prompt.txt","w"); fputs(prompt,f); fclose(f);
  printf("   prompt : %zu octets, schema : %zu octets\n",strlen(prompt),strlen(schema));

  printf("\n%d verifications, %d echec(s)\n",checks,fails);
  return fails?1:0; }
