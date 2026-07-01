#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sht31-d.h"

int main() {
    int file = sht31_open(3, SHT31_DEFAULT_ADDR);

    while (1) {
        float tempC, humid;
        int rtn = gettempandhumidity(file, &tempC, &humid);
        if (rtn == SHT31_OK) {
            printf("Temperature %.2fc\n", tempC);
            printf("Humidity %.2f%%\n", humid);
        }
        sleep(1);
    }

    sht31_close(file);
    return 0;
}
