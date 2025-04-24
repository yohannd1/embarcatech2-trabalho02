#ifndef _WS2812B_MATRIX_H
#define _WS2812B_MATRIX_H

#include "hardware/pio.h"

typedef struct ws2812b_matrix_t {
	PIO pio;
	uint pin;
	uint sm;
} ws2812b_matrix_t;

typedef struct ws2812b_color_t {
	float r, g, b;
} ws2812b_color_t;

typedef ws2812b_color_t ws2812b_buffer_t[25];

bool ws2812b_matrix_init(ws2812b_matrix_t *lm, PIO pio, uint pin);
void ws2812b_matrix_draw(ws2812b_matrix_t *lm, const ws2812b_buffer_t *disp);

#endif
