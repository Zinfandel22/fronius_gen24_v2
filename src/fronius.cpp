#include "fronius.h"
#include "config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static bool fetch_soc(const char *ip, float *soc_out) {
    char url[96];
    snprintf(url, sizeof(url),
             "http://%s/solar_api/v1/GetStorageRealtimeData.fcgi?Scope=System", ip);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(API_TIMEOUT_MS);

    if (http.GET() != 200) {
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *http.getStreamPtr());
    http.end();
    if (err) return false;

    /* GEN24 with BYD: Body.Data."0".Controller.StateOfCharge_Relative */
    JsonVariant soc = doc["Body"]["Data"]["0"]["Controller"]["StateOfCharge_Relative"];
    if (soc.isNull()) return false;

    *soc_out = soc.as<float>();
    return true;
}

bool fronius_fetch(const char *inverter_ip, PowerData *out) {
    char url[96];
    snprintf(url, sizeof(url),
             "http://%s/solar_api/v1/GetPowerFlowRealtimeData.fcgi", inverter_ip);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(API_TIMEOUT_MS);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, *http.getStreamPtr());
    http.end();
    if (err) return false;

    JsonObject site = doc["Body"]["Data"]["Site"];
    if (site.isNull()) return false;

    /* P_PV is null at night */
    out->solar_w = site["P_PV"].isNull() ? 0.0f : site["P_PV"].as<float>();

    /* Fronius returns P_Load as a negative value (load consumes power) */
    float load = site["P_Load"].isNull() ? 0.0f : site["P_Load"].as<float>();
    out->consumption_w = fabsf(load);

    /* P_Grid: positive = export (feed-in), negative = import (draw) */
    out->grid_w = site["P_Grid"].isNull() ? 0.0f : site["P_Grid"].as<float>();

    /* SOC: present in newer GEN24 firmware as rel_SOC; fall back to storage endpoint */
    if (!site["rel_SOC"].isNull()) {
        out->soc_pct = site["rel_SOC"].as<float>();
    } else {
        float soc = -1.0f;
        fetch_soc(inverter_ip, &soc);
        out->soc_pct = soc;
    }

    out->valid        = true;
    out->timestamp_ms = millis();
    return true;
}
