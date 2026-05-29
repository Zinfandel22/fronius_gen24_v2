#pragma once
#include <stdint.h>
#include <stdbool.h>

struct PowerData {
    float    solar_w;        /* PV production, W — 0 at night               */
    float    inverter_w;     /* Inverter AC output, W — PV + battery         */
    float    consumption_w;  /* House load, W — always positive              */
    float    grid_w;         /* + = export to grid, − = import from grid     */
    float    battery_w;      /* P_Akku: − = charging, + = discharging        */
    float    soc_pct;        /* Battery state of charge 0–100; −1 = unknown  */
    float    phase_w[3];   /* per-phase grid: +export / −import; index 0=L1,1=L2,2=L3 */
    bool     phase_valid;  /* true when GetMeterRealtimeData succeeded */
    bool     valid;
    uint32_t timestamp_ms;
};

/*
 * Fetch /solar_api/v1/GetPowerFlowRealtimeData.fcgi.
 * Falls back to GetStorageRealtimeData.fcgi if SOC is absent in power-flow.
 * Returns true on success and fills *out.
 */
bool fronius_fetch(const char *inverter_ip, PowerData *out);
