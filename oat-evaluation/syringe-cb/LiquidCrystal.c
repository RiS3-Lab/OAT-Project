#include "lib/LiquidCrystal.h"
#include <stdio.h>


void lcd_begin(LiquidCrystal* lcd, unsigned int cols, unsigned int rows) {
	printf("%s (LiquidCrystal * lcd = 0x%x, unsigned int cols = %u, unsigned int rows = %u)\n", __func__, lcd, cols, rows);
	return;
}

void lcd_clear(LiquidCrystal* lcd) {
	printf("%s (LiquidCrystal * lcd = 0x%x\n", __func__);
	return; //http://stackoverflow.com/questions/10105666/clearing-the-terminal-screen
}

void lcd_print(LiquidCrystal* lcd, char* output, int len) {
	printf("%s (LiquidCrystal * lcd = 0x%x, char* output = %s, int len = %d)\n", __func__, lcd, output, len);
	return;
}

void lcd_setCursor(LiquidCrystal* lcd, int x, int y) { //http://stackoverflow.com/questions/10105666/clearing-the-terminal-screen
	printf("%s (LiquidCrystal * lcd = 0x%x, int x = %d, int y = %d\n", __func__, x, y);
	return;
}
