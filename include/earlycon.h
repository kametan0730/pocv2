#ifndef CORE_EARLYCON_H
#define CORE_EARLYCON_H

// #define EARLY_PL011_BASE    0xfe201000
#define EARLY_PL011_BASE    0x09000000

#ifndef __ASSEMBLER__

void earlycon_puts(const char *s);

#endif  /* __ASSEMBLER__ */

#endif  /* CORE_EARLYCON_H */
