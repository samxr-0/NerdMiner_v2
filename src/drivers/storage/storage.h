#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <Arduino.h>
#include "../../settings.h"

// config files

// default settings
#ifndef HAN
#define DEFAULT_SSID		"NerdMinerAP"
#else
#define DEFAULT_SSID		"HanSoloAP"
#endif
#define DEFAULT_WIFIPW		"MineYourCoins"
#define DEFAULT_POOLURL		"public-pool.io"
#define DEFAULT_POOLPASS	"x"
#define DEFAULT_WALLETID	"yourBtcAddress"
#define DEFAULT_POOLPORT	21496
#define DEFAULT_TIMEZONE	2
#define DEFAULT_SAVESTATS	false
#define DEFAULT_INVERTCOLORS	false
#define DEFAULT_DISPLAY_ENABLED	false
#define DEFAULT_LED_ENABLED	true

// JSON config files
#define JSON_CONFIG_FILE	"/config.json"
#define JSON_SPIFFS_KEY_SSID	"ssid"
#define JSON_SPIFFS_KEY_PW	"password"
#define JSON_SPIFFS_KEY_POOL_URL	"poolUrl"
#define JSON_SPIFFS_KEY_POOL_PORT	"poolPort"
#define JSON_SPIFFS_KEY_WALLET	"wallet"
#define JSON_SPIFFS_KEY_TIMEZONE	"timezone"
#define JSON_SPIFFS_KEY_SAVE_STATS	"saveStats"
#define JSON_SPIFFS_KEY_INVERT_COLORS	"invertColors"
#define JSON_SPIFFS_KEY_DISPLAY	"displayEnabled"
#define JSON_SPIFFS_KEY_LED	"ledEnabled"

// Logging Verbosity Levels
// Now using the enum from settings.h
// Remove the duplicate enum definition

#endif // _STORAGE_H_