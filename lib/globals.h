// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

extern float temperatura_bmp;
extern float temperatura_aht;
extern float umidade;
extern float altitude;

extern float offsetTemp;
extern float offsetUmi;
extern float offsetAlt;

typedef struct {
    float min;
    float max;
    bool set;
} Limite;

extern Limite limiteTemp;
extern Limite limiteUmi;
extern Limite limiteAlt;
#endif
