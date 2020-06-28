#include <stdint.h>
#include <math.h>
#include "delay.h"
#include "ms5611.h"

uint16_t c1, c2, c3, c4, c5, c6;

float press_sea_level;

void ms5611_reset(void)
{
	ms5611_chip_select();
	spi_read_write(SPI3, 0x1e);
	ms5611_chip_deselect();

	blocked_delay_ms(100);
}

void ms5611_read_uint16(uint8_t address, uint16_t *data)
{
	uint8_t byte1, byte2;

	ms5611_chip_select();
	spi_read_write(SPI3, address);
	byte1 = spi_read_write(SPI3, 0x00);
	byte2 = spi_read_write(SPI3, 0x00);
	*data = ((uint16_t)byte1 << 8) | (uint16_t)byte2;
	ms5611_chip_deselect();
}

void ms5611_read_int24(uint8_t address, int32_t *data)
{
	uint8_t byte1, byte2, byte3;

	ms5611_chip_select();
	spi_read_write(SPI3, address);
	ms5611_chip_deselect();
	blocked_delay_ms(1);

	ms5611_chip_select();
	spi_read_write(SPI3, 0x00);
	byte1 = spi_read_write(SPI3, 0x00);
	byte2 = spi_read_write(SPI3, 0x00);
	byte3 = spi_read_write(SPI3, 0x00);
	*data = ((int32_t)byte1 << 16) | ((int32_t)byte2 << 8) | (int32_t)byte3;
	ms5611_chip_deselect();
}

void ms5611_read_prom(void)
{
	ms5611_chip_select();
	ms5611_read_uint16(0xa2, &c1);
	ms5611_read_uint16(0xa4, &c2);
	ms5611_read_uint16(0xa6, &c3);
	ms5611_read_uint16(0xa8, &c4);
	ms5611_read_uint16(0xaa, &c5);
	ms5611_read_uint16(0xac, &c6);
	ms5611_chip_deselect();
}

void ms5611_init(void)
{
	ms5611_reset();
	ms5611_read_prom();
}

void ms5611_read_pressure(float *temp, float *pressure)
{
	int32_t d1, d2;
	int64_t off, sens, dt;

	ms5611_read_int24(0x40, &d1);
	ms5611_read_int24(0x50, &d2);

	dt = (int64_t)d2 - (int64_t)c5 * (1 << 8);
	int32_t temp32 = 2000 + (dt * (int64_t)c6) / (1 << 23);

	/* second order temperature compensation (<20 degree c) */
	int64_t t2 = 0, sens2 = 0, off2 = 0;
	if(temp32 < 2000) {
		t2 = (dt * dt) / (1 << 31);
		int64_t temp_2000_sqrt = temp32 - 2000;
		off2 = 2.5f * (temp_2000_sqrt * temp_2000_sqrt);
		sens2 =  1.25f * (temp_2000_sqrt * temp_2000_sqrt);

		temp32 -= t2;
		off -= off2;
		sens -= sens2;
	}

	off = (int64_t)c2 * (1 << 16) + ((int32_t)c4 * dt) / (1 << 7);
	sens = (int64_t)c1 * (1 << 15) + ((int32_t)c3 * dt) / (1 << 8);
	int32_t pressure32 = ((d1 * sens) / (1 << 21) - off) / (1 << 15);

	*temp = (float)temp32 * 0.01f; //[deg c]
	*pressure = (float)pressure32 * 0.01f; //[mbar]
}

#if 0
float ms5611_get_relative_height(void)
{
	float press_now, temp_now;
	ms5611_read_pressure(&temp_now, &press_now);
	return 44330.0f * (1.0f - pow(press_now / press_sea_level, 0.1902949f));
}

void m5611_set_sea_level(void)
{
	float press, temp;
	ms5611_read_pressure(&temp, &press);
	press_sea_level = press;
}
#endif