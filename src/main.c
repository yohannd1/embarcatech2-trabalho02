#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "ssd1306.h"
#include "ws2812b_matrix.h"

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define LED_ON_INTENSITY 0.02f
#define LED_STRIP_PIN 7
#define ADC_INPUT_PIN 28
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

const float R_CONHECIDO = 1000.0f; // resistência conhecida (ohm)
const uint32_t ADC_RESOLUTION = 4095;

static ws2812b_matrix_t matrix;
static ws2812b_buffer_t buffer;

/**
 * Calcula a resistência para resistores de 4 bandas, com tolerância máxima de
 * 5% para o intervalo especificado.
 *
 * @param resist o valor da resistência (510.0f <= resistance <= 100000.0f)
 * @param digit1 (vai receber) a cor do primeiro dígito (0 <= *digit1 <= 9)
 * @param digit2 (vai receber) a cor do segundo dígito (0 <= *digit2 <= 9)
 * @param mult (vai receber) a cor do multiplicador (0 <= *mult <= 9)
 *
 * Retorna um valor indicando se a função rodou com sucesso.
 */
static bool calc_res_colors_4band(float resist, uint8_t *digit1, uint8_t *digit2, uint8_t *mult);

/**
 * Calcula o valor mais próximo da série E24. Se o valor providenciado estiver
 * igualmente próximo de dois valores da série, pega o menor número.
 *
 * @param two_digits os dois dígitos mais significantes da resistência atual
 */
static uint8_t closest_e24_value(uint8_t two_digits);

static void draw_resistor_repr(uint8_t digit1, uint8_t digit2, uint8_t mult);
static ws2812b_color_t get_color_for(uint8_t idx);
static void die(const char *msg);
static void on_button_press(uint gpio, uint32_t events);

int main(void) {
	stdio_init_all();

	if (!ws2812b_matrix_init(&matrix, pio0, LED_STRIP_PIN))
		die("falha ao inicializar a matriz de LEDs");

	for (int i = 0; i < 25; i++)
		buffer[i] = (ws2812b_color_t) { 0.0f, 0.0f, 0.0f };
	ws2812b_matrix_draw(&matrix, &buffer);

	gpio_init(BUTTON_A_PIN);
	gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_A_PIN);

	gpio_init(BUTTON_B_PIN);
	gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_B_PIN);
	gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &on_button_press);

	i2c_init(DISPLAY_I2C_PORT, 400000); // 400KHz

	gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(DISPLAY_SDA_PIN);
	gpio_pull_up(DISPLAY_SCL_PIN);

	ssd1306_t display;
	ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, DISPLAY_I2C_PORT);

	ssd1306_fill(&display, false);
	ssd1306_send_data(&display);

	adc_init();
	adc_gpio_init(ADC_INPUT_PIN);

	const size_t PASS_COUNT = 500;

	char str_buf[16];
	const size_t str_max = sizeof(str_buf) / sizeof(str_buf[0]);

	while (true) {
		adc_select_input(2); // referente ao GPIO 28

		printf("Vamos calcular...\n");

		float soma = 0.0f;
		for (int i = 0; i < PASS_COUNT; i++) {
			soma += adc_read();
			sleep_ms(1);
		}
		float adc_encontrado = soma / PASS_COUNT;

		printf("Encontrado adc = %.f\n", adc_encontrado);

		// Fórmula simplificada: r_x = R_CONHECIDO * ADC_encontrado / (ADC_RESOLUTION - adc_encontrado)
		float r_x = (R_CONHECIDO * adc_encontrado) / (ADC_RESOLUTION - adc_encontrado);

		/* float r_x; */
		/* if (!fscanf(stdin, "%f", &r_x)) { */
		/* 	printf("whoop\n"); */
		/* 	fflush(stdin); */
		/* } */

		int e24 = -1;

		uint8_t d1, d2, m;
		if (calc_res_colors_4band(r_x, &d1, &d2, &m)) {
			printf("Código de 4 bandas calculado (para %.f ohms): { %u %u %u } (tol. 5%)\n", r_x, d1, d2, m);
			e24 = closest_e24_value(d1 * 10 + d2);
			printf("Valor da série: %d\n", e24);
			draw_resistor_repr(d1, d2, m);
		} else {
			printf("Falha ao calcular o valor da resistência\n");
		}

		ssd1306_fill(&display, 0);
		ssd1306_rect(&display, 3, 3, 122, 60, 0, 1);
		ssd1306_line(&display, 3, 25, 123, 25, 0);
		ssd1306_line(&display, 3, 37, 123, 37, 0);

		uint8_t x = 12, y = 6;

		ssd1306_draw_string(&display, "EMBARCATECH II", x, y);
		y += 8;

		x = 18;
		ssd1306_draw_string(&display, "OHMIMETRO", x, y);
		y += 12;

		ssd1306_line(&display, 10, y, DISPLAY_WIDTH - 10, y, 1);
		y += 2;

		x = 8;
		snprintf(str_buf, str_max, "ADC %.0f", adc_encontrado);
		ssd1306_draw_string(&display, str_buf, x, y);
		y += 8;

		snprintf(str_buf, str_max, "Rx %.0f ohm", r_x);
		ssd1306_draw_string(&display, str_buf, x, y);
		y += 8;

		snprintf(str_buf, str_max, "E24 n. %d", e24);
		ssd1306_draw_string(&display, str_buf, x, y);
		y += 8;

		ssd1306_send_data(&display);

		sleep_ms(700);
	}

	return 0;
}

static void on_button_press(uint gpio, uint32_t events) {
	if (gpio == BUTTON_B_PIN) {
		printf("Entrando no modo bootsel...\n");
		reset_usb_boot(0, 0);
	}
}

static bool calc_res_colors_4band(float resist, uint8_t *digit1, uint8_t *digit2, uint8_t *mult) {
	// retornar falso se o valor estiver fora do intervalo especificado
	if (resist < 510.0f || resist > 100000.0f)
		return false;

	float val = resist;
	uint8_t m = 0;

	while (val >= 100.0f) {
		// dividir por 10 e aumentar o multiplicador
		val /= 10.0f;
		m++;
	}

	*digit1 = (int)val / 10;
	*digit2 = (int)val % 10;
	*mult = m;

	return true;
}

static void draw_resistor_repr(uint8_t digit1, uint8_t digit2, uint8_t mult) {
	buffer[5*2 + 1] = get_color_for(digit1);
	buffer[5*2 + 2] = get_color_for(digit2);
	buffer[5*2 + 3] = get_color_for(mult);
	ws2812b_matrix_draw(&matrix, &buffer);
}

static ws2812b_color_t get_color_for(uint8_t idx) {
	if (idx > 9) idx = 9;

	const ws2812b_color_t colors[10] = {
		{ 0.0f, 0.0f,  0.0f }, // preto
		{ 0.2f, 0.25f, 0.0f }, // marrom (não muito bom)
		{ 1.0f, 0.0f,  0.0f }, // vermelho
		{ 1.0f, 0.5f,  0.0f }, // laranja
		{ 1.0f, 1.0f,  0.0f }, // amarelo
		{ 0.0f, 1.0f,  0.0f }, // verde
		{ 0.0f, 0.0f,  1.0f }, // azul
		{ 0.5f, 0.15f, 1.0f }, // violeta
		{ 0.5f, 0.5f,  0.5f }, // cinza
		{ 1.0f, 1.0f,  1.0f }, // branco
	};

	ws2812b_color_t ret = colors[idx];
	ret.r *= LED_ON_INTENSITY;
	ret.g *= LED_ON_INTENSITY;
	ret.b *= LED_ON_INTENSITY;
	return ret;
}

static uint8_t closest_e24_value(uint8_t two_digits) {
	const uint8_t e24[] = {
		10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30,
		33, 36, 39, 43, 47, 51, 56, 62, 68, 75, 82, 91,
	};
	const size_t len = sizeof(e24) / sizeof(e24[0]);

	if (two_digits <= e24[0]) return e24[0];
	if (two_digits >= e24[len-1]) return e24[len-1];

	for (int i = 0; i < len - 1; i++) {
		if (two_digits <= e24[i+1]) {
			int dist1 = (int)two_digits - (int)e24[i];
			int dist2 = (int)two_digits - (int)e24[i+1];

			if (abs(dist1) < abs(dist2)) return e24[i];
			else return e24[i+1];
		}
	}
}

static void die(const char *msg) {
	while (true) {
		printf("ERRO FATAL: %s\n", msg);
		sleep_ms(2000);
	}
}
