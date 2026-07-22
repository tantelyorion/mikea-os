/*
====================================================

            Mikea OS Kernel

            Version : 0.2.1 ORIGIN


            File:
            kernel.c


            Developer:
            Tantely Orion


            Architecture:
            x86_64


====================================================
*/


#include "kernel.h"

#include "../include/types.h"



/*
====================================================
DISPLAY SYSTEM
====================================================
*/


void fb_clear();


void console_init();


void console_write(
    const char* text
);




/*
====================================================
MEMORY SYSTEM
====================================================
*/


void heap_init();


void* mk_malloc(
    u32 size
);




/*
====================================================
CPU SYSTEM
====================================================
*/


void cpu_init();


void gdt_init();




/*
====================================================
INTERRUPT SYSTEM
====================================================
*/


void idt_init();


void irq_init();


void irq_enable();




/*
====================================================
HARDWARE DRIVER SYSTEM
====================================================
*/


void timer_init();


void keyboard_init();


void pci_init();




/*
====================================================
PROCESS SYSTEM
====================================================
*/


void process_init();


void thread_init();


void thread_create(void (*function)());


void scheduler_init();


void scheduler_run();




/*
====================================================
FILESYSTEM SYSTEM
====================================================
*/


void mkfs_init();




/*
====================================================
EXECUTABLE SYSTEM
====================================================
*/


void mkx_init();




/*
====================================================
PACKAGE SYSTEM
====================================================
*/


void mpm_init();




/*
====================================================
SECURITY SYSTEM
====================================================
*/


void user_system_init();


void password_init();


void permission_init();




/*
====================================================
SHELL SYSTEM
====================================================
*/


void msh_start();





/*
====================================================

        MIKEA KERNEL ENTRY POINT

====================================================
*/


void kernel_start()
{


/*
----------------------------------------------------
DISPLAY INITIALIZATION
----------------------------------------------------
*/


fb_clear();


console_init();




/*
----------------------------------------------------
MEMORY INITIALIZATION
----------------------------------------------------
*/


heap_init();




/*
----------------------------------------------------
CPU INITIALIZATION
----------------------------------------------------
*/


cpu_init();


gdt_init();




/*
----------------------------------------------------
INTERRUPT INITIALIZATION
----------------------------------------------------
*/


idt_init();


irq_init();




/*
----------------------------------------------------
HARDWARE INITIALIZATION
----------------------------------------------------
*/


timer_init();


keyboard_init();


pci_init();




/*
----------------------------------------------------
PROCESS INITIALIZATION
----------------------------------------------------
*/


process_init();


thread_init();


scheduler_init();




/*
----------------------------------------------------
FILESYSTEM INITIALIZATION
----------------------------------------------------
*/


mkfs_init();




/*
----------------------------------------------------
EXECUTABLE INITIALIZATION
----------------------------------------------------
*/


mkx_init();




/*
----------------------------------------------------
PACKAGE MANAGER INITIALIZATION
----------------------------------------------------
*/


mpm_init();




/*
----------------------------------------------------
SECURITY INITIALIZATION
----------------------------------------------------
*/


user_system_init();


password_init();


permission_init();





/*
====================================================
STARTUP INFORMATION
====================================================
*/


console_write(
"================================\n"
);


console_write(
"\n"
);


console_write(
"          Mikea OS 0.2.1\n"
);


console_write(
"\n"
);


console_write(
"       Kernel Starting...\n"
);


console_write(
"\n"
);




/*
====================================================
SYSTEM COMPONENTS
====================================================
*/


console_write(
"System Components\n"
);


console_write(
"-----------------\n"
);



console_write(
"Framebuffer Driver   : READY\n"
);


console_write(
"Console System       : READY\n"
);


console_write(
"Memory Manager       : READY\n"
);


console_write(
"CPU Core             : READY\n"
);


console_write(
"GDT                  : READY\n"
);


console_write(
"IDT                  : READY\n"
);


console_write(
"IRQ Controller       : READY\n"
);




/*
====================================================
HARDWARE STATUS
====================================================
*/


console_write(
"\nHardware Drivers\n"
);


console_write(
"----------------\n"
);



console_write(
"Timer PIT            : READY\n"
);


console_write(
"Keyboard PS/2        : READY\n"
);


console_write(
"PCI Bus              : READY\n"
);




/*
====================================================
PROCESS STATUS
====================================================
*/


console_write(
"\nProcess System\n"
);


console_write(
"--------------\n"
);



console_write(
"Process Manager      : READY\n"
);


console_write(
"Thread Manager       : READY\n"
);


console_write(
"Scheduler            : READY\n"
);




/*
====================================================
STORAGE STATUS
====================================================
*/


console_write(
"\nStorage System\n"
);


console_write(
"--------------\n"
);



console_write(
"MKFS File System     : READY\n"
);




/*
====================================================
APPLICATION STATUS
====================================================
*/


console_write(
"\nApplication System\n"
);


console_write(
"------------------\n"
);



console_write(
"MKX Runtime          : READY\n"
);



console_write(
"MPM Package Manager  : READY\n"
);




/*
====================================================
SECURITY STATUS
====================================================
*/


console_write(
"\nSecurity System\n"
);


console_write(
"---------------\n"
);



console_write(
"User Manager         : READY\n"
);


console_write(
"Password System      : READY\n"
);


console_write(
"Permission System    : READY\n"
);




/*
====================================================
MEMORY TEST
====================================================
*/


console_write(
"\nMemory Test\n"
);


console_write(
"-----------\n"
);



char* memory;



memory =
(char*)mk_malloc(128);



if(memory != 0)
{


memory[0]='M';
memory[1]='I';
memory[2]='K';
memory[3]='E';
memory[4]='A';

memory[5]=' ';
memory[6]='O';
memory[7]='S';

memory[8]='\0';



console_write(
"Heap Allocation      : OK\n"
);



console_write(
"Test Data            : "
);



console_write(
memory
);



console_write(
"\n"
);


}

else
{


console_write(
"Heap Allocation      : FAILED\n"
);


}




/*
====================================================
SYSTEM INFORMATION
====================================================
*/


console_write(
"\nSystem Information\n"
);


console_write(
"------------------\n"
);



console_write(
"Name          : Mikea OS\n"
);


console_write(
"Version       : 0.2.1 ORIGIN\n"
);


console_write(
"Architecture  : x86_64\n"
);


console_write(
"Kernel        : Mikea Kernel\n"
);


console_write(
"Filesystem    : MKFS\n"
);


console_write(
"Executable    : MKX\n"
);


console_write(
"Package       : MPM\n"
);


console_write(
"Security      : Mikea Security\n"
);


console_write(
"Language      : C / Assembly\n"
);


console_write(
"License       : GPL-3.0\n"
);


console_write(
"Developer     : Tantely Orion\n"
);




/*
====================================================
ENABLE INTERRUPTS
====================================================
*/


irq_enable();



console_write(
"\nInterrupts Enabled\n"
);



console_write(
"Mikea OS Core Ready\n"
);



console_write(
"Starting User Environment...\n"
);



console_write(
"\n"
);




/*
====================================================
START SHELL

====================================================

Le shell tourne desormais comme un thread
ordonnance par le round-robin cooperatif,
au lieu d'etre appele directement et de
bloquer tout le noyau dans sa boucle.
*/


thread_create(msh_start);




/*
====================================================
KERNEL LOOP

Boucle d'ordonnancement reelle (round-robin
cooperatif). Avant ce correctif, cette boucle
ne faisait que "hlt" a l'infini : aucun thread
n'etait jamais execute par le scheduler.
====================================================
*/


scheduler_run();


}