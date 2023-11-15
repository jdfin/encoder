#pragma once

#include <cstddef>
#include <string>
#include <gpiod.h>

// 24 pulses/rev
// 2 msec max bounce


class Encoder
{

    public:

        Encoder(const char *chip_path, unsigned int gpio_a, unsigned int gpio_b, int verbosity=0);

        virtual ~Encoder();

        void update(int timeout_us=0);

        // inc = 1 for clockwise tick, -1 for counterclockwise tick
        virtual void on_change(int inc);

        int count() const { return _count; }

        void count(int count) { _count = count; }

        int fd() const { return gpiod_line_request_get_fd(_line_request); }

    protected:

    private:

        int _count;

        static const size_t max_events = 32;
        gpiod_edge_event_buffer *_events;

        static const unsigned long debounce_us = 2000;

        static const int num_gpios = 2;
        unsigned int _gpio_a;
        unsigned int _gpio_b;

        gpiod_chip *_chip;

        gpiod_line_request *_line_request;

        // shorthand
        static const gpiod_edge_event_type rising = GPIOD_EDGE_EVENT_RISING_EDGE;
        static const gpiod_edge_event_type falling = GPIOD_EDGE_EVENT_FALLING_EDGE;

        gpiod_edge_event_type _gpio_a_last;
        gpiod_edge_event_type _gpio_b_last;

}; // class Encoder
