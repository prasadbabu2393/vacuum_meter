// ESP32 Dual Vacuum Measurement System - Part 2: Server Code
// OPTIMIZED: Removed dead code and added OTA authentication.

// Web handler function prototypes
void handleConfig();
void handleSave();
void handleSystemConfig();
void handleSystemSave();
void handleOTAPage();
void handleUpdate();

const char CALIBRATION_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Vacuum Meter Calibration</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; max-width: 600px; margin: 20px auto; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1, h2 { text-align: center; color: #0056b3; }
        .sensor-card { background-color: #fff; border: 1px solid #ddd; padding: 15px; margin-top: 20px; border-radius: 5px; }
        .live-data { font-size: 1.2em; text-align: center; margin-bottom: 15px; }
        .live-data span { font-weight: bold; color: #d9534f; }
        .buttons { display: flex; justify-content: space-around; }
        button { background-color: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 1em; }
        button:hover { background-color: #0056b3; }
        .footer { text-align: center; margin-top: 20px; font-size: 0.9em; color: #777; }
        .nav { margin-top: 20px; text-align: center; }
        .nav a { display:inline-block; padding:10px 15px; background:#007bff; color:white; text-decoration:none; border-radius:5px; margin:5px; }
        .nav a:hover { background:#0056b3; }
    </style>
</head>
<body>
    <h1>Sensor Calibration</h1>
    <p>Instructions: To calibrate, first apply the minimum vacuum level (e.g., 0.001 mBar) and press "Set Minimum". Then, apply the maximum level (atmospheric pressure, ~1000 mBar) and press "Set Maximum".</p>

    <div class="sensor-card">
        <h2>Sensor 1</h2>
        <div class="live-data">Live Voltage: <span id="v1">--.--</span> V</div>
        <div class="buttons">
            <button onclick="calibrate('set_min_1')">Set Minimum (0.001 mBar)</button>
            <button onclick="calibrate('set_max_1')">Set Maximum (1000 mBar)</button>
        </div>
    </div>

    <div class="sensor-card">
        <h2>Sensor 2</h2>
        <div class="live-data">Live Voltage: <span id="v2">--.--</span> V</div>
        <div class="buttons">
            <button onclick="calibrate('set_min_2')">Set Minimum (0.001 mBar)</button>
            <button onclick="calibrate('set_max_2')">Set Maximum (1000 mBar)</button>
        </div>
    </div>
    <div class="nav">
        <a href="/wifi">WiFi Config</a>
        <a href="/system">System Config</a>
        <a href="/update">Firmware Update</a>
    </div>

<script>
    const ws = new WebSocket(`ws://${window.location.hostname}:81/`);
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        document.getElementById('v1').textContent = data.voltage_diff1;
        document.getElementById('v2').textContent = data.voltage_diff2;
    };

    async function calibrate(action) {
        if (!confirm(`Are you sure you want to set this calibration point?`)) return;
        try {
            const response = await fetch('/calibrate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `action=${action}`
            });
            const result = await response.text();
            alert(result);
        } catch (error) {
            alert('Error: Could not connect to the device.');
        }
    }
</script>
</body>
</html>
)rawliteral";

// Handler function to serve the calibration web page
void handleCalibrationPage() {
    server.send(200, "text/html", CALIBRATION_PAGE);
}

// Handler to process calibration commands from the web page
void handleCalibrate() {
    if (!server.hasArg("action")) {
        server.send(400, "text/plain", "Bad Request: Missing action.");
        return;
    }
    String action = server.arg("action");
    String responseMessage = "Calibration failed.";

    if (action == "set_min_1") {
        current_min_voltage = voltage_diff1;
        saveRangeSettings();
        responseMessage = "Sensor 1 Minimum Voltage set to " + String(current_min_voltage) + " V.";
    } else if (action == "set_max_1") {
        current_max_voltage = voltage_diff1;
        saveRangeSettings();
        responseMessage = "Sensor 1 Maximum Voltage set to " + String(current_max_voltage) + " V.";
    } else if (action == "set_min_2") {
        current_min_voltage_2 = voltage_diff2;
        saveRangeSettings_2();
        responseMessage = "Sensor 2 Minimum Voltage set to " + String(current_min_voltage_2) + " V.";
    } else if (action == "set_max_2") {
        current_max_voltage_2 = voltage_diff2;
        saveRangeSettings_2();
        responseMessage = "Sensor 2 Maximum Voltage set to " + String(current_max_voltage_2) + " V.";
    }
    server.send(200, "text/plain", responseMessage);
}

// Setup web server routes
void setupWebServer() {
    server.on("/", handleCalibrationPage);
    server.on("/wifi", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/system", handleSystemConfig);
    server.on("/system-save", HTTP_POST, handleSystemSave);
    server.on("/settings", HTTP_GET, handleGetSettings);

    // NEW: Secured OTA update routes
    server.on("/update", HTTP_GET, []() {
        if (!server.authenticate(ota_username, ota_password)) {
            return server.requestAuthentication();
        }
        handleOTAPage();
    });
    server.on("/update", HTTP_POST, []() {
        if (!server.authenticate(ota_username, ota_password)) {
            return server.requestAuthentication();
        }
        server.send(200, "text/plain", "OK");
        ESP.restart();
    }, handleUpdate);
    
    server.on("/calibration", HTTP_GET, handleCalibrationPage);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    
    server.begin();
    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_DISCONNECTED) {
            Serial.printf("WebSocket client #%u disconnected\n", num);
        }
    });
}

// System configuration page with all baud rates and no SoftAP password
void handleSystemConfig() {
    String html = R"HTML(
<!DOCTYPE html><html><head><title>System Config</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:sans-serif;background:#f4f4f4;margin:20px;}.container{max-width:500px;margin:auto;background:white;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}h1,h3{text-align:center;}input,select{width:100%;padding:10px;margin:10px 0;border-radius:4px;border:1px solid #ddd;box-sizing:border-box;}button{width:100%;background:#007bff;color:white;padding:12px;border:none;border-radius:4px;cursor:pointer;}button:hover{background:#0056b3;}.nav a{text-decoration: none; color: #007bff;}.nav{text-align:center; margin-top: 15px;}</style>
</head><body><div class="container"><h1>System Configuration</h1>
<form action="/system-save" method="post">
<h3>Modbus Settings</h3>
<label>Slave ID (1-247):</label><input type="number" name="slave_id" value=")HTML" + String(modbus_slave_id) + R"HTML(" min="1" max="247">
<label>Baudrate:</label><select name="baudrate">
<option value="1200" )HTML" + (modbus_baudrate == 1200 ? "selected" : "") + R"HTML(">1200</option>
<option value="2400" )HTML" + (modbus_baudrate == 2400 ? "selected" : "") + R"HTML(">2400</option>
<option value="4800" )HTML" + (modbus_baudrate == 4800 ? "selected" : "") + R"HTML(">4800</option>
<option value="9600" )HTML" + (modbus_baudrate == 9600 ? "selected" : "") + R"HTML(">9600</option>
<option value="19200" )HTML" + (modbus_baudrate == 19200 ? "selected" : "") + R"HTML(">19200</option>
<option value="38400" )HTML" + (modbus_baudrate == 38400 ? "selected" : "") + R"HTML(">38400</option>
<option value="57600" )HTML" + (modbus_baudrate == 57600 ? "selected" : "") + R"HTML(">57600</option>
<option value="115200" )HTML" + (modbus_baudrate == 115200 ? "selected" : "") + R"HTML(">115200</option>
</select>
<h3>SoftAP Settings (Open Network)</h3>
<label>SoftAP SSID:</label><input type="text" name="softap_ssid" value=")HTML" + softAP_SSID + R"HTML(">
<button type="submit">Save & Restart</button></form>
<div class="nav"><a href="/">Back to Calibration</a></div>
</div></body></html>
)HTML";
    server.send(200, "text/html", html);
}

void handleSystemSave() {
    if (server.hasArg("slave_id")) { modbus_slave_id = server.arg("slave_id").toInt(); }
    if (server.hasArg("baudrate")) { modbus_baudrate = server.arg("baudrate").toInt(); }
    if (server.hasArg("softap_ssid")) { softAP_SSID = server.arg("softap_ssid"); }
    softAP_Password = ""; // Set password to empty for an open network

    preferences.begin("vac_meter", false);
    preferences.putUChar("modbus_id", modbus_slave_id);
    preferences.putUInt("modbus_baud", modbus_baudrate);
    preferences.putString("ap_ssid", softAP_SSID);
    preferences.putString("ap_pass", softAP_Password);
    preferences.end();
    
    server.send(200, "text/html", "Saving system... restarting.");
    delay(1000);
    ESP.restart();
}

void handleConfig() {
    String html = R"HTML(
<!DOCTYPE html><html><head><title>WiFi Config</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:sans-serif;background:#f4f4f4;margin:20px;}.container{max-width:500px;margin:auto;background:white;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}h1{text-align:center;}input{width:100%;padding:10px;margin:10px 0;border-radius:4px;border:1px solid #ddd;box-sizing:border-box;}button{width:100%;background:#007bff;color:white;padding:12px;border:none;border-radius:4px;cursor:pointer;}button:hover{background:#0056b3;}.nav a{text-decoration: none; color: #007bff;}.nav{text-align:center; margin-top: 15px;}</style>
</head><body><div class="container"><h1>WiFi Configuration</h1>
<form action="/save" method="post">
<label>SSID:</label><input type="text" name="ssid" placeholder="Current: )HTML" + ssid + R"HTML(">
<label>Password:</label><input type="password" name="password">
<label>Static IP (optional):</label><input type="text" name="ip" placeholder="Current: )HTML" + ipStr + R"HTML(">
<label>Gateway (optional):</label><input type="text" name="gateway" placeholder="Current: )HTML" + gatewayStr + R"HTML(">
<label>Subnet (optional):</label><input type="text" name="subnet" placeholder="Current: )HTML" + subnetStr + R"HTML(">
<button type="submit">Save & Restart</button></form>
<div class="nav"><a href="/">Back to Calibration</a></div>
</div></body></html>
)HTML";
    server.send(200, "text/html", html);
}

void handleSave() {
    saveWiFiSettings(server.arg("ssid"), server.arg("password"), server.arg("ip"), server.arg("gateway"), server.arg("subnet"));
    String html = R"HTML(
<!DOCTYPE html><html><head><title>Saving...</title><meta http-equiv="refresh" content="5;url=/">
<style>body{font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;}.container{text-align:center;}</style>
</head><body><div class="container"><h1>Settings Saved</h1><p>Restarting device... You will be redirected shortly.</p></div></body></html>
)HTML";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}

void handleOTAPage() {
    String html = R"HTML(
<!DOCTYPE html><html><head><title>OTA Update</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:sans-serif;background:#f4f4f4;margin:20px;}.container{max-width:500px;margin:auto;background:white;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);}h1{text-align:center;}progress{width:100%;height:25px;}button{background:#007bff;color:white;padding:10px 15px; border:none; border-radius:4px; margin-left: 10px; cursor: pointer;}.nav a{text-decoration: none; color: #007bff;}.nav{text-align:center; margin-top: 15px;}</style>
</head><body><div class="container"><h1>Firmware Update</h1>
<form id="upload_form" method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="update" accept=".bin" required>
<button type="submit">Update Firmware</button></form>
<progress id="prg" value="0" max="100" style="display:none;"></progress>
<p id="status"></p>
<div class="nav"><a href="/">Back to Calibration</a></div>
</div>
<script>
var form=document.getElementById('upload_form');var prg=document.getElementById('prg');var status=document.getElementById('status');
form.addEventListener('submit',function(e){e.preventDefault();var formData=new FormData(form);var xhr=new XMLHttpRequest();
xhr.open('POST','/update');
xhr.upload.addEventListener('progress',function(e){if(e.lengthComputable){var percent=Math.round((e.loaded/e.total)*100);prg.style.display='block';prg.value=percent;}});
xhr.onreadystatechange=function(){if(xhr.readyState==4){if(xhr.status==200){status.innerHTML='Update Success! Rebooting...';setTimeout(function(){window.location.href='/';},5000);}else{status.innerHTML='Update Failed! (Code: '+xhr.status+')';}}};
xhr.send(formData);});
</script></body></html>
)HTML";
    server.send(200, "text/html", html);
}

void handleUpdate() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { Serial.println("Update Success"); } 
        else { Update.printError(Serial); }
    }
}