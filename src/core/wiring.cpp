/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

#include "Arduino.h"

#define N_BANKS 6
#define N_PINS (N_BANKS * 32)

static const struct device *const gpio_dev[N_BANKS] = {
	DEVICE_DT_GET(DT_NODELABEL(gpioa)), DEVICE_DT_GET(DT_NODELABEL(gpiob)),
	DEVICE_DT_GET(DT_NODELABEL(gpioc)), DEVICE_DT_GET(DT_NODELABEL(gpiod)),
	DEVICE_DT_GET(DT_NODELABEL(gpioe)), DEVICE_DT_GET(DT_NODELABEL(gpiof)),
};

/* pins of analogWrite() that were routed to the PWM output (see pwm_apply) */
static uint32_t pwm_routed;

static inline bool pin_valid(pin_size_t pin)
{
	return pin >= 0 && pin < N_PINS;
}

static inline const struct device *pin_dev(pin_size_t pin)
{
	return gpio_dev[pin >> 5];
}

/* ---- time ---------------------------------------------------------------- */

unsigned long millis(void)
{
	return (unsigned long)k_uptime_get_32();
}

unsigned long micros(void)
{
	return (unsigned long)k_cyc_to_us_floor64(k_cycle_get_64());
}

void delay(unsigned long ms)
{
	if (ms) {
		k_msleep(ms);
	} else {
		k_yield();
	}
}

void delayMicroseconds(unsigned int us)
{
	k_busy_wait(us);
}

void yield(void)
{
	k_yield();
}

void interrupts(void)
{
	/* nesting-free: the lock key is not kept, so this re-enables unconditionally */
	extern void arduino_irq_unlock(void);
	arduino_irq_unlock();
}

void noInterrupts(void)
{
	extern void arduino_irq_lock(void);
	arduino_irq_lock();
}

static unsigned int irq_key;
void arduino_irq_lock(void) { irq_key = irq_lock(); }
void arduino_irq_unlock(void) { irq_unlock(irq_key); }

/* ---- digital I/O --------------------------------------------------------- */

void pinMode(pin_size_t pin, PinMode mode)
{
	if (!pin_valid(pin)) {
		return;
	}
	for (size_t i = 0; i < ARRAY_SIZE(variant_pwm_pins); i++) {
		if (variant_pwm_pins[i] == pin) {
			pwm_routed &= ~BIT(i); /* the pin goes back to GPIO */
		}
	}
	gpio_flags_t f;
	switch (mode) {
	case OUTPUT: f = GPIO_OUTPUT; break;
	case INPUT_PULLUP: f = GPIO_INPUT | GPIO_PULL_UP; break;
	case INPUT_PULLDOWN: f = GPIO_INPUT | GPIO_PULL_DOWN; break;
	default: f = GPIO_INPUT; break;
	}
	gpio_pin_configure(pin_dev(pin), pin & 31, f);
}

void digitalWrite(pin_size_t pin, PinStatus val)
{
	if (pin_valid(pin)) {
		gpio_pin_set_raw(pin_dev(pin), pin & 31, val != LOW);
	}
}

int digitalRead(pin_size_t pin)
{
	if (!pin_valid(pin)) {
		return LOW;
	}
	return gpio_pin_get_raw(pin_dev(pin), pin & 31) > 0 ? HIGH : LOW;
}

/* ---- external interrupts ------------------------------------------------- */

struct pin_irq {
	struct gpio_callback cb;
	void (*fn)(void);
};
static struct pin_irq *pin_irqs[N_PINS];

static void pin_irq_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	struct pin_irq *p = CONTAINER_OF(cb, struct pin_irq, cb);
	p->fn();
}

void attachInterrupt(pin_size_t pin, void (*fn)(void), int mode)
{
	if (!pin_valid(pin) || !fn) {
		return;
	}
	gpio_flags_t f;
	switch (mode) {
	case RISING: f = GPIO_INT_EDGE_RISING; break;
	case FALLING: f = GPIO_INT_EDGE_FALLING; break;
	case CHANGE: f = GPIO_INT_EDGE_BOTH; break;
	case ONHIGH: f = GPIO_INT_LEVEL_HIGH; break;
	default: f = GPIO_INT_LEVEL_LOW; break;
	}
	const struct device *d = pin_dev(pin);
	struct pin_irq *p = pin_irqs[pin];
	if (!p) {
		p = (struct pin_irq *)calloc(1, sizeof(*p));
		if (!p) {
			return;
		}
		pin_irqs[pin] = p;
		gpio_init_callback(&p->cb, pin_irq_handler, BIT(pin & 31));
		gpio_add_callback(d, &p->cb);
	}
	p->fn = fn;
	gpio_pin_interrupt_configure(d, pin & 31, f);
}

void detachInterrupt(pin_size_t pin)
{
	if (!pin_valid(pin) || !pin_irqs[pin]) {
		return;
	}
	gpio_pin_interrupt_configure(pin_dev(pin), pin & 31, GPIO_INT_DISABLE);
}

/* ---- analog input -------------------------------------------------------- */

#define ADC_NODE DT_NODELABEL(adc)
#define ADC_CHANNELS 12

#if DT_NODE_HAS_STATUS(ADC_NODE, okay)
static const struct device *const adc_dev = DEVICE_DT_GET(ADC_NODE);
#define ADC_PRESENT 1
#else
static const struct device *const adc_dev = NULL;
#define ADC_PRESENT 0
#endif
static uint32_t adc_ready_mask;
static int read_bits = 10;
static int write_bits = 8;

void analogReadResolution(int bits)
{
	read_bits = constrain(bits, 1, 16);
}

void analogWriteResolution(int bits)
{
	write_bits = constrain(bits, 1, 16);
}

void analogReference(uint8_t mode)
{
	ARG_UNUSED(mode);
}

int analogRead(pin_size_t pin)
{
	int ch = pin - A0;
	if (ch < 0 || ch >= ADC_CHANNELS || !ADC_PRESENT || !device_is_ready(adc_dev)) {
		return 0;
	}
	if (!(adc_ready_mask & BIT(ch))) {
		struct adc_channel_cfg cfg = {};
		cfg.gain = ADC_GAIN_1;
		cfg.reference = ADC_REF_INTERNAL;
		cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
		cfg.channel_id = ch;
		if (adc_channel_setup(adc_dev, &cfg) < 0) {
			return 0;
		}
		adc_ready_mask |= BIT(ch);
	}
	uint16_t raw = 0;
	struct adc_sequence seq = {};
	seq.channels = BIT(ch);
	seq.buffer = &raw;
	seq.buffer_size = sizeof(raw);
	seq.resolution = 12;
	if (adc_read(adc_dev, &seq) < 0) {
		return 0;
	}
	return read_bits >= 12 ? (int)raw << (read_bits - 12) : (int)raw >> (12 - read_bits);
}

/* ---- PWM output ---------------------------------------------------------- */

#define PWM_NODE DT_NODELABEL(pwm)
#if DT_NODE_HAS_STATUS(PWM_NODE, okay)
static const struct device *const pwm_dev = DEVICE_DT_GET(PWM_NODE);
#define PWM_PRESENT 1
#else
static const struct device *const pwm_dev = NULL;
#define PWM_PRESENT 0
#endif
static uint32_t pwm_period_ns[ARRAY_SIZE(variant_pwm_pins)];

/* returns the PWM channel of a pin or -1 (table comes from pins_arduino.h) */
static int pwm_channel_of(pin_size_t pin)
{
	for (size_t i = 0; i < ARRAY_SIZE(variant_pwm_pins); i++) {
		if (variant_pwm_pins[i] == pin) {
			return (int)i;
		}
	}
	return -1;
}

static void pwm_apply(int ch, uint32_t period_ns, uint32_t duty_ns)
{
	/* the pins are not claimed at boot: route this one the first time it is used */
	if (!(pwm_routed & BIT(ch))) {
		int pin = variant_pwm_pins[ch];
		pinctrl_soc_pin_t p = {.pinmux = ALLWINNER_PINMUX(pin >> 5, pin & 31, variant_pwm_mux[ch])};

		pinctrl_configure_pins(&p, 1, PINCTRL_REG_NONE);
		pwm_routed |= BIT(ch);
	}
	pwm_set(pwm_dev, ch, period_ns, duty_ns, 0);
}

void analogWriteFrequency(pin_size_t pin, uint32_t hz)
{
	int ch = pwm_channel_of(pin);
	if (ch >= 0 && hz) {
		pwm_period_ns[ch] = 1000000000UL / hz;
	}
}

void analogWrite(pin_size_t pin, int val)
{
	int ch = pwm_channel_of(pin);
	int maxv = (1 << write_bits) - 1;
	val = constrain(val, 0, maxv);
	if (ch < 0 || !PWM_PRESENT || !device_is_ready(pwm_dev)) {
		pinMode(pin, OUTPUT);
		digitalWrite(pin, val >= (maxv + 1) / 2 ? HIGH : LOW);
		return;
	}
	uint32_t period = pwm_period_ns[ch] ? pwm_period_ns[ch] : 2000000U; /* 500 Hz */
	uint32_t duty = (uint32_t)(((uint64_t)period * val) / maxv);
	pwm_apply(ch, period, duty);
}

void tone(pin_size_t pin, unsigned int frequency, unsigned long duration)
{
	int ch = pwm_channel_of(pin);
	if (ch < 0 || !PWM_PRESENT || !frequency) {
		return;
	}
	uint32_t period = 1000000000UL / frequency;
	pwm_apply(ch, period, period / 2);
	if (duration) {
		k_msleep(duration);
		pwm_apply(ch, period, 0);
	}
}

void noTone(pin_size_t pin)
{
	int ch = pwm_channel_of(pin);
	if (ch >= 0 && PWM_PRESENT) {
		pwm_apply(ch, 1000000U, 0);
	}
}

/* ---- pulse / shift ------------------------------------------------------- */

unsigned long pulseIn(pin_size_t pin, uint8_t state, unsigned long timeout)
{
	unsigned long start = micros();
	while (digitalRead(pin) == state) {
		if (micros() - start > timeout) return 0;
	}
	while (digitalRead(pin) != state) {
		if (micros() - start > timeout) return 0;
	}
	unsigned long t0 = micros();
	while (digitalRead(pin) == state) {
		if (micros() - start > timeout) return 0;
	}
	return micros() - t0;
}

unsigned long pulseInLong(pin_size_t pin, uint8_t state, unsigned long timeout)
{
	return pulseIn(pin, state, timeout);
}

uint8_t shiftIn(pin_size_t dataPin, pin_size_t clockPin, BitOrder bitOrder)
{
	uint8_t v = 0;
	for (int i = 0; i < 8; ++i) {
		digitalWrite(clockPin, HIGH);
		int bit = digitalRead(dataPin);
		v |= (bitOrder == LSBFIRST) ? bit << i : bit << (7 - i);
		digitalWrite(clockPin, LOW);
	}
	return v;
}

void shiftOut(pin_size_t dataPin, pin_size_t clockPin, BitOrder bitOrder, uint8_t val)
{
	for (int i = 0; i < 8; i++) {
		digitalWrite(dataPin, (bitOrder == LSBFIRST) ? !!(val & (1 << i))
							      : !!(val & (1 << (7 - i))));
		digitalWrite(clockPin, HIGH);
		digitalWrite(clockPin, LOW);
	}
}

/* ---- math ---------------------------------------------------------------- */

static uint32_t rng_state = 0x2545F491u;

static uint32_t rng_next(void)
{
	uint32_t x = rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return rng_state = x;
}

void randomSeed(unsigned long seed)
{
	rng_state = seed ? (uint32_t)seed : 0x2545F491u;
}

long random(long max)
{
	return max <= 0 ? 0 : (long)(rng_next() % (uint32_t)max);
}

long random(long min, long max)
{
	return min >= max ? min : min + random(max - min);
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	long d = in_max - in_min;
	return d ? (x - in_min) * (out_max - out_min) / d + out_min : out_min;
}

uint16_t makeWord(uint16_t w) { return w; }
uint16_t makeWord(uint8_t h, uint8_t l) { return (uint16_t)((h << 8) | l); }

/* ---- startup ------------------------------------------------------------- */

extern "C" void init(void)
{
	randomSeed((unsigned long)k_cycle_get_32());
}

extern "C" __attribute__((weak)) void initVariant(void)
{
}
