#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

void wifi_manager_init(void);
void wifi_manager_set_enabled(bool enable);
void wifi_manager_set_config(const char *ssid, const char *password);
bool wifi_manager_get_saved_config(char *ssid, size_t max_len);
bool wifi_manager_get_ip(char *ip_str, size_t max_len);

#endif // WIFI_MANAGER_H
