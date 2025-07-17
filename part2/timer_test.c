//
// Created by stevehb on 15-Jul-25.
//


#include "common_funcs.h"
#include "tempo.h"

#include <stdio.h>
#include <stdlib.h>

const char* INNER_NAMES[5] = { "inner[025]", "inner[050]", "inner[075]", "inner[100]", "inner[125]", };
const char* J_NAMES[5] = { "j0", "j1", "j2", "j3", "j4" };

void testerFunc(u64 delay) {
    tempo_startFunc;
    tempo_startBlock(INNER_NAMES[(delay / 25) - 1]);
    sleep_ms(delay);
    tempo_stopBlock(NULL);
    tempo_stopFunc;
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    tempo_startProfile("MAIN");
    for (int i = 1; i <= 5; i++) {
        u64 ms = i * 25;
        if (i == 3) {
            for (int j = 0; j < 5; j++) {
                tempo_startBlock(J_NAMES[j]);
            }
            tempo_startBlock("deepest");
            tempo_stopBlock("deepest");
            for (int j = 0; j < 5; j++) {
                tempo_stopBlock(J_NAMES[j]);
            }
        }
        testerFunc(ms);
    }
    tempo_stopProfile();

    tempo_printProfile();
    return 0;
}
