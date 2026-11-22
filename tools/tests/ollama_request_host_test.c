/* Test hote de la REQUETE HTTP vers Ollama : compile le vrai kernel/ai/jarvis.c avec un faux reseau, capture la requete
   et la reponse simulee. Verifier ensuite avec Python que le corps est du JSON valide (voir CORRECTIFS.md). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/ai/jarvis.h"
#include "kernel/ai/agent.h"
#include "kernel/ai/agent_builtin.h"
/* --- stubs du noyau --- */
unsigned int net_gateway=0x0A000202;
void console_write(const char*t){(void)t;} void execute_command(char*c){(void)c;}
char* mk_strcpy(char*d,const char*s,unsigned m){ strncpy(d,s,m-1); d[m-1]=0; return d; }
int mk_strlen(const char*s){return (int)strlen(s);} int mk_strncmp(const char*a,const char*b,unsigned n){return strncmp(a,b,n);}
int net_parse_ip(const char*s,unsigned*o){(void)s;(void)o;return 0;} int net_ping(unsigned ip,int n){(void)ip;(void)n;return 0;}
int serial_getc_timeout(int t){(void)t;return -1;} void serial_write(const char*s){(void)s;}
int net_is_up(void){ return 1; }
static char captured[8192]; static int cap_len; static const char* canned; static int pos;
int tcp_connect(unsigned ip,unsigned short port){ (void)ip; return port==11434?0:-1; }
int tcp_write(const unsigned char*d,unsigned n){ memcpy(captured,d,n); captured[n]=0; cap_len=n; return 0; }
int tcp_read(unsigned char*b,unsigned max,int to){ (void)to; int left=(int)strlen(canned)-pos; if(left<=0) return 0; int k=left<(int)max?left:(int)max; memcpy(b,canned+pos,k); pos+=k; return k; }
void tcp_close(void){}
/* plateforme minimale pour construire le registre */
static int z(void){return 0;}
static const agent_platform P={0};
int main(int argc,char**argv){
  agent_builtin_install(&P);
  static char system[2200], schema[700], answer[420];
  agent_build_system_prompt(system,sizeof system); agent_build_schema(schema,sizeof schema);
  /* reponse simulee d'Ollama : champ "response" contenant du JSON echappe, comme en vrai */
  canned="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"model\":\"llama3.2:3b\",\"created_at\":\"2026-10-30T10:00:00Z\",\"response\":\"{\\\"action\\\":\\\"open_app\\\",\\\"arg1\\\":\\\"calculatrice\\\",\\\"arg2\\\":\\\"\\\",\\\"say\\\":\\\"J'ouvre la calculatrice.\\\"}\",\"done\":true}";
  pos=0; answer[0]=0;
  int ok=jarvis_llm_generate(system,"ouvre la calculatrice \"vite\" \\ test",schema,answer,sizeof answer);
  printf("appel=%d reponse=[%s]\n",ok,answer);
  agent_call c; int parsed=agent_parse_json(answer,&c); printf("parse=%d action=%d arg1=[%s] say=[%s]\n",parsed,c.action,c.arg1,c.say);
  FILE*f=fopen("req.txt","wb"); fwrite(captured,1,cap_len,f); fclose(f); (void)z; (void)argc;(void)argv; return !(ok&&parsed); }
