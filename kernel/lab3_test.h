#ifndef KERNEL_LAB3_TEST_H
#define KERNEL_LAB3_TEST_H

#if LAB2_TEST && LAB3_TEST
#error "Select only one laboratory test suite"
#endif

#if LAB3_TEST
void lab3_test_hart(void);
#endif

#endif
