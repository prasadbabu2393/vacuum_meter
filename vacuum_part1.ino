// ESP32 Dual Vacuum Measurement System - Part 1: Main Code
// Patched with reverted display logic, improved button controls, and integrated UI settings.
// OPTIMIZED for performance and stability, with exponential display and secure OTA.
// FIXED Watchdog Timer initialization for newer ESP32 Core versions.

#include <TM1637Display.h>  //01-10-25
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Update.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <esp_task_wdt.h> // Required for Watchdog Timer

// Create objects
Adafruit_ADS1115 ads;
TM1637Display display1(18, 19); // CLK1, DIO1
TM1637Display display2(32, 33);  // CLK2, DIO2
Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
HardwareSerial uart2(2); // For Modbus

// Pin definitions
#define BTN_INC  34

#define BTN_DEC  35
#define BTN_TOGGLE 14
#define relay1 27
#define relay2 23

// LED pins
#define LED_MBA  13
#define LED_TORR 5
#define LED_PA   25
#define LED_RELAY1 4
#define LED_RELAY2 26

// Modbus pins
#define RE1 12  // RS485 Direction control

// Voltage range shifting
float current_min_voltage = 76.0f;
float current_max_voltage = 1093.0f;
float current_min_voltage_2 = 4.0f;
float current_max_voltage_2 = 1054.0f;
const float ORIGINAL_MIN_VOLTAGE = 172.03f;  //-13.0f;
const float ORIGINAL_MAX_VOLTAGE = 1088.0f; ///995.0f;

// WiFi Configuration
String softAP_SSID = "Vacuum_meter#01";
String softAP_Password = "12345678";
const char* ota_username = "admin";
const char* ota_password = "password";

// Modbus Configuration
uint8_t modbus_slave_id = 1;
uint32_t modbus_baudrate = 9600;
String ssid, password, ipStr, gatewayStr, subnetStr;
IPAddress local_IP, gateway, subnet;
const int MAX_WIFI_RETRIES = 20;
bool softAPMode = false;
bool updateInProgress = false;

// Sampling variables
int num_samples = 10;
#define MAX_SAMPLES 20
float voltage1_buffer[MAX_SAMPLES] = {0};
float voltage2_buffer[MAX_SAMPLES] = {0};
float voltage3_buffer[MAX_SAMPLES] = {0};
float voltage4_buffer[MAX_SAMPLES] = {0};
int buffer_index = 0;
// Unit selection
enum UnitType { UNIT_MBA, UNIT_TORR, UNIT_PA };
UnitType currentUnit = UNIT_MBA;
UnitType tempUnit = currentUnit;
// Conversion factors
const float MBAR_TO_TORR = 0.75; // 0.750062;
const float MBAR_TO_PA = 100.0;
// Settings modes
const char* modes[] = {"HL-1", "LL-1", "HL-2", "LL-2", "Unit"};
int currentModeIndex = -1;
float floatValues[4] = {1.0e-3, 1.0e-3, 1.0e-3, 1.0e-3};
static int currentTableIndex[4] = {0, 0, 0, 0};
// Button state tracking
struct ButtonState {
    bool currentState;
    bool lastState;
    unsigned long lastChangeTime;
    bool pressed;
};
ButtonState btnInc = {HIGH, HIGH, 0, false};
ButtonState btnDec = {HIGH, HIGH, 0, false};
ButtonState btnToggle = {HIGH, HIGH, 0, false};
const unsigned long DEBOUNCE_TIME = 50;
// Vacuum and sensor variables
float HL1, LL1, HL2, LL2;
float vacuum1 = 0.001;
float vacuum2 = 0.001;
double voltage_1, voltage_2, voltage_3, voltage_4, voltage_diff1, voltage_diff2;

// Settings mode variables
unsigned long btnPressStart = 0;
bool btnHeld = false;
unsigned long lastInteractionTime = 0;
bool settingsMode = false;

// WebSocket timing
unsigned long lastWsTime = 0;
const long wsInterval = 1000; // Send updates every 1 second

// Modbus registers
uint16_t modbusRegisters[4];

// Voltage to vacuum lookup table - MOVED TO PROGMEM TO SAVE RAM
const struct {
    float voltage;
    float vacuum;
} voltageToVacuumTable[] PROGMEM = {
    {172.310f, 0.001000f}, {173.43f, 0.002000f}, {175.69f, 0.003000f}, {177.91f, 0.004000f},
    {182.70f, 0.005000f}, {184.52f, 0.006000f}, {186.25f, 0.007000f}, {190.53f, 0.008000f},
    {197.41f, 0.009000f}, {210.200f, 0.010000f}, {225.380f, 0.011000f}, {230.500f, 0.012000f},
    {234.000f, 0.013000f}, {239.130f, 0.014000f}, {242.000f, 0.015000f}, {247.000f, 0.016000f},
    {249.000f, 0.017000f}, {253.500f, 0.018000f}, {256.750f, 0.019000f}, {258.000f, 0.020000f},
    {261.125f, 0.021000f}, {263.250f, 0.022000f}, {267.000f, 0.023000f}, {272.500f, 0.024000f},
    {274.200f, 0.025000f}, {276.500f, 0.026000f}, {280.000f, 0.027000f}, {283.620f, 0.028000f},
    {285.000f, 0.029000f}, {287.000f, 0.030000f}, {292.000f, 0.031000f}, {294.500f, 0.032000f},
    {298.000f, 0.033000f}, {301.000f, 0.034000f}, {305.000f, 0.035000f}, {311.000f, 0.036000f},
    {314.000f, 0.037000f}, {318.500f, 0.038000f}, {321.800f, 0.039000f}, {324.500f, 0.040000f},
    {329.500f, 0.041000f}, {333.000f, 0.042000f}, {337.000f, 0.043000f}, {344.500f, 0.044000f},
    {347.000f, 0.045000f}, {351.500f, 0.046000f}, {354.000f, 0.047000f}, {357.500f, 0.048000f},
    {361.250f, 0.049000f}, {369.000f, 0.050000f}, {372.000f, 0.051000f}, {376.000f, 0.052000f},
    {378.000f, 0.053000f}, {380.000f, 0.054000f}, {384.000f, 0.055000f}, {387.000f, 0.056000f},
    {389.000f, 0.057000f}, {392.000f, 0.058000f}, {395.000f, 0.059000f}, {400.000f, 0.060000f},
    {404.750f, 0.061000f}, {408.500f, 0.062000f}, {411.200f, 0.063000f}, {414.000f, 0.064000f},
    {418.000f, 0.065000f}, {423.500f, 0.066000f}, {427.500f, 0.067000f}, {429.000f, 0.068000f},
    {432.750f, 0.069000f}, {435.500f, 0.070000f}, {439.200f, 0.071000f}, {442.500f, 0.072000f},
    {445.750f, 0.073000f}, {449.500f, 0.074000f}, {453.000f, 0.075000f}, {456.500f, 0.076000f},
    {460.000f, 0.077000f}, {462.500f, 0.078000f}, {466.000f, 0.079000f}, {468.000f, 0.080000f},
    {470.000f, 0.081000f}, {475.500f, 0.082000f}, {478.500f, 0.083000f}, {480.500f, 0.084000f},
    {484.500f, 0.085000f}, {486.000f, 0.086000f}, {488.000f, 0.087000f}, {491.000f, 0.088000f},
    {494.500f, 0.089000f}, {497.000f, 0.090000f}, {500.000f, 0.091000f}, {503.000f, 0.092000f},
    {505.850f, 0.093000f}, {507.025f, 0.094000f}, {513.200f, 0.095000f}, {518.500f, 0.096000f},
    {520.500f, 0.097000f}, {523.000f, 0.098000f}, {528.000f, 0.099000f}, {531.000f, 0.100000f},
    {539.500f, 0.110000f}, {556.000f, 0.120000f}, {580.000f, 0.130000f}, {598.000f, 0.140000f},
    {624.000f, 0.150000f}, {640.000f, 0.160000f}, {649.000f, 0.170000f}, {671.000f, 0.180000f},
    {688.000f, 0.190000f}, {703.000f, 0.200000f}, {729.000f, 0.210000f}, {750.000f, 0.220000f},
    {764.000f, 0.230000f}, {776.000f, 0.240000f}, {783.000f, 0.250000f}, {798.000f, 0.260000f},
    {810.000f, 0.270000f}, {818.000f, 0.280000f}, {833.000f, 0.290000f}, {842.000f, 0.300000f},
    {851.000f, 0.310000f}, {859.000f, 0.320000f}, {863.000f, 0.330000f}, {870.000f, 0.340000f},
    {874.000f, 0.350000f}, {877.000f, 0.360000f}, {884.000f, 0.370000f}, {890.000f, 0.380000f},
    {896.000f, 0.390000f}, {901.000f, 0.400000f}, {904.000f, 0.410000f}, {907.000f, 0.420000f},
    {911.000f, 0.430000f}, {915.000f, 0.440000f}, {918.000f, 0.450000f}, {921.000f, 0.460000f},
    {924.000f, 0.470000f}, {927.000f, 0.480000f}, {931.000f, 0.490000f}, {934.200f, 0.500000f},
    {939.200f, 0.510000f}, {941.000f, 0.520000f}, {943.500f, 0.530000f}, {944.000f, 0.540000f},
    {945.000f, 0.550000f}, {946.000f, 0.560000f}, {948.500f, 0.570000f}, {951.000f, 0.580000f},
    {953.500f, 0.590000f}, {955.000f, 0.600000f}, {958.500f, 0.610000f}, {960.000f, 0.620000f},
    {962.000f, 0.630000f}, {964.000f, 0.640000f}, {966.500f, 0.650000f}, {968.000f, 0.660000f},
    {970.100f, 0.670000f}, {971.000f, 0.680000f}, {974.100f, 0.690000f}, {976.000f, 0.700000f},
    {978.500f, 0.710000f}, {979.000f, 0.720000f}, {980.500f, 0.730000f}, {981.500f, 0.740000f},
    {983.000f, 0.750000f}, {985.000f, 0.760000f}, {987.000f, 0.770000f}, {988.000f, 0.780000f},
    {989.500f, 0.790000f}, {990.100f, 0.800000f}, {990.750f, 0.810000f}, {991.000f, 0.820000f},
    {991.500f, 0.830000f}, {992.750f, 0.840000f}, {993.000f, 0.850000f}, {994.000f, 0.860000f},
    {995.000f, 0.870000f}, {996.000f, 0.880000f}, {996.500f, 0.890000f}, {997.000f, 0.900000f},
    {997.600f, 0.910000f}, {998.300f, 0.920000f}, {999.000f, 0.930000f}, {1000.300f, 0.940000f},
    {1001.300f, 0.950000f}, {1002.500f, 0.960000f}, {1003.500f, 0.970000f}, {1004.500f, 0.980000f},
    {1005.750f, 0.990000f}, {1006.000f, 1.000000f}, {1028.000f, 2.000000f}, {1036.000f, 3.000000f},
    {1041.500f, 4.000000f}, {1045.000f, 5.000000f}, {1049.000f, 6.000000f}, {1054.000f, 7.000000f},
    {1060.100f, 8.000000f}, {1066.000f, 9.000000f}, {1071.000f, 10.000000f}, {1075.750f, 20.000000f},
    {1076.275f, 30.000000f}, {1078.800f, 40.000000f}, {1079.500f, 50.000000f}, {1080.000f, 60.000000f},
    {1081.500f, 70.000000f}, {1082.000f, 80.000000f}, 
    //{1084.800f, 100.000000f}, {1086.300f, 500.000000f},
    {1088.000f, 1000.000000f}
};
const int TABLE_SIZE = sizeof(voltageToVacuumTable) / sizeof(voltageToVacuumTable[0]);

// Function Prototypes
float getVacuumFromVoltage(float voltage);
float getVacuumFromVoltage2(float voltage);
void adcReadings_fun();
void updateButton(ButtonState &btn, int pin);
void showFloat(TM1637Display &disp, float value);
void showExponential(TM1637Display &disp, float value); // NEW FUNCTION
void displayMode(TM1637Display &disp, String mode);
void updateValueFromTable(bool increment, unsigned long holdTime);
void displayUnitType(TM1637Display &disp, UnitType type);
float convertToSelectedUnit(float vacuumInMbar, UnitType unitType);
void loadAllSettings();
void saveUnitSettings();
void saveVacuumSettings();
void saveRangeSettings();
void saveRangeSettings_2();
String getUnitString(UnitType unitType);
int findClosestTableIndex(float vacuumValue);
void connectToWiFi();
void startSoftAP();
void readWiFiSettings();
void saveWiFiSettings(String ssid, String password, String ip, String gateway, String subnet);

void reset_button() {
    // Static variable to track the start time of a button press.
    // 'static' ensures this variable retains its value across multiple calls.
    static unsigned long buttonPressStartTime = 0;

    // Check if the BTN_INC button is currently being pressed (LOW signal).
    if (digitalRead(BTN_INC) == LOW) {
        // If this is the beginning of the press, record the start time.
        if (buttonPressStartTime == 0) {
            buttonPressStartTime = millis();
        }
        
        // If the button has been held continuously for over 3000 milliseconds...
        if (millis() - buttonPressStartTime > 3000) {
            Serial.println("WiFi reset button held for 3 seconds - clearing WiFi credentials.");
            
            // Open preferences in read-write mode and clear all WiFi settings.
            preferences.begin("wifi", false);
            preferences.clear();
            preferences.end();

            Serial.println("WiFi credentials cleared. Restarting...");
            delay(100); // Short delay to ensure the serial message sends.
            ESP.restart();
        }
    } else {
        // If the button is released, reset the timer.
        buttonPressStartTime = 0;
    }
}


void setup() {
    Serial.begin(115200);

// Initialize Watchdog Timer
esp_task_wdt_config_t wdt_config = {};
wdt_config.timeout_ms = 3000;
wdt_config.trigger_panic = true;
wdt_config.idle_core_mask = (1 << 0) | (1 << 1);
esp_err_t wdt_init_result = esp_task_wdt_init(&wdt_config);
if (wdt_init_result == ESP_OK) {
    esp_task_wdt_add(NULL);
}

    Wire.begin(21, 22);
    if(!ads.begin()) { Serial.println("Failed to initialize ADS1115!"); }
    ads.setGain(GAIN_ONE);
    
    display1.setBrightness(7);
    display2.setBrightness(7);

    pinMode(BTN_INC, INPUT_PULLUP);
    pinMode(BTN_DEC, INPUT_PULLUP);
    pinMode(BTN_TOGGLE, INPUT_PULLUP);
    pinMode(relay1, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(LED_MBA, OUTPUT);
    pinMode(LED_TORR, OUTPUT);
    pinMode(LED_PA, OUTPUT);
    pinMode(LED_RELAY1, OUTPUT);
    pinMode(LED_RELAY2, OUTPUT);
    pinMode(RE1, OUTPUT);
    digitalWrite(RE1, LOW);
    
    loadAllSettings();
    floatValues[0] = HL1;
    floatValues[1] = LL1;
    floatValues[2] = HL2;
    floatValues[3] = LL2;
    uart2.begin(modbus_baudrate, SERIAL_8N1, 16, 17);
    
    readWiFiSettings();
    connectToWiFi();
    setupWebServer();
}

void loop() {
    unsigned long currentTime = millis();

    // Reset the watchdog timer on every loop to prevent a restart
    esp_task_wdt_reset();

    if (!softAPMode && WiFi.status() != WL_CONNECTED) { connectToWiFi(); }
    server.handleClient();
    webSocket.loop();
    
    adcReadings_fun();

    updateButton(btnInc, BTN_INC);
    updateButton(btnDec, BTN_DEC);
    updateButton(btnToggle, BTN_TOGGLE);
    if (btnToggle.pressed) {
        if (settingsMode) { // If already in settings mode, save current value before switching
             if (currentModeIndex == 4) { // Unit mode
                currentUnit = tempUnit;
                saveUnitSettings();
            } else { // HL/LL modes
                saveVacuumSettings();
                HL1 = floatValues[0];
                LL1 = floatValues[1];
                HL2 = floatValues[2];
                LL2 = floatValues[3];
            }
        }

        settingsMode = true;
        lastInteractionTime = currentTime;
        currentModeIndex++;

        if (currentModeIndex >= 5) { // Exit settings mode after last item
            settingsMode = false;
            currentModeIndex = -1;
        } else { // Entering a new settings item
            if (currentModeIndex >= 0 && currentModeIndex < 4) {
                float currentValue = 0;
                if (currentModeIndex == 0) currentValue = HL1;
                else if (currentModeIndex == 1) currentValue = LL1;
                else if (currentModeIndex == 2) currentValue = HL2;
                else if (currentModeIndex == 3) currentValue = LL2;
                currentTableIndex[currentModeIndex] = findClosestTableIndex(currentValue);
                // Sync floatValues/exponents to this starting point
                floatValues[currentModeIndex] = pgm_read_float_near(&voltageToVacuumTable[currentTableIndex[currentModeIndex]].vacuum);
            } else if (currentModeIndex == 4) { // Unit mode
                tempUnit = currentUnit;
            }
        }
    }

    if (settingsMode) {
        if (currentTime - lastInteractionTime > 25000) { // Timeout
            settingsMode = false;
            currentModeIndex = -1;
        }

        if (btnInc.currentState == LOW || btnDec.currentState == LOW) { // Check for hold, not just press
            if (!btnHeld) {
                btnPressStart = currentTime;
                btnHeld = true;
            }
            unsigned long holdTime = currentTime - btnPressStart;
            // A single press should trigger an update immediately
            if (btnInc.pressed || btnDec.pressed) {
                 holdTime = 0; // Treat first press as a single step
            }

            if (currentModeIndex == 4) { // Unit mode (only on press)
                if (btnInc.pressed) tempUnit = (UnitType)(((int)tempUnit + 1) % 3);
                else if (btnDec.pressed) tempUnit = (UnitType)(((int)tempUnit + 2) % 3);
            } else if (currentModeIndex >= 0 && currentModeIndex < 4) { // HL/LL modes
                // Only update value if it's a new press or after a short delay for holding
                static unsigned long lastStepTime = 0;
                if (currentTime - lastStepTime > 150) { // Control speed of holding
                    updateValueFromTable(btnInc.currentState == LOW, holdTime);
                    lastStepTime = currentTime;
                }
            }
            lastInteractionTime = currentTime;
        } else {
            btnHeld = false;
        }

        // Display update in settings mode
        if (currentModeIndex >= 0 && currentModeIndex <= 3) {
            showFloat(display1, floatValues[currentModeIndex]);
            displayMode(display2, modes[currentModeIndex]);
        } else if (currentModeIndex == 4) {
            displayUnitType(display1, tempUnit);
            displayMode(display2, modes[currentModeIndex]);
        }
    } else {
        // Normal mode display
        if (voltage_diff1 <= -100 || voltage_diff1 > 1200) {
            display1.setSegments((uint8_t[]){0x71, 0x77, 0x06, 0x38}); // F A I L
        } else {
            float convertedValue1 = convertToSelectedUnit(vacuum1, currentUnit);
            // NEW: Check if exponential notation should be used
            if ((currentUnit == UNIT_TORR || currentUnit == UNIT_PA) && (convertedValue1 < 0.1f || convertedValue1 >= 10000.0f)) {
                 showExponential(display1, convertedValue1);
            } else {
                 showFloat(display1, convertedValue1);
            }
        }
        if (voltage_diff2 <= -100 || voltage_diff2 > 1200) {
            display2.setSegments((uint8_t[]){0x71, 0x77, 0x06, 0x38}); // F A I L
        } else {
            float convertedValue2 = convertToSelectedUnit(vacuum2, currentUnit);
            // NEW: Check if exponential notation should be used
            if ((currentUnit == UNIT_TORR || currentUnit == UNIT_PA) && (convertedValue2 < 0.1f || convertedValue2 >= 10000.0f)) {
                 showExponential(display2, convertedValue2);
            } else {
                 showFloat(display2, convertedValue2);
            }
        }
    }
    if(!settingsMode) { reset_button(); } //reset button for clearing the wifi details 
    

    // Relay & LED control
    // digitalWrite(relay1, (vacuum1 < LL1 && vacuum1 > HL1)); //hl1 on and ll1 = off
    // digitalWrite(relay2, (vacuum2 < LL2 && vacuum2 > HL2));
    if(vacuum1 <= HL1){digitalWrite(relay1, HIGH); }
    if(vacuum1 >= LL1){digitalWrite(relay1, LOW); }
    if(vacuum2 <= HL2){digitalWrite(relay2, HIGH); }
    if(vacuum2 >= LL2){digitalWrite(relay2, LOW); }
    digitalWrite(LED_RELAY1, digitalRead(relay1));
    digitalWrite(LED_RELAY2, digitalRead(relay2));
    digitalWrite(LED_MBA, (currentUnit == UNIT_MBA));
    digitalWrite(LED_TORR, (currentUnit == UNIT_TORR));
    digitalWrite(LED_PA, (currentUnit == UNIT_PA));
    
    // Timed WebSocket broadcast
    if (currentTime - lastWsTime > wsInterval) {
        lastWsTime = currentTime;
        String vacuum1_str = (voltage_diff1 <= -100 || voltage_diff1 > 1200) ? "\"FAIL\"" : String(convertToSelectedUnit(vacuum1, currentUnit), 6);
        String vacuum2_str = (voltage_diff2 <= -100 || voltage_diff2 > 1200) ? "\"FAIL\"" : String(convertToSelectedUnit(vacuum2, currentUnit), 6);
        
        if(vacuum1_str == "FAIL"){digitalWrite(relay1, LOW); }
        if(vacuum2_str == "FAIL"){digitalWrite(relay2, LOW); }
        String json = "{\"vacuum1\":" + vacuum1_str + 
                      ",\"vacuum2\":" + vacuum2_str + 
                      ",\"voltage_diff1\":" + String(voltage_diff1, 2) + 
                      ",\"voltage_diff2\":" + String(voltage_diff2, 2) + 
                      ",\"baudrate\":" + String(modbus_baudrate) + "}";
        webSocket.broadcastTXT(json);
    }
    delay(50);
}


// NEW FUNCTION: Display values in exponential notation
void showExponential(TM1637Display &disp, float value) {
    if (value == 0.0f) {
        disp.showNumberDec(0);
        return;
    }

    // Calculate exponent and mantissa
    float exponent_f = floor(log10(abs(value)));
    int exponent = (int)exponent_f;
    float mantissa = value / pow(10, exponent);

    int mantissa_d1 = (int)mantissa; // Digit before decimal
    int mantissa_d2 = (int)((mantissa - mantissa_d1) * 10); // Digit after decimal

    uint8_t digits[4] = {0, 0, 0, 0};

    // Format: [M].[m] [sign] [E]
    digits[0] = disp.encodeDigit(mantissa_d1) | 0x80; // Add decimal point
    digits[1] = disp.encodeDigit(mantissa_d2);
    
    if (exponent < 0) {
        digits[2] = 0x40; // Minus sign (SEG_G)
        digits[3] = disp.encodeDigit(abs(exponent));
    } else {
        digits[2] = 0; // Blank for positive
        digits[3] = disp.encodeDigit(exponent);
    }
    
    disp.setSegments(digits);
}

// Reverted display function
void showFloat(TM1637Display &disp, float value) {
    uint8_t digits[4] = {0, 0, 0, 0};
    if (value < 0.1f) { // Range: 0.001 to 0.099 (e.g., "0.045")
        int intValue = (int)(value * 1000);
        digits[0] = disp.encodeDigit(0) | 0x80; // 0.
        digits[1] = disp.encodeDigit((intValue / 100) % 10);
        digits[2] = disp.encodeDigit((intValue / 10) % 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    } else if (value < 1.0f) { // Range: 0.10 to 0.99 (e.g., " 0.75")
        int intValue = (int)(value * 100);
        digits[0] = 0x00; // Blank
        digits[1] = disp.encodeDigit(0) | 0x80; // 0.
        digits[2] = disp.encodeDigit(intValue / 10);
        digits[3] = disp.encodeDigit(intValue % 10);
    } else { // Range: 1 to 9999 (e.g., "  50", "1000")
        int intValue = (int)round(value);
        if (intValue > 9999) intValue = 9999;

        if (intValue >= 1000) {
            digits[0] = disp.encodeDigit(intValue / 1000);
            digits[1] = disp.encodeDigit((intValue / 100) % 10);
            digits[2] = disp.encodeDigit((intValue / 10) % 10);
            digits[3] = disp.encodeDigit(intValue % 10);
        } else if (intValue >= 100) {
            digits[0] = 0x00; // Blank
            digits[1] = disp.encodeDigit(intValue / 100);
            digits[2] = disp.encodeDigit((intValue / 10) % 10);
            digits[3] = disp.encodeDigit(intValue % 10);
        } else if (intValue >= 10) {
            digits[0] = 0x00; // Blank
            digits[1] = 0x00; // Blank
            digits[2] = disp.encodeDigit(intValue / 10);
            digits[3] = disp.encodeDigit(intValue % 10);
        } else { // 0-9
            digits[0] = 0x00; // Blank
            digits[1] = 0x00; // Blank
            digits[2] = 0x00; // Blank
            digits[3] = disp.encodeDigit(intValue % 10);
        }
    }
    disp.setSegments(digits);
}

// Corrected button logic
void updateButton(ButtonState &btn, int pin) {
    bool reading = digitalRead(pin);
    if (reading != btn.lastState) {
        btn.lastChangeTime = millis();
    }
    if ((millis() - btn.lastChangeTime) > DEBOUNCE_TIME) {
        if (reading != btn.currentState) {
            btn.currentState = reading;
            if (btn.currentState == LOW) {
                btn.pressed = true;
                return;
            }
        }
    }
    btn.pressed = false;
    btn.lastState = reading;
}

void handleGetSettings() {
    String json = "{";
    json += "\"baudrate\":" + String(modbus_baudrate) + ",";
    json += "\"modbus_id\":" + String(modbus_slave_id);
    json += "}";
    server.send(200, "application/json", json);
}

void adcReadings_fun() {
    float v1_raw = (ads.readADC_SingleEnded(0) * 4096.0) / 32768.0;
    float v2_raw = (ads.readADC_SingleEnded(1) * 4096.0) / 32768.0;
    float v3_raw = (ads.readADC_SingleEnded(2) * 4096.0) / 32768.0;
    float v4_raw = (ads.readADC_SingleEnded(3) * 4096.0) / 32768.0;

    voltage1_buffer[buffer_index] = v1_raw;
    voltage2_buffer[buffer_index] = v2_raw;
    voltage3_buffer[buffer_index] = v3_raw;
    voltage4_buffer[buffer_index] = v4_raw;
    float sum1=0, sum2=0, sum3=0, sum4=0;
    for (int i=0; i<num_samples; i++) {
        sum1 += voltage1_buffer[i];
        sum2 += voltage2_buffer[i];
        sum3 += voltage3_buffer[i];
        sum4 += voltage4_buffer[i];
    }
    
    voltage_1 = sum1 / num_samples;
    voltage_2 = sum2 / num_samples;
    voltage_3 = sum3 / num_samples;
    voltage_4 = sum4 / num_samples;

    voltage_diff1 = voltage_2 - voltage_1;
    voltage_diff2 = voltage_4 - voltage_3;
    
    float threshold1 = getDynamicThreshold();
    float threshold2 = getDynamicThreshold2();
    
    num_samples = (voltage_diff1 < threshold1 || voltage_diff2 < threshold2) ? 20 : 3;
    
    vacuum1 = getVacuumFromVoltage(voltage_diff1);
    vacuum2 = getVacuumFromVoltage2(voltage_diff2);
    buffer_index = (buffer_index + 1) % num_samples;
}

float getDynamicThreshold() {
    float original_range = ORIGINAL_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE;
    float current_range = current_max_voltage - current_min_voltage;
    return 75.0f * (current_range / original_range);
}

float getDynamicThreshold2() {
    float original_range = ORIGINAL_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE;
    float current_range = current_max_voltage_2 - current_min_voltage_2;
    return 75.0f * (current_range / original_range);
}

void updateValueFromTable(bool increment, unsigned long holdTime) {
    int step = 1;
    if (holdTime >= 5000) step = 20;      // After 5 seconds, step by 20
    else if (holdTime >= 3000) step = 10; // After 3 seconds, step by 10
    
    if (increment) {
        currentTableIndex[currentModeIndex] = min(currentTableIndex[currentModeIndex] + step, TABLE_SIZE - 1);
    } else {
        currentTableIndex[currentModeIndex] = max(currentTableIndex[currentModeIndex] - step, 0);
    }
    
    floatValues[currentModeIndex] = pgm_read_float_near(&voltageToVacuumTable[currentTableIndex[currentModeIndex]].vacuum);
}

void displayMode(TM1637Display &disp, String mode) {
    if (mode == "HL-1") disp.setSegments((uint8_t[]){0x76, 0x38, 0x00, 0x06});
    else if (mode == "LL-1") disp.setSegments((uint8_t[]){0x38, 0x38, 0x00, 0x06});
    else if (mode == "HL-2") disp.setSegments((uint8_t[]){0x76, 0x38, 0x00, 0x5b});
    else if (mode == "LL-2") disp.setSegments((uint8_t[]){0x38, 0x38, 0x00, 0x5b});
    else if (mode == "Unit") disp.setSegments((uint8_t[]){0x3e, 0x54, 0x78, 0x00});
}

void displayUnitType(TM1637Display &disp, UnitType type) {
    if (type == UNIT_MBA) disp.setSegments((uint8_t[]){0, 0x74, 0x77, 0x50});
    else if (type == UNIT_TORR) disp.setSegments((uint8_t[]){0x78, 0x3F, 0x50, 0x50});
    else if (type == UNIT_PA) disp.setSegments((uint8_t[]){0, 0, 0x73, 0x77});
}

float convertToSelectedUnit(float m, UnitType u) { return u == UNIT_TORR ? m*MBAR_TO_TORR : (u == UNIT_PA ? m*MBAR_TO_PA : m); }
String getUnitString(UnitType u) { return u == UNIT_TORR ? "Torr" : (u == UNIT_PA ? "Pa" : "mBar"); }

float getVacuumFromVoltage(float input_voltage) {
    if (input_voltage >= current_max_voltage) {
        return pgm_read_float_near(&voltageToVacuumTable[TABLE_SIZE - 1].vacuum);
    }
    if (input_voltage <= current_min_voltage) {
        return pgm_read_float_near(&voltageToVacuumTable[0].vacuum);
    }

    const float TABLE_MAX_VOLTAGE = pgm_read_float_near(&voltageToVacuumTable[TABLE_SIZE - 1].voltage);
    float voltage_ratio = (input_voltage - current_min_voltage) / (current_max_voltage - current_min_voltage);
    float table_voltage = ORIGINAL_MIN_VOLTAGE + (voltage_ratio * (TABLE_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE));
    
    int closestIndex = 0;
    float minDifference = abs(table_voltage - pgm_read_float_near(&voltageToVacuumTable[0].voltage));
    
    for (int i = 1; i < TABLE_SIZE; i++) {
        float difference = abs(table_voltage - pgm_read_float_near(&voltageToVacuumTable[i].voltage));
        if (difference < minDifference) {
            minDifference = difference;
            closestIndex = i;
        }
    }
    return pgm_read_float_near(&voltageToVacuumTable[closestIndex].vacuum);
}

float getVacuumFromVoltage2(float input_voltage) {
    if (input_voltage >= current_max_voltage_2) {
        return pgm_read_float_near(&voltageToVacuumTable[TABLE_SIZE - 1].vacuum);
    }
    if (input_voltage <= current_min_voltage_2) {
        return pgm_read_float_near(&voltageToVacuumTable[0].vacuum);
    }

    const float TABLE_MAX_VOLTAGE = pgm_read_float_near(&voltageToVacuumTable[TABLE_SIZE - 1].voltage);
    float voltage_ratio = (input_voltage - current_min_voltage_2) / (current_max_voltage_2 - current_min_voltage_2);
    float table_voltage = ORIGINAL_MIN_VOLTAGE + (voltage_ratio * (TABLE_MAX_VOLTAGE - ORIGINAL_MIN_VOLTAGE));
    
    int closestIndex = 0;
    float minDifference = abs(table_voltage - pgm_read_float_near(&voltageToVacuumTable[0].voltage));
    
    for (int i = 1; i < TABLE_SIZE; i++) {
        float difference = abs(table_voltage - pgm_read_float_near(&voltageToVacuumTable[i].voltage));
        if (difference < minDifference) {
            minDifference = difference;
            closestIndex = i;
        }
    }
    return pgm_read_float_near(&voltageToVacuumTable[closestIndex].vacuum);
}

int findClosestTableIndex(float vacuumValue) {
    int closestIndex = 0; 
    float minDifference = abs(vacuumValue - pgm_read_float_near(&voltageToVacuumTable[0].vacuum));
    for (int i=1; i<TABLE_SIZE; i++) {
        float difference = abs(vacuumValue - pgm_read_float_near(&voltageToVacuumTable[i].vacuum));
        if (difference < minDifference) { 
            minDifference = difference; 
            closestIndex = i; 
        }
    }
    return closestIndex;
}

void loadAllSettings() {
    preferences.begin("vac_meter", true);
    currentUnit = (UnitType)preferences.getInt("unit", UNIT_MBA);
    HL1 = preferences.getFloat("hl1", 1.0e-3);
    LL1 = preferences.getFloat("ll1", 1.0e-3);
    HL2 = preferences.getFloat("hl2", 1.0e-3);
    LL2 = preferences.getFloat("ll2", 1.0e-3);
    current_min_voltage = preferences.getFloat("min_v1", -13.0f);
    current_max_voltage = preferences.getFloat("max_v1", 1000.0f);
    current_min_voltage_2 = preferences.getFloat("min_v2", -13.0f);
    current_max_voltage_2 = preferences.getFloat("max_v2", 1000.0f);
    modbus_slave_id = preferences.getUChar("modbus_id", 1);
    modbus_baudrate = preferences.getUInt("modbus_baud", 9600);
    softAP_SSID = preferences.getString("ap_ssid", "Vacuum_meter#01");
    softAP_Password = preferences.getString("ap_pass", "12345678");
    preferences.end();
}

void saveUnitSettings() { preferences.begin("vac_meter", false); preferences.putInt("unit", currentUnit); preferences.end(); }

void saveVacuumSettings() {
    preferences.begin("vac_meter", false);
    preferences.putFloat("hl1", floatValues[0]);
    preferences.putFloat("ll1", floatValues[1]);
    preferences.putFloat("hl2", floatValues[2]);
    preferences.putFloat("ll2", floatValues[3]);
    preferences.end();
}
void saveRangeSettings() { preferences.begin("vac_meter", false); preferences.putFloat("min_v1", current_min_voltage); preferences.putFloat("max_v1", current_max_voltage); preferences.end(); }
void saveRangeSettings_2() { preferences.begin("vac_meter", false); preferences.putFloat("min_v2", current_min_voltage_2); preferences.putFloat("max_v2", current_max_voltage_2); preferences.end(); }
void readWiFiSettings() { preferences.begin("wifi", true); ssid = preferences.getString("ssid", ""); password = preferences.getString("password", ""); ipStr = preferences.getString("ip", ""); gatewayStr = preferences.getString("gateway", ""); subnetStr = preferences.getString("subnet", ""); preferences.end(); }
void saveWiFiSettings(String s, String p, String i, String g, String n) { preferences.begin("wifi", false); preferences.putString("ssid", s); preferences.putString("password", p); preferences.putString("ip", i); preferences.putString("gateway", g); preferences.putString("subnet", n); preferences.end(); }
void connectToWiFi() { if (ssid.length() > 0) { if (ipStr.length() > 0) { local_IP.fromString(ipStr); gateway.fromString(gatewayStr); subnet.fromString(subnetStr); WiFi.config(local_IP, gateway, subnet); } WiFi.begin(ssid.c_str(), password.c_str()); int r = 0; while (WiFi.status() != WL_CONNECTED && r++ < MAX_WIFI_RETRIES) delay(500); if (WiFi.status() == WL_CONNECTED) { softAPMode = false; return; } } startSoftAP(); }
void startSoftAP() { softAPMode = true; WiFi.softAP(softAP_SSID.c_str(), softAP_Password.c_str()); }