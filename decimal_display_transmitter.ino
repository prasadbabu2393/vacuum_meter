

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

// Scientific notation structure
struct ScientificNotation {
    float mantissa;
    int exponent;
};

// Voltage to vacuum lookup table
const struct {
  float voltage;
  float vacuum;
} voltageToVacuumTable[] = {
//   {-13.000f, 0.001000f},  {-11.625f, 0.002000f},  {-9.250f, 0.003000f},   {-5.0f, 0.004000f},
//   {-2.120f, 0.005000f},   {0.000f, 0.006000f},    {3.130f, 0.007000f},    {7.880f, 0.008000f},
//   {10.500f, 0.009000f},   {40.200f, 0.010000f},   {48.380f, 0.011000f},   {52.500f, 0.012000f},
//   {57.000f, 0.013000f},   {62.130f, 0.014000f},   {65.000f, 0.015000f},   {69.000f, 0.016000f},
//   {73.000f, 0.017000f},   {76.500f, 0.018000f},   {79.750f, 0.019000f},   {82.000f, 0.020000f},
//   {86.125f, 0.021000f},   {90.250f, 0.022000f},   {93.000f, 0.023000f},   {96.500f, 0.024000f},
//   {101.200f, 0.025000f},  {103.500f, 0.026000f},  {106.000f, 0.027000f},  {110.620f, 0.028000f},
//   {113.000f, 0.029000f},  {115.000f, 0.030000f},  {382.000f, 0.100000f},  {388.500f, 0.110000f},
//   {419.000f, 0.120000f},  {443.000f, 0.130000f},  {460.000f, 0.140000f},  {488.000f, 0.150000f},
//   {501.000f, 0.160000f},  {520.000f, 0.170000f},  {540.000f, 0.180000f},  {560.000f, 0.190000f},
//   {585.000f, 0.200000f},  {829.200f, 0.500000f},  {851.000f, 0.600000f},  {875.000f, 0.700000f},
//   {889.500f, 0.800000f},  {898.500f, 0.900000f},  {906.000f, 1.000000f},  {934.000f, 2.000000f},
//   {940.000f, 3.000000f},  {946.500f, 4.000000f},  {951.000f, 5.000000f},  {957.000f, 6.000000f},
//   {962.000f, 7.000000f},  {969.100f, 8.000000f},  {975.000f, 9.000000f},  {980.000f, 1000.000000f},
//  {982.750f, 20.000000f}, {984.275f, 30.000000f}, {986.800f, 40.000000f}, {988.500f, 50.000000f},
//  {989.000f, 60.000000f}, {991.500f, 70.000000f}, {993.000f, 80.000000f}, {995.000f, 1000.000000f}

{-13.000f, 0.001000f},
  {-11.625f, 0.002000f},  // Missing value filled with average
  {-9.250f, 0.003000f},
  {-5.0f, 0.004000f},

  {-2.120f, 0.005000f},
  {0.000f, 0.006000f},

  {3.130f, 0.007000f},
  {7.880f, 0.008000f},
  {10.500f, 0.009000f},

  {40.200f, 0.010000f},
  {48.380f, 0.011000f},
  {52.500f, 0.012000f},
  {57.000f, 0.013000f},
  {62.130f, 0.014000f},
  {65.000f, 0.015000f},
  {69.000f, 0.016000f},  // Missing value filled with average
  {73.000f, 0.017000f},
  {76.500f, 0.018000f},  // Missing value filled with average
  {79.750f, 0.019000f},  // Missing value filled with average
 // {80.000f, 0.480000f},
  {82.000f, 0.020000f},
  {86.125f, 0.021000f},  // Missing value filled with average
  {90.250f, 0.022000f},
  {93.000f, 0.023000f},
  {96.500f, 0.024000f},
  {101.200f, 0.025000f},
  {103.500f, 0.026000f},
  {106.000f, 0.027000f},
  {110.620f, 0.028000f},
  {113.000f, 0.029000f},
  {115.000f, 0.030000f},
  {118.000f, 0.031000f},
  {123.500f, 0.032000f},
  {127.000f, 0.033000f},
  {130.000f, 0.034000f},
  {135.800f, 0.035000f},
  {140.000f, 0.036000f},
  {147.000f, 0.037000f},
  {150.500f, 0.038000f},
  {153.800f, 0.039000f},
  {159.500f, 0.040000f},
  {163.500f, 0.041000f},
  {167.000f, 0.042000f},
  {170.000f, 0.043000f},
  {174.500f, 0.044000f},
  {181.000f, 0.045000f},
  {184.500f, 0.046000f},
  {188.000f, 0.047000f},
  {192.500f, 0.048000f},
  {200.250f, 0.049000f},
  {204.000f, 0.050000f},
  {207.000f, 0.051000f},  // Missing value filled with average
  {210.000f, 0.052000f},
  {215.000f, 0.053000f},
  {218.000f, 0.054000f},
  {221.000f, 0.055000f},
  {224.000f, 0.056000f},
  {227.000f, 0.057000f},
  {231.000f, 0.058000f},
  {235.000f, 0.059000f},
  {239.000f, 0.060000f},
  {243.750f, 0.061000f},  // Missing value filled with average
  {248.500f, 0.062000f},
  {251.200f, 0.063000f},    
  {255.000f, 0.064000f},
  {259.000f, 0.065000f},
  {266.500f, 0.066000f},
  {268.500f, 0.067000f},
  {271.000f, 0.068000f},
  {274.750f, 0.069000f},  // Missing value filled with average
  {278.500f, 0.070000f},
  {281.200f, 0.071000f},
  {284.500f, 0.072000f},
  {288.750f, 0.073000f},
  {293.500f, 0.074000f},
  {297.000f, 0.075000f},
  {301.500f, 0.076000f},
  {305.000f, 0.077000f},
  {307.500f, 0.078000f},
  {312.000f, 0.079000f},
  {315.000f, 0.080000f},
  {318.000f, 0.081000f},
  {323.500f, 0.082000f},
  {325.500f, 0.083000f},  // Missing value filled with average
  {327.500f, 0.084000f},
  {332.500f, 0.085000f},
  {335.000f, 0.086000f},
  {337.000f, 0.087000f},
  {339.000f, 0.088000f},
  {343.500f, 0.089000f},
  {347.000f, 0.090000f},
  {350.000f, 0.091000f},
  {352.000f, 0.092000f},
  {354.850f, 0.093000f},  // Missing value filled with average
  {357.025f, 0.094000f},  // Missing value filled with average
  {365.200f, 0.095000f},
  {367.500f, 0.096000f},
  {371.500f, 0.097000f},
  {375.000f, 0.098000f},
  {378.000f, 0.099000f},
  {382.000f, 0.100000f},
  {388.500f, 0.110000f},
  {419.000f, 0.120000f},
  {443.000f, 0.130000f}, 
  {460.000f, 0.140000f},
  {488.000f, 0.150000f},
  {501.000f, 0.160000f},
  {520.000f, 0.170000f},
  {540.000f, 0.180000f},
  {560.000f, 0.190000f},
  {585.000f, 0.200000f},
  {605.000f, 0.210000f},
  {620.000f, 0.220000f},
  {636.000f, 0.230000f},
  {650.000f, 0.240000f},
  {670.000f, 0.250000f},
  {672.000f, 0.260000f},
  {690.000f, 0.270000f},
  {700.000f, 0.280000f},
  {712.000f, 0.290000f},
  {725.000f, 0.300000f},
  {738.000f, 0.310000f},
  {746.000f, 0.320000f},
  {750.000f, 0.330000f},
  {755.000f, 0.340000f},
  {762.000f, 0.350000f},
  {765.000f, 0.360000f},
  {772.000f, 0.370000f},
  {779.000f, 0.380000f},
  {785.000f, 0.390000f},
  {792.000f, 0.400000f},
  {795.000f, 0.410000f},
  {798.000f, 0.420000f},
  {804.000f, 0.430000f},
  {806.000f, 0.440000f},
  {810.000f, 0.450000f},
  {813.000f, 0.460000f},
  {817.000f, 0.470000f},
  {820.000f, 0.480000f},
  {824.000f, 0.490000f},
  {829.200f, 0.500000f},
  {832.200f, 0.510000f},
  {835.000f, 0.520000f},
  {836.500f, 0.530000f},
  {838.000f, 0.540000f},
  {841.000f, 0.560000f},
  {844.500f, 0.570000f},
  {846.000f, 0.580000f},
  {848.500f, 0.590000f},  // Missing value filled with average
  {851.000f, 0.600000f},
  {853.500f, 0.610000f},
  {855.000f, 0.620000f},
  {859.000f, 0.630000f},
  {861.000f, 0.640000f},
  {862.500f, 0.650000f},
  {866.000f, 0.660000f},
  {868.100f, 0.670000f},
  {870.000f, 0.680000f},
  {872.100f, 0.690000f},
  {875.000f, 0.700000f},
  {876.500f, 0.710000f},
  {878.000f, 0.720000f},
  {878.500f, 0.730000f},  // Missing value filled with average
  {879.500f, 0.740000f},
  {881.000f, 0.750000f},
  {883.000f, 0.760000f},
  {885.000f, 0.770000f},
  {887.000f, 0.780000f},
  {888.500f, 0.790000f},
  {889.500f, 0.800000f},
  {889.750f, 0.810000f},  // Missing value filled with average
  {890.000f, 0.820000f},
  {891.000f, 0.830000f},
  {892.750f, 0.840000f},
  {893.000f, 0.850000f},
  {894.000f, 0.860000f},
  {895.000f, 0.870000f},
  {896.500f, 0.880000f},
  {897.250f, 0.890000f},
  {898.500f, 0.900000f},
  {899.100f, 0.910000f},
  {899.300f, 0.920000f},
  {900.000f, 0.930000f},
  {901.300f, 0.940000f},
  {902.300f, 0.950000f},
  {903.500f, 0.960000f},
  {904.500f, 0.970000f},
  {905.500f, 0.980000f},
  {905.750f, 0.990000f},
  {906.000f, 1.000000f},
  {934.000f, 2.000000f},

  {940.000f, 3.000000f},
  {946.500f, 4.000000f},
  {951.000f, 5.000000f},
  {957.000f, 6.000000f},
  {962.000f, 7.000000f},
  {969.100f, 8.000000f},
  {975.000f, 9.000000f},
  {980.000f, 10.000000f},

// Missing value filled with average
  {982.750f, 20.000000f},
  {984.275f, 30.000000f},  // Missing value filled with average
  {986.800f, 40.000000f},
  {988.500f, 50.000000f},
  {989.000f, 60.000000f},
  {991.500f, 70.000000f},
  {993.000f, 80.000000f},
  {995.000f, 1000.000000f}  // For extreme case, using last available value
};

const int TABLE_SIZE = sizeof(voltageToVacuumTable) / sizeof(voltageToVacuumTable[0]);

// Function prototypes
float getVacuumFromVoltage(float voltage);
bool isButtonPressed(int pin);
void adcReadings_fun();
ScientificNotation toScientific(float value);
void showScientific(TM1637Display &disp, float num, int exponent);
void displayMode(TM1637Display &disp, String mode);
void updateValue(bool increment, unsigned long holdTime);
void displayUnitType(TM1637Display &disp, UnitType type);
float convertToSelectedUnit(float vacuumInMbar, UnitType unitType);
void loadUnitSettings();
void saveUnitSettings();
void loadVacuumSettings();
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
                    preferences.begin("settings", false);
                    preferences.putFloat(modes[currentModeIndex], floatValues[currentModeIndex]);
                    preferences.putInt((String(modes[currentModeIndex]) + "_exp").c_str(), exponents[currentModeIndex]);
                    preferences.end();

                    HL1 = floatValues[0] * pow(10, exponents[0]);
                    LL1 = floatValues[1] * pow(10, exponents[1]);

                    Serial.println("Saved threshold settings");
                }
                lastInteractionTime = currentTime;
            }
        }

        // Display handling in settings mode
        if (currentModeIndex == 0 || currentModeIndex == 1) {
            showScientific(display1, floatValues[currentModeIndex], exponents[currentModeIndex]);
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
                 ScientificNotation vac1 = toScientific(displayVacuum);
                 showScientific(display2, vac1.mantissa, vac1.exponent);
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
            ScientificNotation vac1 = toScientific(displayVacuum);
            showScientific(display2, vac1.mantissa, vac1.exponent);
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



ScientificNotation toScientific(float value) {
    ScientificNotation result;
    
    if (value == 0) {
        result.mantissa = 0;
        result.exponent = 0;
        return result;
    }
    
    float absValue = abs(value);
    int exp = 0;
    
    while (absValue >= 10.0) {
        absValue /= 10.0;
        exp++;
    }
    
    while (absValue < 1.0 && absValue > 0.0) {
        absValue *= 10.0;
        exp--;
    }
    
    result.mantissa = (value < 0) ? -absValue : absValue;
    result.exponent = exp;
    
    return result;
}

void showScientific(TM1637Display &disp, float num, int exponent) {
    int intNum = (int)(num * 10);
    uint8_t digits[4];
    digits[0] = disp.encodeDigit((intNum / 10) % 10) | 0x80;
    digits[1] = disp.encodeDigit(intNum % 10);
    digits[2] = (exponent >= 0) ? 0x00 : 0x40;
    digits[3] = disp.encodeDigit(abs(exponent));
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
        Serial.println("✅ Loaded voltage ranges from flash memory.");
    } else {
        // Keys don't exist, so we use the default values defined globally.
        Serial.println("⚠️  No voltage ranges found in flash, using default compile-time values.");
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
            message = "<h1>Success!</h1><p>New voltage range has been saved to flash memory.</p>";
            success = true;
        } else {
            message = "<h1> Error!</h1><p>Maximum voltage must be greater than minimum voltage.</p>";
        }
    } else {
        message = "<h1>Error!</h1><p>Missing parameters. Please provide both min and max voltage.</p>";
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
    html += "<p><a href='/ranges'> click here to go back immediately.</a></p>";
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


// UPDATED to include Range Config button
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
            display: flex;
            justify-content: space-between;
            margin-top: 20px;
            gap: 10px;
            flex-wrap: wrap;
        }
        .nav-link {
            flex: 1;
            text-align: center;
            padding: 10px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            transition: all 0.3s ease;
            min-width: 120px;
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
            <h1> Vacuum Meter Configuration</h1>
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
            <a href="/vacuum" class="nav-link"> Vacuum Display</a>
            <a href="/data" class="nav-link"> Live Data</a>
            <a href="/ranges" class="nav-link">Range Config</a>
            <a href="/update" class="nav-link"> OTA Update</a>
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

// UPDATED to include Range Config button
void handleVacuumDisplay() {
    float displayVacuum = convertToSelectedUnit(vacuum1, currentUnit);
    String unitStr = getUnitString(currentUnit);
    
    ScientificNotation vac1 = toScientific(displayVacuum);
    
    String vacuumReading;
    if (vac1.exponent == 0) {
        vacuumReading = String(vac1.mantissa, 2);
    } else {
        vacuumReading = String(vac1.mantissa, 1) + "E" + String(vac1.exponent);
    }
    
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
        }
        .nav-button {
            background: linear-gradient(45deg, #667eea, #764ba2);
            color: white;
            border: none;
            padding: 12px 25px;
            margin: 5px;
            border-radius: 25px;
            cursor: pointer;
            font-size: 1em;
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
            <a href="/config" class="nav-button">Configuration</a>
            <a href="/ranges" class="nav-button">Range Config</a>
            <a href="/data" class="nav-button">Live Data</a>
            <a href="/update" class="nav-button">OTA Update</a>
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
    ScientificNotation hl1_sci = toScientific(convertToSelectedUnit(HL1, currentUnit));
    ScientificNotation ll1_sci = toScientific(convertToSelectedUnit(LL1, currentUnit));
    
    String hl1_str = (hl1_sci.exponent == 0) ? String(hl1_sci.mantissa, 2) : 
                     String(hl1_sci.mantissa, 1) + "E" + String(hl1_sci.exponent);
    String ll1_str = (ll1_sci.exponent == 0) ? String(ll1_sci.mantissa, 2) : 
                     String(ll1_sci.mantissa, 1) + "E" + String(ll1_sci.exponent);
    
    html.replace("%HL1_VALUE%", hl1_str + " " + unitStr);
    html.replace("%LL1_VALUE%", ll1_str + " " + unitStr);
    
    // Relay status
    bool relayState = digitalRead(relay2);
    html.replace("%RELAY_STATUS%", relayState ? "ON" : "OFF");
    html.replace("%RELAY_CLASS%", relayState ? "relay-on" : "relay-off");
    
    html.replace("%TIMESTAMP%", String(millis() / 1000) + "s");
    
    server.send(200, "text/html", html);
}

// UPDATED to include Range Config button
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
            padding: 12px 24px;
            background: #667eea;
            color: white;
            text-decoration: none;
            border-radius: 8px;
            transition: all 0.3s ease;
            font-weight: 600;
            margin: 0 10px;
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
            <a href="/config" class="nav-button">WiFi Config</a>
            <a href="/ranges" class="nav-button">Range Config</a>
            <a href="/vacuum" class="nav-button">Vacuum Display</a>
            <a href="/update" class="nav-button">OTA Update</a>
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
      <p> Select a compiled firmware file (.bin format)</p>
      <p> File will be uploaded and flashed automatically</p>
      <p> Device will restart after successful update</p>
      <p> Do not disconnect power during the update process</p>
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
      <p><strong> Device Information:</strong></p>
      <p>Device: ESP32 Vacuum Meter</p>
      <p>Current IP: )rawliteral" + currentIPAddress + R"rawliteral(</p>
      <p>Mode: )rawliteral" + (softAPMode ? "SoftAP" : "WiFi Station") + R"rawliteral(</p>
 
    </div>
    
    <a href="/vacuum" class="back-link"> Vacuum Display</a>
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