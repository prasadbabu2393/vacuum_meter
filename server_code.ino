
void startSoftAP() {
    softAPMode = true;
    WiFi.softAP(softAP_SSID, softAP_Password);
    Serial.println("SoftAP Started");
    Serial.println("SSID: " + String(softAP_SSID));
    Serial.println("IP Address: " + WiFi.softAPIP().toString());
}

// Handle voltage range configuration page
void handleConfig_1() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Voltage Range Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }";
    html += ".form-group { margin: 15px 0; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += "input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
    html += "button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; width: 100%; font-size: 16px; }";
    html += "button:hover { background-color: #45a049; }";
    html += ".current-config { background-color: #e8f4fd; padding: 15px; margin: 20px 0; border-radius: 8px; }";
    html += ".back-link { display: inline-block; margin-top: 20px; color: #2196F3; text-decoration: none; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>Voltage Range Configuration</h1>";
    
    html += "<div class='current-config'>";
    html += "<h3>Current Settings</h3>";
    html += "<p><strong>Min Voltage:</strong> " + String(current_min_voltage, 1) + " V</p>";
    html += "<p><strong>Max Voltage:</strong> " + String(current_max_voltage, 1) + " V</p>";
    html += "<p><strong>Dynamic Threshold:</strong> " + String(getDynamicThreshold(), 1) + " V</p>";
    html += "</div>";
    
    html += "<form action='/setconfig' method='POST'>";
    html += "<div class='form-group'>";
    html += "<label for='min_voltage'>Minimum Voltage (V):</label>";
    html += "<input type='number' step='0.1' name='min_voltage' value='" + String(current_min_voltage, 1) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='max_voltage'>Maximum Voltage (V):</label>";
    html += "<input type='number' step='0.1' name='max_voltage' value='" + String(current_max_voltage, 1) + "' required>";
    html += "</div>";
    
    html += "<button type='submit'>Update Voltage Range</button>";
    html += "</form>";
    
    html += "<a href='/' class='back-link'> Back to Main Page</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

// CORRECTED FUNCTION
void handleSetConfig() {
    String message = "";
    bool success = false;

    if (server.hasArg("min_voltage") && server.hasArg("max_voltage")) {
        float new_min = server.arg("min_voltage").toFloat();
        float new_max = server.arg("max_voltage").toFloat();
        
        if (new_max > new_min) {
            current_min_voltage = new_min;
            current_max_voltage = new_max;
            saveRangeSettings();
            message = "<h1> Success!</h1><p>New voltage range has been saved to flash memory.</p>";
            success = true;
        } else {
            message = "<h1> Error!</h1><p>Maximum voltage must be greater than minimum voltage.</p>";
        }
    } else {
        message = "<h1> Error!</h1><p>Missing parameters. Please provide both min and max voltage.</p>";
    }

    String html = "<!DOCTYPE html><html><head><title>Update Status</title>";
    html += "<meta http-equiv='refresh' content='4;url=/ranges'>"; // Redirect after 4 seconds
    html += "<style>body { font-family: Arial, sans-serif; background-color: #f0f0f0; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }";
    html += ".container { text-align: center; padding: 40px; background-color: white; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }";
    html += "h1 { color: " + String(success ? "#28a745" : "#dc3545") + "; }";
    html += "a { color: #007bff; } </style>";
    html += "</head><body><div class='container'>";
    html += message;
    html += "<p>You will be redirected back to the range configuration page in 4 seconds.</p>";
    html += "<p><a href='/ranges'> Click here to go back immediately.</a></p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

// NEW FUNCTION: Handle threshold configuration page
void handleThresholdConfig() {
    String unitStr = getUnitString(currentUnit);
    float hl1_display = convertToSelectedUnit(HL1, currentUnit);
    float ll1_display = convertToSelectedUnit(LL1, currentUnit);
    
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Threshold Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }";
    html += ".form-group { margin: 15px 0; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += "input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
    html += "button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; width: 100%; font-size: 16px; }";
    html += "button:hover { background-color: #45a049; }";
    html += ".current-config { background-color: #e8f4fd; padding: 15px; margin: 20px 0; border-radius: 8px; }";
    html += ".info-box { background-color: #fff3cd; padding: 15px; margin: 20px 0; border-radius: 8px; border: 1px solid #ffeaa7; }";
    html += ".back-link { display: inline-block; margin-top: 20px; color: #2196F3; text-decoration: none; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1> Threshold Configuration (HL-1 & LL-1)</h1>";
    
    html += "<div class='info-box'>";
    html += "<p><strong> Information:</strong></p>";
    html += "<p> <strong>HL-1 (High Limit):</strong> When vacuum exceeds this value, relay turns OFF</p>";
    html += "<p> <strong>LL-1 (Low Limit):</strong> When vacuum goes below this value, relay turns ON</p>";
    html += "<p> Values can also be set using the physical buttons on the device</p>";
    html += "<p> Current unit: <strong>" + unitStr + "</strong></p>";
    html += "</div>";
    
    html += "<div class='current-config'>";
    html += "<h3>Current Settings</h3>";
    html += "<p><strong>HL-1 (High Limit):</strong> " + String(hl1_display, 6) + " " + unitStr + "</p>";
    html += "<p><strong>LL-1 (Low Limit):</strong> " + String(ll1_display, 6) + " " + unitStr + "</p>";
    html += "<p><strong>Current Vacuum:</strong> " + String(convertToSelectedUnit(vacuum1, currentUnit), 6) + " " + unitStr + "</p>";
    html += "<p><strong>Relay Status:</strong> " + String(digitalRead(relay2) ? "ON" : "OFF") + "</p>";
    html += "</div>";
    
    html += "<form action='/setthresholds' method='POST'>";
    html += "<div class='form-group'>";
    html += "<label for='hl1'>HL-1 (High Limit) in " + unitStr + ":</label>";
    html += "<input type='number' step='0.000001' name='hl1' value='" + String(hl1_display, 6) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='ll1'>LL-1 (Low Limit) in " + unitStr + ":</label>";
    html += "<input type='number' step='0.000001' name='ll1' value='" + String(ll1_display, 6) + "' required>";
    html += "</div>";
    
    html += "<button type='submit'> Update Thresholds</button>";
    html += "</form>";
    
    html += "<a href='/' class='back-link'> Back to Main Page</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

// NEW FUNCTION: Handle saving threshold values from web
void handleSetThresholds() {
    String message = "";
    bool success = false;

    if (server.hasArg("hl1") && server.hasArg("ll1")) {
        float new_hl1_display = server.arg("hl1").toFloat();
        float new_ll1_display = server.arg("ll1").toFloat();
        
        // Convert from current display unit back to mBar for internal storage
        float new_hl1_mbar, new_ll1_mbar;
        switch (currentUnit) {
            case UNIT_TORR:
                new_hl1_mbar = new_hl1_display / MBAR_TO_TORR;
                new_ll1_mbar = new_ll1_display / MBAR_TO_TORR;
                break;
            case UNIT_PA:
                new_hl1_mbar = new_hl1_display / MBAR_TO_PA;
                new_ll1_mbar = new_ll1_display / MBAR_TO_PA;
                break;
            case UNIT_MBA:
            default:
                new_hl1_mbar = new_hl1_display;
                new_ll1_mbar = new_ll1_display;
                break;
        }
        
        if (new_hl1_mbar > new_ll1_mbar && new_hl1_mbar > 0 && new_ll1_mbar > 0) {
            // Update internal values
            HL1 = new_hl1_mbar;
            LL1 = new_ll1_mbar;
            
            // Convert to scientific notation for storage (compatible with button interface)
            for (int i = 0; i < 2; i++) {
                float value = (i == 0) ? HL1 : LL1;
                
                // Convert to scientific notation
                int exp = 0;
                float mantissa = value;
                
                if (mantissa != 0) {
                    while (mantissa >= 10.0) {
                        mantissa /= 10.0;
                        exp++;
                    }
                    while (mantissa < 1.0) {
                        mantissa *= 10.0;
                        exp--;
                    }
                }
                
                floatValues[i] = mantissa;
                exponents[i] = exp;
            }
            
            // Save to flash
            saveVacuumSettings();
            
            message = "<h1> Success!</h1><p>New threshold values have been saved to flash memory and are now active.</p>";
            message += "<p><strong>HL-1:</strong> " + String(new_hl1_display, 6) + " " + getUnitString(currentUnit) + "</p>";
            message += "<p><strong>LL-1:</strong> " + String(new_ll1_display, 6) + " " + getUnitString(currentUnit) + "</p>";
            success = true;
            
            Serial.println("Threshold values updated from web interface:");
            Serial.println("HL1: " + String(HL1, 6) + " mBar");
            Serial.println("LL1: " + String(LL1, 6) + " mBar");
        } else {
            message = "<h1> Error!</h1><p>Invalid threshold values. HL-1 must be greater than LL-1, and both must be positive.</p>";
        }
    } else {
        message = "<h1>Error!</h1><p>Missing parameters. Please provide both HL-1 and LL-1 values.</p>";
    }

    String html = "<!DOCTYPE html><html><head><title>Threshold Update Status</title>";
    html += "<meta http-equiv='refresh' content='5;url=/thresholds'>"; // Redirect after 5 seconds
    html += "<style>body { font-family: Arial, sans-serif; background-color: #f0f0f0; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }";
    html += ".container { text-align: center; padding: 40px; background-color: white; border-radius: 10px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }";
    html += "h1 { color: " + String(success ? "#28a745" : "#dc3545") + "; }";
    html += "a { color: #007bff; } </style>";
    html += "</head><body><div class='container'>";
    html += message;
    html += "<p>You will be redirected back to the threshold configuration page in 5 seconds.</p>";
    html += "<p><a href='/thresholds'> Click here to go back immediately.</a></p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleUpdate() {
    HTTPUpload& upload = server.upload();
    
    switch (upload.status) {
        case UPLOAD_FILE_START:
            Serial.printf("Update: %s\n", upload.filename.c_str());
            updateInProgress = true;
            
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
            break;
            
        case UPLOAD_FILE_WRITE:
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            break;
            
        case UPLOAD_FILE_END:
            if (Update.end(true)) {
                Serial.printf("Update Success: %u bytes\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
            updateInProgress = false;
            break;
            
        case UPLOAD_FILE_ABORTED:
            Update.end();
            updateInProgress = false;
            break;
    }
}

// Web handlers
void handleRoot() {
    // Redirect root URL to vacuum display instead of WiFi configuration
    server.sendHeader("Location", "/vacuum", true);
    server.send(302, "text/plain", "");
}


// UPDATED to include Threshold Config button
void handleConfig() {
    String currentSSID = ssid.length() > 0 ? ssid : "Not configured";
    String currentIP = ipStr.length() > 0 ? ipStr : "Not configured";
    String currentGateway = gatewayStr.length() > 0 ? gatewayStr : "Not configured";
    String currentSubnet = subnetStr.length() > 0 ? subnetStr : "Not configured";
    String currentIPAddress = softAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Vacuum Meter Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 15px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
            padding: 40px;
            width: 100%;
            max-width: 500px;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #333;
            font-size: 28px;
            margin-bottom: 10px;
        }
        .form-group {
            margin-bottom: 25px;
        }
        .form-group label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 600;
            font-size: 14px;
        }
        .current-value {
            font-size: 12px;
            color: #666;
            font-style: italic;
            margin-left: 5px;
        }
        .form-group input {
            width: 100%;
            padding: 12px 15px;
            border: 2px solid #e1e5e9;
            border-radius: 8px;
            font-size: 16px;
            transition: all 0.3s ease;
            background: #f8f9fa;
        }
        .form-group input:focus {
            outline: none;
            border-color: #667eea;
            background: white;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }
        .submit-btn {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            margin-top: 20px;
        }
        .submit-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 20px rgba(102, 126, 234, 0.3);
        }
        .current-settings {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 20px;
            border-left: 4px solid #667eea;
        }
        .current-settings h3 {
            color: #333;
            margin-bottom: 15px;
            font-size: 16px;
        }
        .current-settings p {
            color: #666;
            font-size: 14px;
            margin-bottom: 5px;
        }
        .nav-links {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
            gap: 10px;
            margin-top: 20px;
        }
        .nav-link {
            text-align: center;
            padding: 10px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            transition: all 0.3s ease;
        }
        .nav-link:hover {
            background: #5a6fd8;
            transform: translateY(-1px);
        }
        .section-title {
            color: #667eea;
            font-size: 18px;
            font-weight: 600;
            margin: 30px 0 15px 0;
            padding-bottom: 10px;
            border-bottom: 2px solid #e1e5e9;
        }
        .section-title:first-of-type {
            margin-top: 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔧 Vacuum Meter Configuration</h1>
            <p>Configure your ESP32 network settings</p>
        </div>
        
        <div class="current-settings">
            <h3> Current Settings</h3>
            <p><strong>SSID:</strong> )rawliteral" + currentSSID + R"rawliteral(</p>
            <p><strong>Current IP:</strong> )rawliteral" + currentIPAddress + R"rawliteral(</p>
            <p><strong>Static IP:</strong> )rawliteral" + currentIP + R"rawliteral(</p>
            <p><strong>Gateway:</strong> )rawliteral" + currentGateway + R"rawliteral(</p>
            <p><strong>Subnet:</strong> )rawliteral" + currentSubnet + R"rawliteral(</p>
        </div>
        
        <form action="/save" method="post">
            <div class="section-title"> WiFi Credentials</div>
            
            <div class="form-group">
                <label for="ssid">Network Name (SSID) <span class="current-value">Current: )rawliteral" + currentSSID + R"rawliteral(</span></label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter WiFi network name" required>
            </div>
            
            <div class="form-group">
                <label for="password">WiFi Password <span class="current-value">)rawliteral" + (password.length() > 0 ? "Current: ***********" : "Current: Not set") + R"rawliteral(</span></label>
                <input type="password" id="password" name="password" placeholder="Enter WiFi password" required>
            </div>
            
            <div class="section-title"> Network Configuration (Optional)</div>
            
            <div class="form-group">
                <label for="ip">Static IP Address <span class="current-value">Current: )rawliteral" + currentIP + R"rawliteral(</span></label>
                <input type="text" id="ip" name="ip" placeholder="e.g., 192.168.1.100">
            </div>
            
            <div class="form-group">
                <label for="gateway">Gateway Address <span class="current-value">Current: )rawliteral" + currentGateway + R"rawliteral(</span></label>
                <input type="text" id="gateway" name="gateway" placeholder="e.g., 192.168.1.1">
            </div>
            
            <div class="form-group">
                <label for="subnet">Subnet Mask <span class="current-value">Current: )rawliteral" + currentSubnet + R"rawliteral(</span></label>
                <input type="text" id="subnet" name="subnet" placeholder="e.g., 255.255.255.0">
            </div>
            
            <button type="submit" class="submit-btn"> Save Configuration & Restart</button>
        </form>
        
        <div class="nav-links">
            <a href="/vacuum" class="nav-link"> Vacuum</a>
            <a href="/data" class="nav-link"> Live Data</a>
            <a href="/ranges" class="nav-link"> Voltage</a>
            <a href="/thresholds" class="nav-link"> Thresholds</a>
            <a href="/update" class="nav-link"> OTA</a>
        </div>
    </div>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleSave() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    String newIP = server.arg("ip");
    String newGateway = server.arg("gateway");
    String newSubnet = server.arg("subnet");
    
    newSSID.trim();
    newPassword.trim();
    newIP.trim();
    newGateway.trim();
    newSubnet.trim();
    
    saveWiFiSettings(newSSID, newPassword, newIP, newGateway, newSubnet);
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Settings Saved</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 15px;
            padding: 40px;
            text-align: center;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
            max-width: 400px;
            width: 100%;
        }
        .success-icon {
            font-size: 60px;
            margin-bottom: 20px;
        }
        h1 {
            color: #28a745;
            margin-bottom: 20px;
            font-size: 24px;
        }
        p {
            color: #666;
            margin-bottom: 20px;
            font-size: 16px;
        }
        .spinner {
            border: 3px solid #f3f3f3;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 30px;
            height: 30px;
            animation: spin 1s linear infinite;
            margin: 20px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon"></div>
        <h1>Settings Saved Successfully!</h1>
        <p>Your ESP32 is restarting with the new configuration...</p>
        <div class="spinner"></div>
        <p><small>This page will automatically redirect in 5 seconds</small></p>
    </div>
    <script>
        setTimeout(function() {
            window.location.href = '/vacuum';
        }, 5000);
    </script>
</body>
</html>
    )rawliteral";
    
    server.send(200, "text/html", html);
    delay(3000);
    ESP.restart();
}

// UPDATED to include Threshold Config button
void handleVacuumDisplay() {
    float displayVacuum = convertToSelectedUnit(vacuum1, currentUnit);
    String unitStr = getUnitString(currentUnit);
    
    String vacuumReading = String(displayVacuum, 6);
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Vacuum Meter Display</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="2">
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .container {
            background-color: rgba(255, 255, 255, 0.95);
            padding: 40px;
            border-radius: 20px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            text-align: center;
            max-width: 500px;
            width: 90%;
            backdrop-filter: blur(10px);
        }
        h1 {
            color: #333;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.1);
        }
        .vacuum-display {
            background: linear-gradient(45deg, #1e3c72, #2a5298);
            color: white;
            padding: 30px;
            border-radius: 15px;
            margin: 20px 0;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        .vacuum-value {
            font-size: 3.5em;
            font-weight: bold;
            margin: 0;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
        }
        .vacuum-unit {
            font-size: 1.5em;
            margin-top: 10px;
            opacity: 0.9;
        }
        .status-info {
            display: flex;
            justify-content: space-around;
            margin-top: 30px;
            flex-wrap: wrap;
        }
        .status-item {
            background-color: rgba(102, 126, 234, 0.1);
            padding: 15px;
            border-radius: 10px;
            margin: 5px;
            flex: 1;
            min-width: 120px;
            border: 1px solid rgba(102, 126, 234, 0.3);
        }
        .status-label {
            font-size: 0.9em;
            color: #666;
            margin-bottom: 5px;
        }
        .status-value {
            font-size: 1.2em;
            font-weight: bold;
            color: #333;
        }
        .relay-status {
            margin-top: 20px;
            padding: 15px;
            border-radius: 10px;
            font-weight: bold;
        }
        .relay-on {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .relay-off {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .navigation {
            margin-top: 30px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
            gap: 10px;
        }
        .nav-button {
            background: linear-gradient(45deg, #667eea, #764ba2);
            color: white;
            border: none;
            padding: 12px 15px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 0.9em;
            text-decoration: none;
            display: inline-block;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        .nav-button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0, 0, 0, 0.3);
        }
        .error-status {
            background-color: #f8d7da;
            color: #721c24;
            padding: 15px;
            border-radius: 10px;
            margin: 20px 0;
            border: 1px solid #f5c6cb;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1> Vacuum Meter</h1>
        
        %ERROR_DISPLAY%
        
        <div class="vacuum-display">
            <div class="vacuum-value">%VACUUM_VALUE%</div>
            <div class="vacuum-unit">%VACUUM_UNIT%</div>
        </div>
        
        <div class="status-info">
            <div class="status-item">
                <div class="status-label">High Limit</div>
                <div class="status-value">%HL1_VALUE%</div>
            </div>
            <div class="status-item">
                <div class="status-label">Low Limit</div>
                <div class="status-value">%LL1_VALUE%</div>
            </div>
        </div>
        
        <div class="relay-status %RELAY_CLASS%">
            Relay Status: %RELAY_STATUS%
        </div>
        
        <div class="navigation">
            <a href="/config" class="nav-button"> Config</a>
            <a href="/thresholds" class="nav-button"> Thresholds</a>
            <a href="/ranges" class="nav-button"> Voltage</a>
            <a href="/data" class="nav-button"> Live Data</a>
            <a href="/update" class="nav-button"> OTA</a>
        </div>
        
        <p style="margin-top: 30px; color: #666; font-size: 0.9em;">
            Auto-refreshing every 2 seconds<br>
            Last updated: %TIMESTAMP%
        </p>
    </div>
</body>
</html>
)rawliteral";

    // Check for sensor error
    String errorDisplay = "";
    if (voltage_diff <= -100 || voltage_diff > 1200) {
        errorDisplay = "<div class=\"error-status\"> SENSOR FAILURE DETECTED</div>";
        vacuumReading = "FAIL";
        unitStr = "";
    }

    html.replace("%ERROR_DISPLAY%", errorDisplay);
    html.replace("%VACUUM_VALUE%", vacuumReading);
    html.replace("%VACUUM_UNIT%", unitStr);
    
    // Format limit values
    float hl1_display = convertToSelectedUnit(HL1, currentUnit);
    float ll1_display = convertToSelectedUnit(LL1, currentUnit);
    
    String hl1_str = String(hl1_display, 6);
    String ll1_str = String(ll1_display, 6);
    
    html.replace("%HL1_VALUE%", hl1_str + " " + unitStr);
    html.replace("%LL1_VALUE%", ll1_str + " " + unitStr);
    
    // Relay status
    bool relayState = digitalRead(relay2);
    html.replace("%RELAY_STATUS%", relayState ? "ON" : "OFF");
    html.replace("%RELAY_CLASS%", relayState ? "relay-on" : "relay-off");
    
    html.replace("%TIMESTAMP%", String(millis() / 1000) + "s");
    
    server.send(200, "text/html", html);
}

// UPDATED to include Threshold Config button
void handleDataPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Vacuum Sensor Live Data</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 15px;
            padding: 30px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #333;
            font-size: 28px;
            margin-bottom: 10px;
        }
        .voltage-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .voltage-card {
            background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
            border-radius: 10px;
            padding: 20px;
            text-align: center;
            border: 2px solid #e1e5e9;
            transition: all 0.3s ease;
        }
        .voltage-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 20px rgba(0,0,0,0.1);
        }
        .channel-name {
            font-size: 14px;
            color: #666;
            margin-bottom: 10px;
            font-weight: 600;
        }
        .voltage-value {
            font-size: 24px;
            font-weight: bold;
            color: #333;
            margin-bottom: 5px;
        }
        .status {
            padding: 10px;
            border-radius: 8px;
            margin-bottom: 20px;
            text-align: center;
        }
        .status.connected {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.disconnected {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .nav-button {
            display: inline-block;
            padding: 12px 16px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 8px;
            transition: all 0.3s ease;
            font-weight: 600;
            margin: 5px;
            font-size: 14px;
        }
        .nav-button:hover {
            background: #5a6fd8;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.3);
        }
        .button-container {
            text-align: center;
            margin-top: 20px;
        }
    </style>
    <script>
        var ws;
        var wsConnected = false;
        
        function connectWebSocket() {
            ws = new WebSocket("ws://" + location.hostname + ":81/");
            
            ws.onopen = function() {
                wsConnected = true;
                document.getElementById("status").innerHTML = " Connected - Receiving live data";
                document.getElementById("status").className = "status connected";
            };
            
            ws.onmessage = function(event) {
                var data = JSON.parse(event.data);
                document.getElementById("ch0").innerText = data.voltage1.toFixed(3) + " mV";
                document.getElementById("ch1").innerText = data.voltage2.toFixed(3) + " mV";
                document.getElementById("ch2").innerText = data.voltage3.toFixed(3) + " mV";
                document.getElementById("ch3").innerText = data.voltage_diff.toFixed(3) + " mV";
                document.getElementById("vacuum").innerText = data.vacuum.toFixed(6) + " " + data.unit;
            };
            
            ws.onclose = function() {
                wsConnected = false;
                document.getElementById("status").innerHTML = " Disconnected - Attempting to reconnect...";
                document.getElementById("status").className = "status disconnected";
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = function() {
                wsConnected = false;
                document.getElementById("status").innerHTML = " Connection Error - Retrying...";
                document.getElementById("status").className = "status disconnected";
            };
        }
        
        window.onload = function() {
            connectWebSocket();
        };
    </script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1> Vacuum Sensor Live Data</h1>
            <div id="status" class="status disconnected"> Connecting...</div>
        </div>
        
        <div class="voltage-grid">
            <div class="voltage-card">
                <div class="channel-name">Voltage 1 (ADC0)</div>
                <div class="voltage-value" id="ch0">0.000 mV</div>
            </div>
            <div class="voltage-card">
                <div class="channel-name">Voltage 2 (ADC1)</div>
                <div class="voltage-value" id="ch1">0.000 mV</div>
            </div>
            <div class="voltage-card">
                <div class="channel-name">Voltage 3 (ADC2)</div>
                <div class="voltage-value" id="ch2">0.000 mV</div>
            </div>
            <div class="voltage-card">
                <div class="channel-name">Voltage Difference</div>
                <div class="voltage-value" id="ch3">0.000 mV</div>
            </div>
            <div class="voltage-card" style="grid-column: span 2;">
                <div class="channel-name">Vacuum Reading</div>
                <div class="voltage-value" id="vacuum">0.000000 mBar</div>
            </div>
        </div>
        
        <div class="button-container">
            <a href="/config" class="nav-button"> WiFi Config</a>
            <a href="/thresholds" class="nav-button"> Thresholds</a>
            <a href="/ranges" class="nav-button"> Voltage Config</a>
            <a href="/vacuum" class="nav-button"> Vacuum Display</a>
            <a href="/update" class="nav-button"> OTA Update</a>
        </div>
    </div>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleOTAPage() {
    if (!server.authenticate(ota_username, ota_password)) {
        return server.requestAuthentication();
    }
    
    String currentIPAddress = softAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Vacuum Meter OTA Update</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .container {
      background: white;
      border-radius: 15px;
      padding: 40px;
      box-shadow: 0 20px 40px rgba(0,0,0,0.1);
      max-width: 500px;
      width: 100%;
    }
    h1 {
      color: #333;
      margin-top: 0;
      text-align: center;
      font-size: 28px;
      margin-bottom: 10px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    .btn {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      padding: 15px 25px;
      border-radius: 8px;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      transition: all 0.3s ease;
      width: 100%;
    }
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 20px rgba(102, 126, 234, 0.3);
    }
    .btn:disabled {
      background: #95a5a6;
      cursor: not-allowed;
      transform: none;
      box-shadow: none;
    }
    input[type="file"] {
      width: 100%;
      padding: 15px;
      border: 2px dashed #667eea;
      border-radius: 8px;
      background: #f8f9fa;
      cursor: pointer;
      transition: all 0.3s ease;
    }
    input[type="file"]:hover {
      border-color: #5a6fd8;
      background: #e3f2fd;
    }
    #progress-container {
      display: none;
      margin-top: 25px;
    }
    .progress-bar {
      height: 25px;
      background-color: #ecf0f1;
      border-radius: 12px;
      margin-bottom: 15px;
      overflow: hidden;
      box-shadow: inset 0 2px 4px rgba(0,0,0,0.1);
    }
    .progress-fill {
      height: 100%;
      background: linear-gradient(135deg, #2ecc71 0%, #27ae60 100%);
      width: 0%;
      transition: width 0.3s ease;
      display: flex;
      align-items: center;
      justify-content: center;
      color: white;
      font-weight: bold;
      font-size: 12px;
    }
    .status {
      font-weight: bold;
      text-align: center;
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 15px;
    }
    .success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .info {
      background: #e3f2fd;
      border: 1px solid #bbdefb;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 25px;
      color: #1976d2;
      font-size: 14px;
    }
    .device-info {
      background: #f8f9fa;
      border-radius: 8px;
      padding: 20px;
      margin-top: 30px;
      font-size: 14px;
      color: #666;
    }
    .device-info p {
      margin: 8px 0;
    }
    .back-link {
      display: inline-block;
      margin-top: 10px;
      text-align: center;
      color: #667eea;
      text-decoration: none;
      font-weight: 600;
      width: 100%;
      padding: 12px;
      border: 2px solid #667eea;
      border-radius: 8px;
      transition: all 0.3s ease;
    }
    .back-link:hover {
      background: #667eea;
      color: white;
      transform: translateY(-1px);
    }
  </style>
</head>
<body>
  <div class="container">
    <h1> Vacuum Meter OTA Update</h1>
    
    <div class="info">
      <p><strong> Upload Instructions:</strong></p>
      <p>• Select a compiled firmware file (.bin format)</p>
      <p>• File will be uploaded and flashed automatically</p>
      <p>• Device will restart after successful update</p>
      <p>• Do not disconnect power during the update process</p>
    </div>
    
    <form id="upload-form" method="POST" action="/update" enctype="multipart/form-data">
      <div class="form-group">
        <input type="file" name="update" id="firmware-file" accept=".bin" required>
      </div>
      <button type="submit" class="btn" id="upload-btn"> Upload & Update Firmware</button>
    </form>
    
    <div id="progress-container">
      <p class="status" id="status-text">Uploading firmware...</p>
      <div class="progress-bar">
        <div class="progress-fill" id="progress">0%</div>
      </div>
      <p id="progress-text" style="text-align: center; color: #666;">0%</p>
    </div>

    <div class="device-info">
      <p><strong>Device Information:</strong></p>
      <p>Device: ESP32 Vacuum Meter</p>
      <p>Current IP: )rawliteral" + currentIPAddress + R"rawliteral(</p>
      <p>Mode: )rawliteral" + (softAPMode ? "SoftAP" : "WiFi Station") + R"rawliteral(</p>
    </div>
    
    <a href="/vacuum" class="back-link"> Back to Vacuum Display</a>
  </div>

  <script>
    function updateHeapInfo() {
      fetch('/heap')
        .then(response => response.text())
        .then(data => {
          document.getElementById('heap').textContent = data;
        })
        .catch(() => {
          document.getElementById('heap').textContent = 'N/A';
        });
    }
    
    updateHeapInfo();
    setInterval(updateHeapInfo, 5000);

    document.getElementById('upload-form').addEventListener('submit', function(e) {
      const fileInput = document.getElementById('firmware-file');
      if (!fileInput.files.length) {
        e.preventDefault();
        alert('Please select a firmware file');
        return;
      }
      
      const fileName = fileInput.files[0].name;
      if (!fileName.toLowerCase().endsWith('.bin')) {
        e.preventDefault();
        alert('Please select a valid .bin firmware file');
        return;
      }
      
      document.getElementById('upload-btn').disabled = true;
      document.getElementById('progress-container').style.display = 'block';
      
      const xhr = new XMLHttpRequest();
      const formData = new FormData(this);
      
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          const percentComplete = Math.round((e.loaded / e.total) * 100);
          const progressBar = document.getElementById('progress');
          const progressText = document.getElementById('progress-text');
          
          progressBar.style.width = percentComplete + '%';
          progressBar.textContent = percentComplete + '%';
          progressText.textContent = percentComplete + '% (' + 
            Math.round(e.loaded/1024) + ' KB / ' + Math.round(e.total/1024) + ' KB)';
        }
      });
      
      xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
          document.getElementById('status-text').textContent = ' Update successful! Device is restarting...';
          document.getElementById('status-text').className = 'status success';
          
          setTimeout(function() {
            window.location.href = '/vacuum';
          }, 10000);
        } else {
          document.getElementById('status-text').textContent = ' Update failed! Please try again.';
          document.getElementById('status-text').className = 'status error';
          document.getElementById('upload-btn').disabled = false;
        }
      });
      
      xhr.addEventListener('error', function() {
        document.getElementById('status-text').textContent = ' Connection error! Please try again.';
        document.getElementById('status-text').className = 'status error';
        document.getElementById('upload-btn').disabled = false;
      });
      
      xhr.open('POST', '/update', true);
      xhr.send(formData);
      e.preventDefault();
    });
  </script>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
}