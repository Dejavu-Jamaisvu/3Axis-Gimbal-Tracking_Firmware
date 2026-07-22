#pragma once
#include <Arduino.h>

extern volatile int  g_cx;
extern volatile int  g_cy;
extern volatile int  g_area;
extern volatile bool g_detected;

void wifi_init();
void wifi_handle_client();
void print_wifi_ip_periodically();