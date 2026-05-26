# fronius_gen24_v2
Fronius monitoring with a Waveshare ESP32-S3 AMOLED 1.75" touch display

Open the serial monitor at 115200. You should see output like:
[boot] Fronius Gen24 Monitor starting
[boot] display init done
[boot] UI init done
[wifi] stored inverter IP: '192.168.x.x'
[wifi] connected, local IP: 192.168.x.x
[wifi] inverter IP to use: '192.168.x.x'
[fronius] task started, polling 192.168.x.x every 5000 ms
[fronius] GET http://192.168.x.x/solar_api/v1/GetPowerFlowRealtimeData.fcgi
[fronius] response code: 200
[fronius] OK  solar=2340 W  load=850 W  grid=+1490 W  soc=78%

The key things the serial output will reveal:

What you see	Diagnosis
response code: -1 or connection error	Wrong IP, inverter unreachable, or WiFi dropped
response code: 200 but JSON parse error	Fronius returned HTML/error page instead of JSON
'Body.Data.Site' not found + key dump	API response structure different — keys printed will show the real layout
WARN mutex take timed out	FreeRTOS scheduling issue — unlikely but possible
data state changed → VALID never appears	Fetch always failing — look at the fronius lines above it