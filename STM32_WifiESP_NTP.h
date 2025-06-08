/*
 * STM32_WifiESP_NTP.h
 *
 *  Created on: Jun 8, 2025
 *      Author: weedm
 */

#ifndef INC_STM32_WIFIESP_NTP_H_
#define INC_STM32_WIFIESP_NTP_H_

#include "STM32_WifiESP.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ==================== CONFIGURATION ====================

// Active/désactive la gestion RTC depuis NTP (0 ou 1)
#define USE_RTC_FROM_NTP 0

// ==================== ACCES DONNEES NTP ====================

const char *esp01_ntp_get_last_datetime(void);
uint8_t esp01_ntp_is_updated(void);
void esp01_ntp_clear_updated_flag(void);

// ==================== API NTP ====================

ESP01_Status_t esp01_configure_ntp(const char *ntp_server, int timezone_offset, int sync_period_s);
ESP01_Status_t esp01_get_ntp_time(char *datetime_buf, size_t bufsize);

void esp01_ntp_start_periodic_sync(const char *ntp_server, int timezone_offset, uint32_t interval_seconds, bool print_fr);
void esp01_ntp_stop_periodic_sync(void);
void esp01_ntp_periodic_task(void);
void esp01_ntp_sync_once(const char *ntp_server, int timezone_offset, bool print_fr);

// ==================== UTILITAIRES PARSING ====================

void esp01_parse_fr_local_datetime(const char *datetime_ntp, char *out, size_t out_size);
void esp01_parse_local_datetime(const char *datetime_ntp, int timezone_offset, char *out, size_t out_size);

// ==================== AFFICHAGE UTILISATEUR ====================

void esp01_print_fr_local_datetime(const char *datetime_ntp);
void esp01_ntp_sync_and_print(int timezone_offset, bool print_fr, int sync_period_s);

// ==================== RTC OPTIONNEL ====================

#if USE_RTC_FROM_NTP
int esp01_ntp_update_rtc(RTC_HandleTypeDef *hrtc, int timezone_offset);
#endif

#endif /* INC_STM32_WIFIESP_NTP_H_ */
