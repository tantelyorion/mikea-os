#include "string.h"



u32 mk_strlen(const char* s)
{

u32 len = 0;

while (s[len] != 0)
{
len++;
}

return len;

}



int mk_strcmp(const char* a, const char* b)
{

u32 i = 0;

while (a[i] != 0 && b[i] != 0)
{

if (a[i] != b[i])
{
return (u8)a[i] - (u8)b[i];
}

i++;

}

return (u8)a[i] - (u8)b[i];

}



int mk_strncmp(const char* a, const char* b, u32 n)
{

u32 i = 0;

while (i < n && a[i] != 0 && b[i] != 0)
{

if (a[i] != b[i])
{
return (u8)a[i] - (u8)b[i];
}

i++;

}

if (i == n)
{
return 0;
}

return (u8)a[i] - (u8)b[i];

}



char* mk_strcpy(char* dest, const char* src, u32 max_len)
{

/*
    Copie au plus max_len-1 caracteres et termine toujours
    par un octet nul (contrairement a strcpy standard, qui
    ne protege pas contre un depassement du buffer dest).
*/

if (max_len == 0)
{
return dest;
}

u32 i = 0;

while (src[i] != 0 && i < max_len - 1)
{
dest[i] = src[i];
i++;
}

dest[i] = 0;

return dest;

}



void* mk_memset(void* dest, int value, u32 len)
{

u8* p = (u8*)dest;

u32 i = 0;

while (i < len)
{
p[i] = (u8)value;
i++;
}

return dest;

}



void* mk_memcpy(void* dest, const void* src, u32 len)
{

u8* d = (u8*)dest;

const u8* s = (const u8*)src;

u32 i = 0;

while (i < len)
{
d[i] = s[i];
i++;
}

return dest;

}



int mk_memcmp(const void* a, const void* b, u32 len)
{

const u8* pa = (const u8*)a;

const u8* pb = (const u8*)b;

u32 i = 0;

while (i < len)
{

if (pa[i] != pb[i])
{
return pa[i] - pb[i];
}

i++;

}

return 0;

}
