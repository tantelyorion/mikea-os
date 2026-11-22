/* Test hote de l agent IA (kernel/ai/agent*.c). Compiler depuis la racine du depot :
     gcc -O1 -I. -Iinclude tools/tests/agent_fuzz_json_host_test.c kernel/ai/agent.c kernel/ai/agent_builtin.c -o /tmp/t && /tmp/t
   Fuzzers : ajouter -fsanitize=address,undefined. Les chemins /tmp/ag/... du premier test sont a adapter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/ai/agent.h"
#include "kernel/ai/agent_builtin.h"
static int dark,snd=1; static u32 wp;
static int o(const char*a){(void)a;return 1;} static int gd(void){return dark;} static void sd(int d){dark=d;} static int gs(void){return snd;} static void ss(int s){snd=s;}
static u32 wc(void){return 6;} static u32 gw(void){return wp;} static void sw(u32 i){wp=i;} static void si(const char*w,char*out,u32 m){(void)w;out[0]=0;(void)m;}
static u32 lf(const char*d,char*out,u32 m){(void)d;(void)m;out[0]=0;return 0;} static int rf(const char*n,char*out,u32 m){(void)n;(void)m;out[0]=0;return 1;}
static int ex(const char*n){(void)n;return 0;} static int cf(const char*n,const char*c){(void)n;(void)c;return 1;} static int tf(const char*n){(void)n;return 1;} static void lo(void){}
static const agent_platform P={o,gd,sd,gs,ss,wc,gw,sw,si,lf,rf,ex,cf,tf,tf,lo};
static unsigned long long rs=1234567ULL; static unsigned rnd(void){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (unsigned)(rs>>11); }
int main(void){ agent_builtin_install(&P);
  const char* seeds[]={ "{\"action\":\"open_app\",\"arg1\":\"calculatrice\",\"arg2\":\"\",\"say\":\"ok\"}",
    "{\"action\":\"create_file\",\"arg1\":\"a.txt\",\"arg2\":\"bonjour\\nmonde\",\"say\":\"fait\"}",
    "Voila:\n```json\n{\"action\":\"set_wallpaper\",\"args\":{\"arg1\":3}}\n```", "{\"action\":\"reply\",\"say\":\"Paris \\u00e9\\u0041.\"}",
    "{\"action\":\"delete_file\",\"filename\":\"x.log\"}", "{\"action\":\"undo\"}" };
  char buf[600], out[300]; long valid=0, n=0, executed=0;
  for(int it=0; it<400000; it++){
    const char* s=seeds[rnd()%6]; u32 L=strlen(s); memcpy(buf,s,L+1);
    int muts=rnd()%5; for(int m=0;m<muts;m++){ u32 p=rnd()%(L?L:1); switch(rnd()%4){ case 0: buf[p]=(char)(rnd()%255+1); break; case 1: buf[p]=0; L=p; break;
       case 2: if(L<590){ memmove(buf+p+1,buf+p,L-p+1); buf[p]="{}\":,\\\\u"[rnd()%8]; L++; } break; default: if(L>0){ memmove(buf+p,buf+p+1,L-p); L--; } } }
    agent_call c; memset(&c,0xAB,sizeof c);
    if(agent_parse_json(buf,&c)){ valid++;
      if(strlen(c.arg1)>=sizeof c.arg1||strlen(c.arg2)>=sizeof c.arg2||strlen(c.say)>=sizeof c.say){ printf("DEBORDEMENT\n"); return 1; }
      if(c.action>=0 && (u32)c.action>=agent_action_count()){ printf("INDEX HORS REGISTRE\n"); return 1; }
      char r[96]; if(agent_decide(&c,r,96)!=AGENT_DECISION_REJECT){ executed++; agent_execute(&c,rnd()%2,out,300); } }
    n++; }
  printf("fuzz JSON par mutation : %ld entrees, %ld JSON acceptes, %ld executes via garde-fous, 0 plantage, 0 index invalide\n",n,valid,executed); return 0; }
