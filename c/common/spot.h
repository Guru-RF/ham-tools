#ifndef HAM_SPOT_H
#define HAM_SPOT_H

typedef struct {
    char spotter[16];
    char freq[16];
    char dx[16];
    char info[48];
    char timez[16];
    char country[48];
    char mode[8];       /* empty if unknown */
} ham_spot;

#endif
