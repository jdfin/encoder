
#include <stdexcept>
#include <gpiod.h>
#include "encoder.h"


Encoder::Encoder(const char *chip_path, unsigned int gpio_a, unsigned int gpio_b, int verbosity) :
    _count(0),
    _events(nullptr),
    _gpio_a(gpio_a),
    _gpio_b(gpio_b),
    _chip(nullptr),
    _line_request(nullptr)
{

    _events = gpiod_edge_event_buffer_new(max_events);
    if (_events == nullptr) {
        throw std::runtime_error("gpiod_edge_event_buffer_new");
    }

    // Things to free:
    //   _events

    gpiod_line_settings *line_settings = gpiod_line_settings_new();
    if (line_settings == nullptr) {
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_line_settings_new");
    }

    // Things to free:
    //   line_settings
    //   _events

    gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(line_settings, GPIOD_LINE_EDGE_BOTH);
    gpiod_line_settings_set_bias(line_settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_debounce_period_us(line_settings, debounce_us);
    gpiod_line_settings_set_event_clock(line_settings, GPIOD_LINE_CLOCK_MONOTONIC);

    gpiod_line_config *line_config = gpiod_line_config_new();
    if (line_config == nullptr) {
        gpiod_line_settings_free(line_settings);
        line_settings = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_line_config_new");
    }

    // Things to free:
    //   line_config
    //   line_settings
    //   _events

    const unsigned int offsets[num_gpios] = { _gpio_a, _gpio_b };

    if (gpiod_line_config_add_line_settings(line_config, offsets, num_gpios, line_settings) != 0) {
        gpiod_line_config_free(line_config);
        line_config = nullptr;
        gpiod_line_settings_free(line_settings);
        line_settings = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_line_config_add_line_settings");
    }

    gpiod_line_settings_free(line_settings);
    line_settings = nullptr;

    // Things to free:
    //   line_config
    //   _events

    gpiod_request_config *request_config = gpiod_request_config_new();
    if (request_config == nullptr) {
        gpiod_line_config_free(line_config);
        line_config = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_request_config_new");
    }

    // Things to free:
    //   request_config
    //   line_config
    //   _events

    gpiod_request_config_set_consumer(request_config, "encoder");

    _chip = gpiod_chip_open(chip_path);
    if (_chip == nullptr) {
        gpiod_request_config_free(request_config);
        request_config = nullptr;
        gpiod_line_config_free(line_config);
        line_config = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_chip_open");
    }

    // Things to free:
    //   _chip
    //   request_config
    //   line_config
    //   _events

    _line_request = gpiod_chip_request_lines(_chip, request_config, line_config);
    if (_line_request == nullptr) {
        gpiod_chip_close(_chip);
        _chip = nullptr;
        gpiod_request_config_free(request_config);
        request_config = nullptr;
        gpiod_line_config_free(line_config);
        line_config = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_request_lines");
    }

    // Things to free:
    //   _line_request
    //   _chip
    //   request_config
    //   line_config
    //   _events

    gpiod_request_config_free(request_config);
    request_config = nullptr;

    gpiod_line_config_free(line_config);
    line_config = nullptr;

    // Things to free:
    //   _line_request
    //   _chip
    //   _events

    // read initial states

    gpiod_line_value values[2];

    if (gpiod_line_request_get_values(_line_request, values) != 0) {
        gpiod_line_request_release(_line_request);
        _line_request = nullptr;
        gpiod_chip_close(_chip);
        _chip = nullptr;
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
        throw std::runtime_error("gpiod_request_lines");
    }

    if (values[0] == GPIOD_LINE_VALUE_ACTIVE)
        _gpio_a_last = rising;
    else
        _gpio_a_last = falling;

    if (values[1] == GPIOD_LINE_VALUE_ACTIVE)
        _gpio_b_last = rising;
    else
        _gpio_b_last = falling;

} // Encoder::Encoder


Encoder::~Encoder()
{
    if (_line_request != nullptr) {
        gpiod_line_request_release(_line_request);
        _line_request = nullptr;
    }

    if (_chip != nullptr) {
        gpiod_chip_close(_chip);
        _chip = nullptr;
    }

    if (_events != nullptr) {
        gpiod_edge_event_buffer_free(_events);
        _events = nullptr;
    }

    _count = 0;

} // Encoder::~Encoder


// timeout_us = -1 to wait forever, 0 to not wait, or microseconds
void Encoder::update(int timeout_us)
{
    int ret = gpiod_line_request_wait_edge_events(_line_request, timeout_us * 1000ULL);

    if (ret <= 0)
        return;

    int num_events = gpiod_line_request_read_edge_events(_line_request, _events, max_events);
    if (num_events <= 0)
        throw std::runtime_error("gpiod_line_request_read_edge_events");

    for (int i = 0; i < num_events; i++) {
        gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(_events, i);
        unsigned int pin = gpiod_edge_event_get_line_offset(event);
        gpiod_edge_event_type edge = gpiod_edge_event_get_event_type(event);

        if (pin != _gpio_a && pin != _gpio_b)
            continue; // assert fail?

        if (edge != rising && edge != falling)
            continue; // assert fail?

        //std::cout << "pin=" << pin << " edge=" << int(edge) << std::endl;


        //  --> increment (clockwise) -->
        //
        //    +---+   +---+   +---+   +-
        // a  |   |   |   |   |   |   |
        //   -+   +---+   +---+   +---+
        //
        //      +---+   +---+   +---
        // b    |   |   |   |   |
        //   ---+   +---+   +---+

        int dir;
        if (pin == _gpio_a) {
            if (edge == rising) {
                if (_gpio_b_last == rising) {
                    dir = -1; // b high, a rising
                } else {
                    dir = +1; // b low, a rising
                }
            } else { // a falling
                if (_gpio_b_last == rising) {
                    dir = +1; // b high, a falling
                } else {
                    dir = -1; // b low, a falling
                }
            }
            _gpio_a_last = edge;
        } else { // _gpio_b
            if (edge == rising) {
                if (_gpio_a_last == rising) {
                    dir = +1; // a high, b rising
                } else {
                    dir = -1; // a low, b rising
                }
            } else { // b falling
                if (_gpio_a_last == rising) {
                    dir = -1; // a high, b falling
                } else {
                    dir = +1; // a low, b falling
                }
            }
            _gpio_b_last = edge;
        }
        on_change(dir);

    } // for

} // Encoder::update


void Encoder::on_change(int inc)
{
    _count += inc;
}
