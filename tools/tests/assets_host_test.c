/* Test hote du pack d assets (voir kernel/assets). Compile le VRAI assets.c et wallpaper_images.c
   contre une image assemblee comme le Makefile, et compare octet par octet.
   Usage :
     dd if=/dev/urandom of=/tmp/img.bin bs=512 count=1100; truncate -s 1M /tmp/img.bin
     dd if=assets/assets.pak of=/tmp/img.bin bs=512 seek=1100 conv=notrunc
     gcc -w -I. -Iinclude tools/tests/assets_host_test.c kernel/assets/assets.c gui/assets/wallpaper_images.c -o /tmp/at
     /tmp/at /tmp/img.bin .                                                                         */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/assets/assets.h"
#include "gui/assets/wallpaper_images.h"
typedef unsigned int u32; typedef unsigned char u8;
static const char* IMG; static int fail_reads=0, reads=0;
int installer_read_boot_sector(u32 lba, u8* buf){ reads++; if(fail_reads) return -1; FILE*f=fopen(IMG,"rb"); fseek(f,(long)lba*512,SEEK_SET); size_t n=fread(buf,1,512,f); fclose(f); return n==512?0:-1; }
void* mk_malloc(u32 n){return malloc(n);} void mk_free(void*p){free(p);}
void* mk_memcpy(void*d,const void*s,u32 n){return memcpy(d,s,n);}
int mk_strncmp(const char*a,const char*b,u32 n){return strncmp(a,b,n);}
static unsigned char* slurp(const char*p,long*n){FILE*f=fopen(p,"rb");fseek(f,0,SEEK_END);*n=ftell(f);rewind(f);unsigned char*b=malloc(*n);fread(b,1,*n,f);fclose(f);return b;}
#define CHECK(c) do{ if(!(c)){printf("ECHEC: %s (ligne %d)\n",#c,__LINE__);return 1;} }while(0)
int main(int argc,char**argv){ IMG=argv[1]; const char* R=argv[2]; char p[256]; long n;
  const char* wp[]={"brume","collines","cameleon","lune","nebuleuse","satellite"};
  for(u32 i=0;i<6;i++){ const wallpaper_photo* ph=wallpaper_photo_get(i); CHECK(ph); CHECK(ph->width==112&&ph->height==84);
    snprintf(p,256,"%s/assets/wallpapers/%s.rgb",R,wp[i]); unsigned char*ref=slurp(p,&n); CHECK(n==112*84*3); CHECK(memcmp(ref,ph->rgb_data,n)==0); free(ref);
    int before=reads; CHECK(wallpaper_photo_get(i)==ph); CHECK(reads==before); }   /* cache : aucune relecture */
  const char* sn[]={"startup","shutdown"}; const char* an[]={"snd_startup","snd_shutdown"};
  for(int i=0;i<2;i++){ u32 sz=0; u8*d=assets_load(an[i],&sz); CHECK(d); snprintf(p,256,"%s/assets/sounds/%s.pcm",R,sn[i]); unsigned char*ref=slurp(p,&n); CHECK((long)sz==n); CHECK(memcmp(ref,d,n)==0); free(ref); mk_free(d); }
  CHECK(!assets_find("inexistant",0)); CHECK(wallpaper_photo_get(99)==0);
  printf("OK : 6 fonds + 2 sons identiques octet par octet, cache valide\n"); return 0; }
