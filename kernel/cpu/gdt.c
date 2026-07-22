#include "gdt.h"


struct gdt_entry
{

unsigned short limit_low;

unsigned short base_low;

unsigned char base_middle;

unsigned char access;

unsigned char granularity;

unsigned char base_high;

};



static struct gdt_entry gdt[3];



void gdt_init()
{


/*
Entry 0 :
NULL descriptor
*/


gdt[0].limit_low=0;



/*
Code Segment
*/


gdt[1].access=0x9A;

gdt[1].granularity=0xCF;



/*
Data Segment
*/


gdt[2].access=0x92;

gdt[2].granularity=0xCF;



}