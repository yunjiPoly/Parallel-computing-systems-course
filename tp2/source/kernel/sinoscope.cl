#include "helpers.cl"

typedef struct sinoscope_float_args {
	float interval_inverse;
    float time;
    float max;
    float phase0;
    float phase1;
    float dx;
    float dy;
} sinoscope_float_args_t;

typedef struct sinoscope_int_args {
	unsigned int buffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int taylor;
    unsigned int interval;
} sinoscope_int_args_t;

__kernel void kernel_sinoscope (__global unsigned char* buffer, sinoscope_int_args_t sinoscope_int_args, sinoscope_float_args_t sinoscope_float_args) {
    const int i = get_global_id(0);
    const int j = get_global_id(1);

    float px    = sinoscope_float_args.dx * j - 2 * M_PI;
    float py    = sinoscope_float_args.dy * i - 2 * M_PI;
    float value = 0;

    for (int k = 1; k <= sinoscope_int_args.taylor; k += 2) {
        value += sin(px * k * sinoscope_float_args.phase1 + sinoscope_float_args.time) / k;
        value += cos(py * k * sinoscope_float_args.phase0) / k;
    }

    value = (atan(value) - atan(-value)) / M_PI;
    value = (value + 1) * 100;

    pixel_t pixel;
    color_value(&pixel, value, sinoscope_int_args.interval, sinoscope_float_args.interval_inverse);

    int index = (i * 3) + (j * 3) * sinoscope_int_args.width;

    buffer[index + 0] = pixel.bytes[0];
    buffer[index + 1] = pixel.bytes[1];
    buffer[index + 2] = pixel.bytes[2];
}