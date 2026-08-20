#include "rtc.h"

#include "../../cpu/io.h"


#define CMOS_INDEX_PORT 0x70

#define CMOS_DATA_PORT  0x71


#define CMOS_REG_SECOND   0x00

#define CMOS_REG_MINUTE   0x02

#define CMOS_REG_HOUR     0x04

#define CMOS_REG_DAY      0x07

#define CMOS_REG_MONTH    0x08

#define CMOS_REG_YEAR     0x09

#define CMOS_REG_STATUS_A 0x0A

#define CMOS_REG_STATUS_B 0x0B


static u8 cmos_read(u8 reg)
{

/*
    Bit 7 de l'octet d'index (NMI Disable) laisse a 0 : ce
    projet n'a pas besoin de desactiver les NMI pour une simple
    lecture d'horloge, contrairement a certaines sequences
    d'initialisation plus sensibles.
*/

outb(CMOS_INDEX_PORT, reg);

return inb(CMOS_DATA_PORT);

}


static int rtc_update_in_progress()
{

return (cmos_read(CMOS_REG_STATUS_A) & 0x80) != 0;

}


static u8 bcd_to_binary(u8 value)
{

return (u8)(((value & 0xF0) >> 1) + ((value & 0xF0) >> 3) + (value & 0x0F));

}


static void rtc_read_raw(rtc_time* out)
{

/*
    Correctif stabilite (meme principe que
    kernel/drivers/mouse/mouse.c et filesystem/disk.c) : une
    attente materielle ne doit JAMAIS pouvoir figer tout le
    systeme indefiniment, meme dans un cas totalement
    inattendu (comportement d'emulateur different d'un vrai
    CMOS, par exemple). Bornee a un grand nombre d'iterations
    plutot qu'une boucle "while" nue : en cas de depassement
    (ce qui ne devrait jamais arriver en usage normal), on
    continue avec la valeur lue telle quelle plutot que de
    bloquer le reste de l'OS pour un simple affichage
    d'horloge.
*/

u32 timeout = 100000;

while (rtc_update_in_progress() && timeout > 0)
{

timeout--;

}


out->second = cmos_read(CMOS_REG_SECOND);

out->minute = cmos_read(CMOS_REG_MINUTE);

out->hour = cmos_read(CMOS_REG_HOUR);

out->day = cmos_read(CMOS_REG_DAY);

out->month = cmos_read(CMOS_REG_MONTH);

out->year = cmos_read(CMOS_REG_YEAR);

}


void rtc_read(rtc_time* out)
{


rtc_time first;

rtc_time second;


/*
    Algorithme standard (voir OSDev Wiki, "CMOS" -- meme
    principe que tout pilote RTC serieux) : deux lectures
    brutes successives. Si elles different, l'une des deux est
    tombee en plein milieu d'une mise a jour interne de la
    puce (meme apres avoir attendu la fin du bit "Update In
    Progress" ci-dessus -- une petite fenetre de temps existe
    encore juste apres) ; on recommence jusqu'a obtenir deux
    lectures identiques.
*/

rtc_read_raw(&first);


/*
    Bornee elle aussi (voir le commentaire de rtc_read_raw()
    ci-dessus) : deux lectures qui ne se stabilisent jamais
    (improbable, mais pas a exclure sous un emulateur) ne
    doivent pas boucler indefiniment -- on se contente alors de
    la derniere lecture obtenue, potentiellement decalee d'une
    seconde tout au plus, plutot que de figer tout le systeme
    pour un affichage d'horloge.
*/

int stabilize_attempts = 10;

while (stabilize_attempts > 0)
{

rtc_read_raw(&second);


if (first.second == second.second
&& first.minute == second.minute
&& first.hour == second.hour
&& first.day == second.day
&& first.month == second.month
&& first.year == second.year)
{

break;

}


first = second;

stabilize_attempts--;

}


u8 status_b = cmos_read(CMOS_REG_STATUS_B);

int is_binary = (status_b & 0x04) != 0;

int is_24h = (status_b & 0x02) != 0;


u8 raw_year = second.year;

u8 raw_month = second.month;

u8 raw_day = second.day;

u8 raw_hour = second.hour;

u8 raw_minute = second.minute;

u8 raw_second = second.second;


if (!is_binary)
{

/*
    Bit 7 de l'heure = indicateur PM en mode BCD 12h (voir
    ci-dessous) : a masquer AVANT la conversion BCD, sinon
    ce bit se retrouve interprete a tort comme un chiffre.
*/

int pm = (raw_hour & 0x80) != 0;

raw_hour &= 0x7F;


raw_second = bcd_to_binary(raw_second);

raw_minute = bcd_to_binary(raw_minute);

raw_hour = bcd_to_binary(raw_hour);

raw_day = bcd_to_binary(raw_day);

raw_month = bcd_to_binary(raw_month);

raw_year = bcd_to_binary(raw_year);


if (!is_24h)
{

raw_hour = (u8)(raw_hour % 12);

if (pm)
{

raw_hour = (u8)(raw_hour + 12);

}

}

}

else if (!is_24h)
{

int pm = (raw_hour & 0x80) != 0;

raw_hour &= 0x7F;

raw_hour = (u8)(raw_hour % 12);

if (pm)
{

raw_hour = (u8)(raw_hour + 12);

}

}


out->second = raw_second;

out->minute = raw_minute;

out->hour = raw_hour;

out->day = raw_day;

out->month = raw_month;

/*
    Pas de registre "siecle" lu ici (voir le commentaire de
    rtc.h) : 00-99 suppose 2000-2099.
*/

out->year = 2000 + raw_year;

}
