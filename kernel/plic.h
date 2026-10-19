#ifndef KERNEL_PLIC_H
#define KERNEL_PLIC_H

void plic_init(void);
void plic_inithart(void);
int plic_claim(void);
void plic_complete(int irq);

#endif
