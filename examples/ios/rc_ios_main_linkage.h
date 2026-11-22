/* Forced into every C++ example translation unit on iOS (-include, C++
   sources only; C never sees this file). The build renames the example's main to
   rci_ios_example_main with -Dmain=..., and rc_ios_entry.c - a C file -
   calls that name with C linkage. Left alone, a C++ definition of it mangles
   (_Z20rci_ios_example_mainv, measured g++ 15 / clang++ 21) and the bundle does
   not link. A definition that follows an extern "C" declaration inherits C
   linkage, so the example keeps its plain int main() and the rename still works.
   A consumer cannot write this escape itself: `extern "C" int main(void)` is an
   error under -pedantic-errors and -Wmain. */
#ifndef RC_IOS_MAIN_LINKAGE_H
#define RC_IOS_MAIN_LINKAGE_H
#ifdef __cplusplus
extern "C" int rci_ios_example_main(void);
#endif
#endif
