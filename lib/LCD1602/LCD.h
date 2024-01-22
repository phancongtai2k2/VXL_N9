
/**
 * @file LCD.h
 * @author LeThanhLuan_HUST (luan172002@gmail.com)
 * @brief LCD1602 (HD44780) with module i2c
 * @version 0.1
 * @date 2023-12-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _LCD_H
#define _LCD_H

#pragma once

// initialize lcd
void lcd_init (void);   
// send command to the lcd
void lcd_send_cmd (char cmd);  
// send data to the lcd
void lcd_send_data (char data);  
// send string to the lcd
void lcd_send_string (char *str);  
 // put cursor at the entered position row (0 or 1), col (0-15);
void lcd_put_cur(int row, int col); 
 // clear lcd
void lcd_clear (void);

#endif