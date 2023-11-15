
#include <iostream>
//#include <unistd.h>
#include "encoder.h"

static const char *chip_path = "/dev/gpiochip0";

static const int gpio_a = 23;
static const int gpio_b = 24;


int main(int argc, char *argv[])
{
    int verbosity = 3;

    Encoder encoder(chip_path, gpio_a, gpio_b, verbosity);

    int count_last = 0;

    while (true) {

        // nonblocking by default, -1 means block
        encoder.update(-1);

        if (count_last != encoder.count()) {
            std::cout << encoder.count() << std::endl;
            count_last = encoder.count();
        }

        //usleep(5000);
    }

    return 0;
}
