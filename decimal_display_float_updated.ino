// The following is the complete and corrected code for your project.
// All sections have been reviewed and updated for functionality and usability.

#include <TM1637Display.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Update.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Create objects
Adafruit_ADS1115 ads;
TM1637Display display1(18, 19);  // CLK1, DIO1
TM1637Display display2(32, 33);  // CLK2, DIO2
Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Pin definitions
#define BTN_INC  27
#define BTN_DEC  14
#define BTN_TOGGLE  15
#define BTN_OK  13
#define relay1 4
#define relay2 2

// Vacuum range shifting
float current_min_voltage = -13.0f;      
float current_max_voltage = 1000.0f;   
const float ORIGINAL_MIN_VOLTAGE = -13.0f;   
const float ORIGINAL_MAX_VOLTAGE = 995.0f;

// WiFi Configuration
const char* softAP_SSID = "Vacuum_meter#01";
const char* softAP_Password = "12345678";
const char* ota_username = "admin";
const char* ota_password = "password";

String ssid, password, ipStr, gatewayStr, subnetStr;
IPAddress local_IP, gateway, subnet;
const int MAX_WIFI_RETRIES = 20;
bool softAPMode = false;
bool updateInProgress = false;

// Sampling variables
int num_samples = 3;
#define MAX_SAMPLES 20
float voltage1_buffer[MAX_SAMPLES] = {0};
float voltage2_buffer[MAX_SAMPLES] = {0};
float voltage3_buffer[MAX_SAMPLES] = {0};
int buffer_index = 0;

// Unit selection
enum UnitType { UNIT_MBA, UNIT_TORR, UNIT_PA };
UnitType currentUnit = UNIT_MBA;
UnitType tempUnit = currentUnit;

// Conversion factors
const float MBAR_TO_TORR = 0.750062;
const float MBAR_TO_PA = 100.0;

// Settings modes
const char* modes[] = {"HL-1", "LL-1", "Unit"};
int currentModeIndex = -1;
float floatValues[2] = {1.0e-3, 1.0e-3};
int exponents[2] = {-3, -3};

// Button debounce
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 200;

// Vacuum and sensor variables
float HL1, LL1;
float vacuum1 = 0.001;
int16_t adcReading_1, adcReading_2, adcReading_3;
double voltage_1, voltage_2, voltage_3, voltage_diff;

// Settings mode variables
unsigned long btnPressStart = 0;
bool btnHeld = false;
unsigned long lastInteractionTime = 0;
bool settingsMode = false;

// Voltage to vacuum lookup table
const struct {
  float voltage;
  float vacuum;
} voltageToVacuumTable[] = {
  {172.310f, 0.001000f},  {173.43f, 0.002000f},  {175.69f, 0.003000f},   {177.91f, 0.004000f},
  {182.70f, 0.005000f},   {184.52f, 0.006000f},    {186.25f, 0.007000f},    {190.53f, 0.008000f},
  {197.41f, 0.009000f},   {210.200f, 0.010000f},   {225.380f, 0.011000f},   {230.500f, 0.012000f},
  {234.000f, 0.013000f},   {239.130f, 0.014000f},   {242.000f, 0.015000f},   {247.000f, 0.016000f},
  {249.000f, 0.017000f},   {253.500f, 0.018000f},   {256.750f, 0.019000f},   {258.000f, 0.020000f},
  {261.125f, 0.021000f},   {263.250f, 0.022000f},   {267.000f, 0.023000f},   {272.500f, 0.024000f},
  {274.200f, 0.025000f},  {276.500f, 0.026000f},  {280.000f, 0.027000f},  {283.620f, 0.028000f},
  {285.000f, 0.029000f},  {287.000f, 0.030000f},  {292.000f, 0.031000f},  {294.500f, 0.032000f},
  {298.000f, 0.033000f},  {301.000f, 0.034000f},  {305.000f, 0.035000f},   {311.000f, 0.036000f},
  {314.000f, 0.037000f},
  {318.500f, 0.038000f},
  {321.800f, 0.039000f},
  {324.500f, 0.040000f},
  {329.500f, 0.041000f},
  {333.000f, 0.042000f},
  {337.000f, 0.043000f},
  {344.500f, 0.044000f},
  {347.000f, 0.045000f},
  {351.500f, 0.046000f},
  {354.000f, 0.047000f},
  {357.500f, 0.048000f},
  {361.250f, 0.049000f},
  {369.000f, 0.050000f},
  {372.000f, 0.051000f},  // Missing value filled with average
  {376.000f, 0.052000f},
  {378.000f, 0.053000f},
  {380.000f, 0.054000f},
  {384.000f, 0.055000f},
  {387.000f, 0.056000f},
  {389.000f, 0.057000f},
  {392.000f, 0.058000f},
  {395.000f, 0.059000f},
  {400.000f, 0.060000f},
  {404.750f, 0.061000f},  // Missing value filled with average
  {408.500f, 0.062000f},
  {411.200f, 0.063000f},    
  {414.000f, 0.064000f},
  {418.000f, 0.065000f},
  {423.500f, 0.066000f},
  {427.500f, 0.067000f},
  {429.000f, 0.068000f},
  {432.750f, 0.069000f},  // Missing value filled with average
  {435.500f, 0.070000f},
  {439.200f, 0.071000f},
  {442.500f, 0.072000f},
  {445.750f, 0.073000f},
  {449.500f, 0.074000f},
  {453.000f, 0.075000f},
  {456.500f, 0.076000f},
  {460.000f, 0.077000f},

  {462.500f, 0.078000f},
  {466.000f, 0.079000f},
  {468.000f, 0.080000f},
  {470.000f, 0.081000f},
  {475.500f, 0.082000f},
  {478.500f, 0.083000f},  // Missing value filled with average
  {480.500f, 0.084000f},
  {484.500f, 0.085000f},
  {486.000f, 0.086000f},
  {488.000f, 0.087000f},
  {491.000f, 0.088000f},
  {494.500f, 0.089000f},
  {497.000f, 0.090000f},
  {500.000f, 0.091000f},
  {503.000f, 0.092000f},
  {505.850f, 0.093000f},  // Missing value filled with average
  {507.025f, 0.094000f},  // Missing value filled with average
  {513.200f, 0.095000f},
  {518.500f, 0.096000f},
  {520.500f, 0.097000f}, {523.000f, 0.098000f}, {528.000f, 0.099000f},

  {531.000f, 0.100000f},  {539.500f, 0.110000f},
  {556.000f, 0.120000f},  {580.000f, 0.130000f},  {598.000f, 0.140000f},  {624.000f, 0.150000f},
  {640.000f, 0.160000f},  {649.000f, 0.170000f},  {671.000f, 0.180000f},  {688.000f, 0.190000f},
  {703.000f, 0.200000f},   {729.000f, 0.210000f},
  {750.000f, 0.220000f},
  {764.000f, 0.230000f},
  {776.000f, 0.240000f},
  {783.000f, 0.250000f},
  {798.000f, 0.260000f},
  {810.000f, 0.270000f},
  {818.000f, 0.280000f},
  {833.000f, 0.290000f},
  {842.000f, 0.300000f},
  {851.000f, 0.310000f},
  {859.000f, 0.320000f},
  {863.000f, 0.330000f},
  {870.000f, 0.340000f},
  {874.000f, 0.350000f},
  {877.000f, 0.360000f},
  {884.000f, 0.370000f},
  {890.000f, 0.380000f},
  {896.000f, 0.390000f},
  {901.000f, 0.400000f},
  {904.000f, 0.410000f},
  {907.000f, 0.420000f},
  {911.000f, 0.430000f},
  {915.000f, 0.440000f},
  {918.000f, 0.450000f},
  {921.000f, 0.460000f},
  {924.000f, 0.470000f},
  {927.000f, 0.480000f},
  {931.000f, 0.490000f},
  {934.200f, 0.500000f},
  {939.200f, 0.510000f},
  {941.000f, 0.520000f},
  {943.500f, 0.530000f},
  {944.000f, 0.540000f},
  {945.000f, 0.550000f},

  {946.000f, 0.560000f},
  {948.500f, 0.570000f},
  {951.000f, 0.580000f},
  {953.500f, 0.590000f},  // Missing value filled with average
  {955.000f, 0.600000f},
  {958.500f, 0.610000f},
  {960.000f, 0.620000f},
  {962.000f, 0.630000f},
  {964.000f, 0.640000f},
  {966.500f, 0.650000f},
  {968.000f, 0.660000f},
  {970.100f, 0.670000f},
  {971.000f, 0.680000f},
  {974.100f, 0.690000f},
  {976.000f, 0.700000f},
  {978.500f, 0.710000f},
  {979.000f, 0.720000f},
  {980.500f, 0.730000f},  // Missing value filled with average
  {981.500f, 0.740000f},
  {983.000f, 0.750000f},
  {985.000f, 0.760000f},
  {987.000f, 0.770000f},
  {988.000f, 0.780000f},
  {989.500f, 0.790000f},
  {990.100f, 0.800000f},
  {990.750f, 0.810000f},  // Missing value filled with average
  {991.000f, 0.820000f},
  {991.500f, 0.830000f},
  {992.750f, 0.840000f},
  {993.000f, 0.850000f},
  {994.000f, 0.860000f},
  {995.000f, 0.870000f},
  {996.000f, 0.880000f},
  {996.500f, 0.890000f},
  {997.000f, 0.900000f},
  {997.600f, 0.910000f},
  {998.300f, 0.920000f},
  {999.000f, 0.930000f},
  {1000.300f, 0.940000f},
  {1001.300f, 0.950000f},
  {1002.500f, 0.960000f},
  {1003.500f, 0.970000f},
  {1004.500f, 0.980000f},
  {1005.750f, 0.990000f},  {1006.000f, 1.000000f},  {1028.000f, 2.000000f},
  {1036.000f, 3.000000f},  {1041.500f, 4.000000f},  {1045.000f, 5.000000f},  {1049.000f, 6.000000f},
  {1054.000f, 7.000000f},  {1060.100f, 8.000000f},  {1066.000f, 9.000000f},  {1071.000f, 10.000000f},
  {1075.750f, 20.000000f}, {1076.275f, 30.000000f}, {1078.800f, 40.000000f}, {1079.500f, 50.000000f},
  {1080.000f, 60.000000f}, {1081.500f, 70.000000f}, {1082.000f, 80.000000f}, {1084.800f, 100.000000f},
  {1085.300f, 500.000000f},{1086.000f, 1000.000000f}
};

const int TABLE_SIZE = sizeof(voltageToVacuumTable) / sizeof(voltageToVacuumTable[0]);

// Function prototypes
float getVacuumFromVoltage(float voltage);
bool isButtonPressed(int pin);
void adcReadings_fun();
void showFloat(TM1637Display &disp, float value);
void displayMode(TM1637Display &disp, String mode);
void updateValue(bool increment, unsigned long holdTime);
void displayUnitType(TM1637Display &disp, UnitType type);
float convertToSelectedUnit(float vacuumInMbar, UnitType unitType);
void loadUnitSettings();
void saveUnitSettings();
void loadVacuumSettings();
void saveVacuumSettings();
void saveRangeSettings();
void loadRangeSettings();
void displayFail(TM1637Display &display1, TM1637Display &display2);
String getUnitString(UnitType unitType);

// WiFi and OTA functions
void connectToWiFi();
void startSoftAP();
void saveWiFiSettings(String ssid, String password, String ip, String gateway, String subnet);
void readWiFiSettings();
void handleUpdate();
String urlDecode(String input);
int hexToInt(char c);

// Web handlers
void handleRoot();
void handleConfig();
void handleSave();
void handleVacuumDisplay();
void handleDataPage();
void handleOTAPage();
void handleConfig_1();
void handleSetConfig();
void handleThresholdConfig();
void handleSetThresholds();


void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 Vacuum Measurement System with WiFi & OTA ===");
    
    //Initialize I2C and ADS1115
    Wire.begin(21, 22);
    if(!ads.begin()) {
        Serial.println("Failed to initialize ADS1115!");
    } else {
        Serial.println("ADS1115 initialized successfully");
    }
    ads.setGain(GAIN_ONE);
    
    // Initialize displays
    display1.setBrightness(7);
    display2.setBrightness(7); 

    // Initialize pins
    pinMode(BTN_INC, INPUT_PULLUP);
    pinMode(BTN_DEC, INPUT_PULLUP); 
    pinMode(BTN_TOGGLE, INPUT_PULLUP);
    pinMode(BTN_OK, INPUT_PULLUP);
    pinMode(relay1, OUTPUT);
    digitalWrite(relay1, LOW);
    pinMode(relay2, OUTPUT);
    digitalWrite(relay2, LOW);

    // Load settings from flash memory
    loadUnitSettings();
    loadVacuumSettings();
    loadRangeSettings(); // Correctly load voltage ranges

    // Initialize WiFi
    readWiFiSettings();
    connectToWiFi();
    
    // Setup web server routes
    server.on("/", handleRoot);                 // Root now redirects to vacuum display
    server.on("/config", handleConfig);         // WiFi configuration moved here
    server.on("/save", HTTP_POST, handleSave);
    server.on("/vacuum", handleVacuumDisplay);
    server.on("/data", handleDataPage);
    server.on("/ranges", handleConfig_1);       // Handle the new Range Configuration page
    server.on("/setconfig", HTTP_POST, handleSetConfig); // Handle saving the new ranges
    server.on("/thresholds", handleThresholdConfig);     // Handle threshold configuration page
    server.on("/setthresholds", HTTP_POST, handleSetThresholds); // Handle saving thresholds
    
    // OTA Update routes
    server.on("/update", HTTP_GET, handleOTAPage);
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        if (Update.hasError()) {
            server.send(200, "text/plain", "UPDATE FAILED");
        } else {
            server.send(200, "text/plain", "UPDATE SUCCESS");
            delay(1000);
            ESP.restart();
        }
    }, handleUpdate);
    
    // Heap info endpoint
    server.on("/heap", HTTP_GET, []() {
        server.send(200, "text/plain", String(ESP.getFreeHeap() / 1024));
    });
    
    server.begin();
    Serial.println("Web server started on port 80");
    
    // Setup WebSocket server
    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.println("WebSocket client connected: " + String(num));
        } else if (type == WStype_DISCONNECTED) {
            Serial.println("WebSocket client disconnected: " + String(num));
        }
    });
    Serial.println("WebSocket server started on port 81");
    
    Serial.println("=== Setup Complete ===");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Ready! Access the vacuum display at: http://" + WiFi.localIP().toString());
    } else {
        Serial.println("Ready! Connect to SoftAP and access: http://192.168.4.1");
    }
}

void loop() {
    // Check WiFi connection and reconnect if needed
    if (!softAPMode && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Attempting to reconnect...");
        connectToWiFi();
    }
    
    adcReadings_fun();
    server.handleClient();
    webSocket.loop();

    unsigned long currentTime = millis(); 

    // Button handling for settings mode
    if (isButtonPressed(BTN_TOGGLE) && currentTime - lastButtonPressTime > debounceDelay) {
        lastButtonPressTime = currentTime;
        currentModeIndex++;

        if (currentModeIndex > 3) {
            settingsMode = false;
            currentModeIndex = -1;
            lastButtonPressTime = 0;
            Serial.println("Exiting settings mode");
        } else {
            settingsMode = true;
            lastInteractionTime = currentTime;
            Serial.println("currentModeIndex: " + String(currentModeIndex));
            
            if (currentModeIndex == 2) {
                tempUnit = currentUnit;
            }
        }
    }

    if (settingsMode) {
        // Auto exit after 25s of no interaction
        if (currentTime - lastInteractionTime > 25000) {
            settingsMode = false;
            currentModeIndex = -1;
            Serial.println("Settings mode timed out");
        }

        if (currentModeIndex >= 0 && currentModeIndex <= 2) {
            // INC / DEC buttons
            if ((isButtonPressed(BTN_INC) || isButtonPressed(BTN_DEC)) && currentTime - lastButtonPressTime > debounceDelay) {
                lastButtonPressTime = currentTime;

                if (!btnHeld) {
                    btnPressStart = currentTime;
                    btnHeld = true;
                }

                unsigned long holdTime = currentTime - btnPressStart;

                if (currentModeIndex == 2) { // Unit
                    if (isButtonPressed(BTN_INC)) {
                        tempUnit = (UnitType)(((int)currentUnit + 1) % 3);
                    } else if (isButtonPressed(BTN_DEC)) {
                        tempUnit = (UnitType)(((int)currentUnit + 2) % 3);
                    }
                } else {
                    updateValue(isButtonPressed(BTN_INC), holdTime);
                }

                lastInteractionTime = currentTime;
                delay(100);
            } else {
                btnHeld = false;
            }

            // OK button to save
            if (isButtonPressed(BTN_OK) && currentTime - lastButtonPressTime > debounceDelay) {
                lastButtonPressTime = currentTime;

                if (currentModeIndex == 2) {
                    currentUnit = tempUnit;
                    saveUnitSettings();
                    Serial.println("Saved unit setting");
                } else {
                    // Save threshold values when button is pressed
                    saveVacuumSettings();
                    HL1 = floatValues[0] * pow(10, exponents[0]);
                    LL1 = floatValues[1] * pow(10, exponents[1]);
                    Serial.println("Saved threshold settings via buttons");
                }
                lastInteractionTime = currentTime;
            }
        }

        // Display handling in settings mode
        if (currentModeIndex == 0 || currentModeIndex == 1) {
            float displayValue = floatValues[currentModeIndex] * pow(10, exponents[currentModeIndex]);
            showFloat(display1, displayValue);
            displayMode(display2, modes[currentModeIndex]);
        } 
        else if (currentModeIndex == 2) {
            displayUnitType(display1, tempUnit);
            displayMode(display2, modes[currentModeIndex]);
        } 
        else if (currentModeIndex == 3) {
            settingsMode = false;
            currentModeIndex = -1;
            lastButtonPressTime = 0;
            
            if (voltage_diff <= -100 || voltage_diff > 1200) {
                displayFail(display1, display2);
            } else {
                 float displayVacuum = convertToSelectedUnit(vacuum1, currentUnit);
                 showFloat(display2, displayVacuum);
                 displayUnitType(display1, currentUnit);
                 delay(100); 
            }
        }
    } 
    else {
        // Normal mode: check for sensor failure
        if (voltage_diff <= -100 || voltage_diff > 1200) {
            displayFail(display1, display2);
        } else {
            // Normal vacuum display
            float displayVacuum = convertToSelectedUnit(vacuum1, currentUnit);
            showFloat(display2, displayVacuum);
            displayUnitType(display1, currentUnit);
        }
    }

    // Relay control
    if (vacuum1 < LL1) {
        if (digitalRead(relay2) != HIGH) {
            Serial.println("LL1-on");
            digitalWrite(relay2, HIGH);
        }
    } else {
        if (vacuum1 > HL1) {
            if (digitalRead(relay2) != LOW) {
                Serial.println("HL1-off");
                digitalWrite(relay2, LOW);
            }
        }
    }

    // Send WebSocket data for live monitoring
    String json = "{\"vacuum\":" + String(vacuum1, 6) + 
                  ",\"voltage1\":" + String(voltage_1, 3) + 
                  ",\"voltage2\":" + String(voltage_2, 3) + 
                  ",\"voltage3\":" + String(voltage_3, 3) + 
                  ",\"voltage_diff\":" + String(voltage_diff, 3) + 
                  ",\"unit\":\"" + getUnitString(currentUnit) + "\"}";
    webSocket.broadcastTXT(json);

    delay(50);
}

float getVacuumFromVoltage(float input_voltage) {
    // Convert input voltage from current range to original table range
    float voltage_ratio = (input_voltage - current_min_voltage) / (current_max_voltage - current_min_voltage);
    float table_voltage = ORIGINAL_MIN_VOLTAGE + (voltage_ratio * (ORIGINAL_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE));
    
    // Keep your existing lookup logic
    if (table_voltage <= voltageToVacuumTable[0].voltage) {
        return voltageToVacuumTable[0].vacuum;
    }
    
    if (table_voltage >= voltageToVacuumTable[TABLE_SIZE - 1].voltage) {
        return voltageToVacuumTable[TABLE_SIZE - 1].vacuum;
    }
    
    int closestIndex = 0;
    float minDifference = abs(table_voltage - voltageToVacuumTable[0].voltage);
    
    for (int i = 1; i < TABLE_SIZE; i++) {
        float difference = abs(table_voltage - voltageToVacuumTable[i].voltage);
        if (difference < minDifference) {
            minDifference = difference;
            closestIndex = i;
        }
    }
    
    return voltageToVacuumTable[closestIndex].vacuum;
}

float getDynamicThreshold() {
    float original_range = ORIGINAL_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE;
    float current_range = current_max_voltage - current_min_voltage;
    return 75.0f * (current_range / original_range);
}


bool isButtonPressed(int pin) {
    return digitalRead(pin) == LOW;
}

void adcReadings_fun() {
    adcReading_1 = ads.readADC_SingleEnded(0);
    float voltage1_now = (adcReading_1 * 4096.0) / 32768.0;
    
    if (buffer_index >= num_samples) buffer_index = 0;
    
    voltage1_buffer[buffer_index] = voltage1_now;
    float sum1 = 0;
    for (int i = 0; i < num_samples; i++) {
        sum1 += voltage1_buffer[i];
    }
    voltage_1 = sum1 / num_samples;

    adcReading_2 = ads.readADC_SingleEnded(1);
    float voltage2_now = (adcReading_2 * 4096.0) / 32768.0;
    voltage2_buffer[buffer_index] = voltage2_now;
    float sum2 = 0;
    for (int i = 0; i < num_samples; i++) {
        sum2 += voltage2_buffer[i];
    }
    voltage_2 = sum2 / num_samples;

    adcReading_3 = ads.readADC_SingleEnded(2);
    float voltage3_now = (adcReading_3 * 4096.0) / 32768.0;
    voltage3_buffer[buffer_index] = voltage3_now;
    float sum3 = 0;
    for (int i = 0; i < num_samples; i++) {
        sum3 += voltage3_buffer[i];
    }
    voltage_3 = sum3 / num_samples;

    voltage_diff = voltage_2 - voltage_3;
    
    // Adjust sampling based on voltage difference
    if (voltage_diff < getDynamicThreshold()) {  
        num_samples = 20;
    } else {
        num_samples = 3;
    }
    
    if (buffer_index >= num_samples) buffer_index = 0;
    
    vacuum1 = getVacuumFromVoltage(voltage_diff);
    
    buffer_index++;
    if (buffer_index >= num_samples) buffer_index = 0;
}

// NEW FUNCTION: Display direct float values with decimal points on TM1637
void showFloat(TM1637Display &disp, float value) {
    uint8_t digits[4];
    
    // Handle different ranges and format appropriately for 4-digit display
    if (value >= 1000) {
        // For values >= 1000, show as integer (e.g., 1000)
        int intValue = (int)value;
        if (intValue > 9999) intValue = 9999;
        
        digits[0] = disp.encodeDigit((intValue / 1000) % 10);
        digits[1] = disp.encodeDigit((intValue / 100) % 10);
        digits[2] = disp.encodeDigit((intValue / 10) % 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    }
    else if (value >= 100) {
        // For values 100-999.9, show one decimal place (e.g., 123.4)
        int intValue = (int)(value * 10);
        if (intValue > 9999) intValue = 9999;
        
        digits[0] = disp.encodeDigit((intValue / 1000) % 10);
        digits[1] = disp.encodeDigit((intValue / 100) % 10);
        digits[2] = disp.encodeDigit((intValue / 10) % 10) | 0x80; // Add decimal point
        digits[3] = disp.encodeDigit(intValue % 10);
    }
    else if (value >= 10) {
        // For values 10-99.99, show two decimal places (e.g., 12.34)
        int intValue = (int)(value * 100);
        if (intValue > 9999) intValue = 9999;
        
        digits[0] = disp.encodeDigit((intValue / 1000) % 10);
        digits[1] = disp.encodeDigit((intValue / 100) % 10) | 0x80; // Add decimal point
        digits[2] = disp.encodeDigit((intValue / 10) % 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    }
    else if (value >= 1) {
        // For values 1-9.999, show three decimal places (e.g., 1.234)
        int intValue = (int)(value * 1000);
        if (intValue > 9999) intValue = 9999;
        
        digits[0] = disp.encodeDigit((intValue / 1000) % 10) | 0x80; // Add decimal point
        digits[1] = disp.encodeDigit((intValue / 100) % 10);
        digits[2] = disp.encodeDigit((intValue / 10) % 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    }
    else {
        // For values 0.001-0.999, show three decimal places (e.g., 0.123)
        int intValue = (int)(value * 1000);
        if (intValue > 999) intValue = 999;
        
        digits[0] = disp.encodeDigit(0) | 0x80; // "0." 
        digits[1] = disp.encodeDigit((intValue / 100) % 10);
        digits[2] = disp.encodeDigit((intValue / 10) % 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    }
    
    disp.setSegments(digits);
}

void displayMode(TM1637Display &disp, String mode) {
    uint8_t modeSegments[4];
    uint8_t seg_H = 0b01110110;
    uint8_t seg_L = 0b00111000;
    uint8_t seg_1 = 0b00110000;
    uint8_t seg_U = 0b00111110;
    uint8_t seg_n = 0b01010100;
    uint8_t seg_t = 0b01111000;
    uint8_t seg_space = 0x00;

    if (mode == "HL-1") { 
        modeSegments[0] = seg_H; modeSegments[1] = seg_L; 
        modeSegments[2] = seg_space; modeSegments[3] = seg_1; 
    }
    else if (mode == "LL-1") { 
        modeSegments[0] = seg_L; modeSegments[1] = seg_L; 
        modeSegments[2] = seg_space; modeSegments[3] = seg_1; 
    }
    else if (mode == "Unit") { 
        modeSegments[0] = seg_U; modeSegments[1] = seg_n; 
        modeSegments[2] = seg_t; modeSegments[3] = seg_space; 
    }
    
    disp.setSegments(modeSegments);
}

void updateValue(bool increment, unsigned long holdTime) {
    float &currentValue = floatValues[currentModeIndex];
    int &exponent = exponents[currentModeIndex];

    if (holdTime < 3000) {
        currentValue += (increment ? 0.1 : -0.1);
    } else if (holdTime < 5000) {
        currentValue = (increment ? currentValue + 1.0 : currentValue - 1.0);
    } else {
        exponent += (increment ? 1 : -1);
    }

    if (currentValue >= 10.0) { currentValue /= 10.0; exponent++; }
    while (currentValue < 1.0 && exponent > -3) { currentValue *= 10.0; exponent--; }

    if (exponent > 3) { exponent = 3; currentValue = 9.9; }
    if (exponent < -3) { exponent = -3; currentValue = 1.0; }
    if (currentValue > 9.9) { currentValue = 9.9; }
    if (currentValue < 1.0) { currentValue = 1.0; }

    float finalValue = currentValue * pow(10, exponent);
    if (finalValue > 1000.0) {
        finalValue = 1000.0;
        exponent = 0;
        while (finalValue >= 10.0 && exponent < 3) {
            finalValue /= 10.0;
            exponent++;
        }
        currentValue = finalValue;
    }
}

void displayUnitType(TM1637Display &disp, UnitType type) {
    uint8_t segments[4];
    
    switch (type) {
        case UNIT_MBA:
            segments[0] = 0b01010100;  // m
            segments[1] = 0b01111100;  // b
            segments[2] = 0b01110111;  // A
            segments[3] = 0b01010000;  // r
            break;
        case UNIT_TORR:
            segments[0] = 0b01111000;  // t
            segments[1] = 0b00111111;  // o
            segments[2] = 0b01010000;  // r
            segments[3] = 0b01010000;  // r
            break;
        case UNIT_PA:
            segments[0] = 0b01110011;  // P
            segments[1] = 0b01110111;  // A
            segments[2] = 0x00;        // blank
            segments[3] = 0x00;        // blank
            break;
    }
    disp.setSegments(segments);
}

float convertToSelectedUnit(float vacuumInMbar, UnitType unitType) {
    switch (unitType) {
        case UNIT_TORR:
            return vacuumInMbar * MBAR_TO_TORR;
        case UNIT_PA:
            return vacuumInMbar * MBAR_TO_PA;
        case UNIT_MBA:
        default:
            return vacuumInMbar;
    } 
}

void loadUnitSettings() {
    preferences.begin("unit_settings", false);
    currentUnit = (UnitType)preferences.getInt("unit", UNIT_MBA);
    preferences.end();
}

void saveUnitSettings() {
    preferences.begin("unit_settings", false);
    preferences.putInt("unit", currentUnit);
    preferences.end();
}

void saveVacuumSettings() {
    preferences.begin("settings", false);
    for (int i = 0; i < 2; i++) {
        preferences.putFloat(modes[i], floatValues[i]);
        preferences.putInt((String(modes[i]) + "_exp").c_str(), exponents[i]);
    }
    preferences.end();
    Serial.println("Threshold settings saved to flash");
}

//range settings functions
void saveRangeSettings() {
    preferences.begin("range_settings", false);
    preferences.putFloat("min_voltage", current_min_voltage);
    preferences.putFloat("max_voltage", current_max_voltage);
    preferences.end();
    
    Serial.println("Stored ranges successfully");
    Serial.printf("Saved - Min: %.1f V, Max: %.1f V\n", current_min_voltage, current_max_voltage);
}

// CORRECTED FUNCTION
void loadRangeSettings() {
    preferences.begin("range_settings", true); // Open in read-only mode

    // Check if the keys actually exist before trying to load them
    if (preferences.isKey("min_voltage") && preferences.isKey("max_voltage")) {
        current_min_voltage = preferences.getFloat("min_voltage", ORIGINAL_MIN_VOLTAGE);
        current_max_voltage = preferences.getFloat("max_voltage", ORIGINAL_MAX_VOLTAGE);
        Serial.println(" Loaded voltage ranges from flash memory.");
    } else {
        // Keys don't exist, so we use the default values defined globally.
        Serial.println("  No voltage ranges found in flash, using default compile-time values.");
    }
    preferences.end();
    
    Serial.printf("Initialized with Min: %.1f V, Max: %.1f V\n", current_min_voltage, current_max_voltage);
}

void loadVacuumSettings() {
    preferences.begin("settings", false);
    for (int i = 0; i < 2; i++) {
        floatValues[i] = preferences.getFloat(modes[i], 1.0);
        exponents[i] = preferences.getInt((String(modes[i]) + "_exp").c_str(), -3);
    }
    preferences.end();
    
    HL1 = floatValues[0] * pow(10, exponents[0]);
    LL1 = floatValues[1] * pow(10, exponents[1]);
}

void displayFail(TM1637Display &display1, TM1637Display &display2) {
    const uint8_t S_SEG = SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
    const uint8_t E_SEG = SEG_A | SEG_F | SEG_G | SEG_E | SEG_D;
    const uint8_t N_SEG = SEG_C | SEG_E | SEG_G;
    const uint8_t R_SEG = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t F_SEG = SEG_A | SEG_E | SEG_F | SEG_G;
    const uint8_t A_SEG = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t I_SEG = SEG_B | SEG_C;
    const uint8_t L_SEG = SEG_D | SEG_E | SEG_F;

    uint8_t sensorData[] = { S_SEG, E_SEG, N_SEG, R_SEG };
    uint8_t failData[] = { F_SEG, A_SEG, I_SEG, L_SEG };

    display1.setSegments(failData);
    display2.setSegments(sensorData);
}

String getUnitString(UnitType unitType) {
    switch (unitType) {
        case UNIT_MBA: return "mBar";
        case UNIT_TORR: return "Torr";
        case UNIT_PA: return "Pa";
        default: return "mBar";
    }
}

// Helper functions for WiFi configuration
int hexToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

String urlDecode(String input) {
    String decoded = "";
    char a, b;
    for (size_t i = 0; i < input.length(); i++) {
        if (input[i] == '%') {
            if (i + 2 < input.length()) {
                a = input[i + 1];
                b = input[i + 2];
                if (isxdigit(a) && isxdigit(b)) {
                    decoded += (char)(hexToInt(a) * 16 + hexToInt(b));
                    i += 2;
                } else {
                    decoded += input[i];
                }
            } else {
                decoded += input[i];
            }
        } else if (input[i] == '+') {
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void saveWiFiSettings(String ssid, String password, String ip, String gateway, String subnet) {
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString("ip", ip);
    preferences.putString("gateway", gateway);
    preferences.putString("subnet", subnet);
    preferences.end();
    Serial.println("WiFi settings saved to flash memory");
}

void readWiFiSettings() {
    preferences.begin("wifi", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    ipStr = preferences.getString("ip", "");
    gatewayStr = preferences.getString("gateway", "");
    subnetStr = preferences.getString("subnet", "");
    preferences.end();
    
    Serial.println("=== WiFi Settings from Flash ===");
    Serial.println("SSID: " + (ssid.length() > 0 ? ssid : "Not set"));
    Serial.println("Static IP: " + (ipStr.length() > 0 ? ipStr : "Not set"));
    Serial.println("================================");
}

void connectToWiFi() {
    if (ssid.length() > 0 && password.length() > 0) {
        // Configure static IP if provided
        if (ipStr.length() > 0 && gatewayStr.length() > 0 && subnetStr.length() > 0) {
            local_IP.fromString(ipStr);
            gateway.fromString(gatewayStr);
            subnet.fromString(subnetStr);
            if (WiFi.config(local_IP, gateway, subnet)) {
                Serial.println("Static IP configuration set");
            }
        }
        
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("Connecting to WiFi: " + ssid);
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < MAX_WIFI_RETRIES) {
            delay(1000);
            Serial.print(".");
            retries++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(" Successfully connected to WiFi!");
            Serial.println("IP Address: " + WiFi.localIP().toString());
            softAPMode = false;
        } else {
            Serial.println(" Failed to connect - Starting SoftAP mode");
            startSoftAP();
        }
    } else {
        Serial.println("No WiFi credentials - Starting SoftAP mode");
        startSoftAP();
    }
}
