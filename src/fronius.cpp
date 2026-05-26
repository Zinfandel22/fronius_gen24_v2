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

    Serial.printf("[fronius] SOC fallback GET %s\n", url);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(API_TIMEOUT_MS);

    int code = http.GET();
    Serial.printf("[fronius] SOC response: %d\n", code);
    if (code != 200) {
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();
    Serial.printf("[fronius] SOC body (first 120): %.120s\n", body.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[fronius] SOC JSON error: %s\n", err.c_str());
        return false;
    }

    JsonVariant soc = doc["Body"]["Data"]["0"]["Controller"]["StateOfCharge_Relative"];
    if (soc.isNull()) {
        Serial.println("[fronius] SOC key not found in response");
        return false;
    }

    *soc_out = soc.as<float>();
    Serial.printf("[fronius] SOC from storage endpoint: %.1f%%\n", *soc_out);
    return true;
}

bool fronius_fetch(const char *inverter_ip, PowerData *out) {
    char url[96];
    snprintf(url, sizeof(url),
             "http://%s/solar_api/v1/GetPowerFlowRealtimeData.fcgi", inverter_ip);

    Serial.printf("[fronius] GET %s\n", url);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(API_TIMEOUT_MS);

    int code = http.GET();
    Serial.printf("[fronius] response code: %d\n", code);

    if (code != 200) {
        if (code < 0) {
            Serial.printf("[fronius] connection error: %s\n", http.errorToString(code).c_str());
        }
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();
    Serial.printf("[fronius] body (%d bytes, first 400):\n%.400s\n",
                  body.length(), body.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[fronius] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonObject site = doc["Body"]["Data"]["Site"];
    if (site.isNull()) {
        Serial.println("[fronius] 'Body.Data.Site' not found — printing top-level keys:");
        for (JsonPair kv : doc.as<JsonObject>()) {
            Serial.printf("  key: %s\n", kv.key().c_str());
        }
        return false;
    }

    /* P_PV is null at night */
    out->solar_w = site["P_PV"].isNull() ? 0.0f : site["P_PV"].as<float>();

    /* Fronius returns P_Load as a negative value */
    float load = site["P_Load"].isNull() ? 0.0f : site["P_Load"].as<float>();
    out->consumption_w = fabsf(load);

    /* P_Grid: positive = export, negative = import */
    out->grid_w = site["P_Grid"].isNull() ? 0.0f : site["P_Grid"].as<float>();

    /* P_Akku: negative = charging, positive = discharging */
    out->battery_w = site["P_Akku"].isNull() ? 0.0f : site["P_Akku"].as<float>();

    /* SOC: try three locations in order of preference */
    if (!site["rel_SOC"].isNull()) {
        out->soc_pct = site["rel_SOC"].as<float>();
        Serial.printf("[fronius] SOC from Site/rel_SOC: %.1f%%\n", out->soc_pct);
    } else if (!doc["Body"]["Data"]["Inverters"]["1"]["SOC"].isNull()) {
        out->soc_pct = doc["Body"]["Data"]["Inverters"]["1"]["SOC"].as<float>();
        Serial.printf("[fronius] SOC from Inverters/1/SOC: %.1f%%\n", out->soc_pct);
    } else {
        Serial.println("[fronius] SOC absent from power-flow, trying storage endpoint");
        float soc = -1.0f;
        fetch_soc(inverter_ip, &soc);
        out->soc_pct = soc;
    }

    out->valid        = true;
    out->timestamp_ms = millis();
    return true;
}
