/* Mock du minuteur materiel (PIT), utilise uniquement par
   security/user.c pour deriver le sel des mots de passe.
   Une valeur qui avance a chaque appel suffit ici : le vrai
   noyau utilise timer_tick()/IRQ0 (kernel/drivers/timer). */
static unsigned long fake_ticks = 1;
unsigned long timer_ticks() { return fake_ticks++; }
