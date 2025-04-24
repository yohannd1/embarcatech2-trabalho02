#include "hardware/clocks.h"

#include "ws2812b_matrix.h"
#include "ws2812b.pio.h"

static uint32_t encode_color(ws2812b_color_t c);

bool ws2812b_matrix_init(ws2812b_matrix_t *lm, PIO pio, uint pin) {
	lm->pin = pin;
	lm->pio = pio;

	// 128 MHz, para facilitar a divisão de clock
	if (!set_sys_clock_khz(128000, false))
		return false;

	uint offset = pio_add_program(pio, &ws2812b_program);
	lm->sm = pio_claim_unused_sm(pio, true);
	ws2812b_pio_init(lm->pio, lm->sm, offset, lm->pin);

	return true;
}

void ws2812b_matrix_draw(ws2812b_matrix_t *lm, const ws2812b_buffer_t *disp) {
	for (int row = 4; row >= 0; row--) {
		if (row % 2) {
			for (int col = 0; col < 5; col++) {
				ws2812b_color_t c = (*disp)[5*row + col];
				pio_sm_put_blocking(lm->pio, lm->sm, encode_color(c));
			}
		} else {
			for (int col = 4; col >= 0; col--) {
				ws2812b_color_t c = (*disp)[5*row + col];
				pio_sm_put_blocking(lm->pio, lm->sm, encode_color(c));
			}
		}
	}
}

static uint32_t encode_color(ws2812b_color_t c) {
	uint8_t red = c.r * 255.0f;
	uint8_t green = c.g * 255.0f;
	uint8_t blue = c.b * 255.0f;
	return (green << 24) | (red << 16) | (blue << 8);
}
