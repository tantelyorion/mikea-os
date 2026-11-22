/* Test hote de l agent IA (kernel/ai/agent*.c). Compiler depuis la racine du depot :
     gcc -O1 -I. -Iinclude tools/tests/agent_fuzz_random_host_test.c kernel/ai/agent.c kernel/ai/agent_builtin.c -o /tmp/t && /tmp/t
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
static unsigned long long rs=88172645463325252ULL; static unsigned rnd(void){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (unsigned)(rs>>11); }
int main(void){ agent_builtin_install(&P);
  const char* words[]={"ouvre","la","calculatrice","supprime","crée","fichier","notes.txt","avec","le","texte","mode","sombre","annule","fond","3","{","}","\"","\\","\"action\":","\"arg1\":","open_app","delete_file","reply","say","-","é","è","ç","'","’","«","»","\n","\t","..","/","\\u00e9","\\u","null","true","[","]",":",","};
  char buf[4000]; char out[300]; long n=0, planned=0, parsed=0;
  for(int it=0; it<300000; it++){
    u32 len = rnd()%40; buf[0]=0; u32 pos=0;
    for(u32 w=0; w<len && pos<3900; w++){
      if(rnd()%6==0){ int nb=rnd()%8; for(int b=0;b<nb&&pos<3990;b++) buf[pos++]=(char)(rnd()%255+1); }   /* octets quelconques (UTF-8 invalide) */
      else { const char* s=words[rnd()%(sizeof words/sizeof*words)]; while(*s&&pos<3990) buf[pos++]=*s++; if(rnd()%4) buf[pos++]=' '; } }
    if(it%5000==0){ for(int k=0;k<3900;k++) buf[k]='a'+rnd()%26; pos=3900; }                              /* texte tres long */
    buf[pos]=0;
    agent_call c; memset(&c,0xAB,sizeof c);
    if(agent_plan_rules(buf,&c)){ planned++;
       if(strlen(c.arg1)>=sizeof c.arg1 || strlen(c.arg2)>=sizeof c.arg2) { printf("DEBORDEMENT arg\n"); return 1; }
       char reason[96]; agent_decision d=agent_decide(&c,reason,96); if(d==AGENT_DECISION_RUN||d==AGENT_DECISION_CONFIRM||d==AGENT_DECISION_CONFIRM_STRONG) agent_execute(&c,0,out,300); }
    memset(&c,0xAB,sizeof c);
    if(agent_parse_json(buf,&c)){ parsed++; if(strlen(c.arg1)>=sizeof c.arg1||strlen(c.say)>=sizeof c.say) { printf("DEBORDEMENT json\n"); return 1; } agent_execute(&c,1,out,300); }
    n++; }
  printf("fuzz : %ld entrees, %ld planifiees, %ld JSON valides, aucun plantage ni debordement\n",n,planned,parsed); return 0; }
