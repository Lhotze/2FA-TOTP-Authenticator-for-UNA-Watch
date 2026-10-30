/**
 ******************************************************************************
 * @file    AppConfigFields.cpp
 * @brief   The app's copy of the configuration contract in its app-manifest.json: the one-shot form in which a secret key can be pasted on the phone.
 ******************************************************************************
 */

#include <gui/model/AppConfigFields.hpp>

using SDK::AppConfig;

const char *const kConfigFile = "app_config.json";

// Every value here must match app-manifest.json exactly. CI checks it with validate_app_config.py --check app-manifest.json --check-bounds <this file>.
const AppConfig::Field kConfigFields[6] = {
    AppConfig::stringField("newCode", "", 0, 128),
    AppConfig::stringField("newName", "", 0, 27),
    AppConfig::stringField("newIssuer", "", 0, 23),
    AppConfig::intField("newDigits", 6, 6, 8),
    AppConfig::intField("newPeriod", 30, 10, 120),
    AppConfig::stringField("newAlgo", "SHA1", 0, 6),
};
