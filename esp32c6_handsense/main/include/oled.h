#ifndef OLED_H
#define OLED_H

#include <stdint.h>

// OLED Configuration
#define OLED_ADDRESS 0x3C

// Function declarations
void oled_init_simple(void);
void oled_clear_screen(void);
void oled_set_cursor(uint8_t page, uint8_t column);
void oled_write_char(char c);
void oled_print_text(uint8_t page, uint8_t column, const char *text);
void oled_update_display(float accel1_x, float accel1_y, float accel1_z,
                         float accel2_x, float accel2_y, float accel2_z,
                         float gyro1_x, float gyro1_y, float gyro1_z,
                         float gyro2_x, float gyro2_y, float gyro2_z,
                         float temp1, float temp2);

#endif // OLED_H