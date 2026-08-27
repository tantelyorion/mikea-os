#include "speaker.h"

#include "../../cpu/io.h"

#include "../soundblaster/sb16.h"
#include "../soundblaster/sound_data.h"


void console_write(const char* text);

unsigned long timer_ticks();


#define PIT_CHANNEL2_DATA 0x42

#define PIT_COMMAND        0x43

#define PIT_BASE_FREQUENCY 1193182


#define SPEAKER_CONTROL_PORT 0x61


static int g_sound_enabled = 1;


void sound_set_enabled(int enabled)
{

g_sound_enabled = enabled ? 1 : 0;

}


int sound_is_enabled()
{

return g_sound_enabled;

}


static void speaker_on(u32 frequency_hz)
{

if (frequency_hz == 0)
{

return;

}


u32 divisor = PIT_BASE_FREQUENCY / frequency_hz;


/*
    0xB6 = canal 2, acces "lobyte puis hibyte", mode 3 (onde
    carree), binaire -- configuration standard du PC Speaker
    (voir le wiki OSDev, "PC Speaker"). Independant du canal 0
    deja utilise par kernel/drivers/timer/timer.c pour l'horloge
    systeme : reprogrammer le canal 2 ici ne touche pas au
    minuteur de 100Hz dont depend l'ordonnanceur de threads.
*/

outb(PIT_COMMAND, 0xB6);

outb(PIT_CHANNEL2_DATA, (u8)(divisor & 0xFF));

outb(PIT_CHANNEL2_DATA, (u8)((divisor >> 8) & 0xFF));


/*
    Bits 0-1 du port 0x61 : bit 0 active le canal 2 du PIT,
    bit 1 connecte sa sortie au haut-parleur -- les deux
    doivent etre a 1 pour entendre quoi que ce soit.
*/

u8 control = inb(SPEAKER_CONTROL_PORT);

if ((control & 0x03) != 0x03)
{

outb(SPEAKER_CONTROL_PORT, control | 0x03);

}

}


static void speaker_off()
{

u8 control = inb(SPEAKER_CONTROL_PORT);

outb(SPEAKER_CONTROL_PORT, control & 0xFC);

}


void speaker_beep(u32 frequency_hz, u32 duration_ms)
{

if (!g_sound_enabled)
{

return;

}


speaker_on(frequency_hz);


/*
    timer_ticks() avance a 100Hz (10ms/tick, voir
    kernel/drivers/timer/timer.c) : meme technique de cadence
    que le limiteur d'images des applications graphiques
    (ex. apps/calculator/calculator.c), pas une temporisation
    materielle dediee.
*/

unsigned long ticks_needed = duration_ms / 10;

if (ticks_needed == 0)
{

ticks_needed = 1;

}


unsigned long start = timer_ticks();

while (timer_ticks() - start < ticks_needed)
{
}


speaker_off();

}


void sound_play_startup()
{

/*
    Correctif (vrai son de demarrage) : si une Sound Blaster 16
    est detectee (voir kernel/kernel.c, sb16_init() -- section
    HARDWARE INITIALIZATION, avant cet appel), on joue le vrai
    clip audio fourni par l'utilisateur (converti hors ligne, voir
    sound_data.h) plutot qu'un simple bip synthetique. Sur du
    materiel/un emulateur sans Sound Blaster 16 (cas normal en
    dehors de QEMU avec "-device sb16"), on retombe sur les deux
    notes de PC Speaker ci-dessous -- meme logique de repli honnete
    que power_shutdown() (kernel/drivers/power/power.c) quand
    aucune methode d'extinction ACPI ne fonctionne.
*/

if (sb16_available())
{

sb16_play_pcm(startup_pcm, startup_pcm_len, SOUND_PCM_SAMPLE_RATE);

return;

}


/* Repli : deux notes ascendantes, courtes -- accueil bref, pas envahissant. */

speaker_beep(523, 90);

speaker_beep(0, 20);

speaker_beep(784, 140);

}


void sound_play_shutdown()
{

/* Voir le commentaire de sound_play_startup() ci-dessus : meme logique de repli. */

if (sb16_available())
{

sb16_play_pcm(shutdown_pcm, shutdown_pcm_len, SOUND_PCM_SAMPLE_RATE);

return;

}


/* Repli : deux notes descendantes -- symetrique de sound_play_startup(). */

speaker_beep(784, 90);

speaker_beep(0, 20);

speaker_beep(392, 160);

}


void sound_play_error()
{

/* Un seul ton bas et bref -- distinct des melodies a deux notes ci-dessus, sans etre agressif. */

speaker_beep(220, 180);

}


void sound_play_usb_added()
{

speaker_beep(440, 60);

speaker_beep(880, 80);

}


void sound_play_usb_removed()
{

speaker_beep(880, 60);

speaker_beep(440, 80);

}
