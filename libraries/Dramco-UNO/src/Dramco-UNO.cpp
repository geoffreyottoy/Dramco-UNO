#include <Dramco-UNO.h>


// ------------------------ LMIC STUFF ------------------------
const lmic_pinmap lmic_pins = {
  .nss = DRAMCO_UNO_LMIC_NSS_PIN,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = DRAMCO_UNO_LMIC_RST_PIN,
  .dio = {DRAMCO_UNO_LMIC_DIO0_PIN, DRAMCO_UNO_LMIC_DIO1_PIN, DRAMCO_UNO_LMIC_DIO2_PIN},
};

static u1_t _appeui[DRAMCO_UNO_LORA_EUI_SIZE];
static u1_t _deveui[DRAMCO_UNO_LORA_EUI_SIZE];
static u1_t _appkey[DRAMCO_UNO_LORA_KEY_SIZE];

static osjob_t sendjob;
static osjob_t blinkjob;
static uint8_t data[DRAMCO_UNO_BUFFER_SIZE];
static byte _cursor;

static volatile unsigned int _wdtSleepTimeMillis;
static volatile unsigned long _millisInDeepSleep;

static bool _keep3V3Active = false;
static uint8_t _accelerometerIntEnabled = false;
static uint8_t _buttonIntEnabled = false;

float _calibration = 1;

LIS2DW12 _accelerometer;

bool packetReadyForTransmission = false; 

void os_getArtEui (u1_t* buf) { // LMIC expects reverse from TTN
  for(byte i = 8; i>0; i--){
    buf[8-i] = _appeui[i-1];
  }
}

void os_getDevEui (u1_t* buf) { // LMIC expects reverse from TTN
  for(byte i = 8; i>0; i--){
    buf[8-i] = _deveui[i-1];
  }
}

void os_getDevKey (u1_t* buf) {  // no reverse here
	memcpy(buf, _appkey, 16);
} 

#ifdef DEBUG
void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}
#endif 

void onEvent (ev_t ev) {
	#ifdef DEBUG
    Serial.print(os_getTime());
    Serial.print(": ");
    #endif 
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            #ifdef DEBUG
        	Serial.println(F("EV_SCAN_TIMEOUT"));
        	#endif
            break;
        case EV_BEACON_FOUND:
        	#ifdef DEBUG
            Serial.println(F("EV_BEACON_FOUND"));
            #endif
            break;
        case EV_BEACON_MISSED:
        	#ifdef DEBUG
            Serial.println(F("EV_BEACON_MISSED"));
            #endif
            break;
        case EV_BEACON_TRACKED:
        	#ifdef DEBUG
            Serial.println(F("EV_BEACON_TRACKED"));
            #endif
            break;
        case EV_JOINING:
        	#ifdef DEBUG
            Serial.println(F("EV_JOINING"));
            #endif
            break;
        case EV_JOINED:
        	#ifdef DEBUG
            Serial.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
              
            }
            #endif
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
	        // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
        case EV_JOIN_FAILED:
        	#ifdef DEBUG
            Serial.println(F("EV_JOIN_FAILED"));
            #endif
            error(DRAMCO_UNO_ERROR_LORA_JOIN);
            break;
        case EV_REJOIN_FAILED:
        	#ifdef DEBUG
            Serial.println(F("EV_REJOIN_FAILED"));
            #endif
            error(DRAMCO_UNO_ERROR_LORA_JOIN);
            break;
        case EV_TXCOMPLETE:
        	#ifdef DEBUG
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            #endif
            packetReadyForTransmission = false; 
            // Schedule next transmission
            // os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
       		#ifdef DEBUG
            Serial.println(F("EV_LOST_TSYNC"));
            #endif
            break;
        case EV_RESET:
        	#ifdef DEBUG
            Serial.println(F("EV_RESET"));
            #endif
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
       		#ifdef DEBUG
            Serial.println(F("EV_RXCOMPLETE"));
            #endif
            break;
        case EV_LINK_DEAD:
        	#ifdef DEBUG
            Serial.println(F("EV_LINK_DEAD"));
            #endif
            break;
        case EV_LINK_ALIVE:
        	#ifdef DEBUG
            Serial.println(F("EV_LINK_ALIVE"));
            #endif
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
        	#ifdef DEBUG
            Serial.println(F("EV_TXSTART"));
            #endif
            break;
        case EV_TXCANCELED:
        	#ifdef DEBUG
            Serial.println(F("EV_TXCANCELED"));
            #endif
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
        	#ifdef DEBUG
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            #endif
            error(DRAMCO_UNO_ERROR_LORA_JOIN);
            break;

        default:
        	#ifdef DEBUG
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            #endif
            break;
    }
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        #ifdef DEBUG
        Serial.println(F("OP_TXRXPEND, not sending")); 
        #endif
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, data, _cursor, 0);
        packetReadyForTransmission = true;
        #ifdef DEBUG
        Serial.println(F("Packet queued"));
        #endif
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void pciInit(byte pin){
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

void pciDeinit(){
    PCICR  = 0x00; // disable interrupt for all
}

// ------------------------ DRAMCO UNO LIB ------------------------
void DramcoUnoClass::begin(LoraParam deveui, LoraParam appeui, LoraParam appkey){

    #ifdef DEBUG
    Serial.begin(DRAMCO_UNO_SERIAL_BAUDRATE);
    Serial.println("Started");
    #endif

	// copy and convert string (aka char *) to byte array
	char tempStr[3] = {0x00, 0x00, 0x00};
	// -> deveui
	for(uint8_t i = 0; i < DRAMCO_UNO_LORA_EUI_SIZE; i++){
		tempStr[0] = *(deveui+(i*2));
		tempStr[1] = *(deveui+(i*2)+1);
		*(_deveui+i) = (u1_t)strtol(tempStr, NULL, 16);
	}
	// -> appeui
	for(uint8_t i = 0; i < DRAMCO_UNO_LORA_EUI_SIZE; i++){
		tempStr[0] = *(appeui+(i*2));
		tempStr[1] = *(appeui+(i*2)+1);
		*(_appeui+i) = (u1_t)strtol(tempStr, NULL, 16);
	}
	// -> appkey
	for(uint8_t i = 0; i < DRAMCO_UNO_LORA_KEY_SIZE; i++){
		tempStr[0] = *(appkey+(i*2));
		tempStr[1] = *(appkey+(i*2)+1);
		*(_appkey+i) = (u1_t)strtol(tempStr, NULL, 16);
	}
    
	pinMode(DRAMCO_UNO_3V3_ENABLE_PIN, OUTPUT);
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);

    pinMode(DRAMCO_UNO_TEMPERATURE_SENSOR_ENABLE_PIN, OUTPUT);
    digitalWrite(DRAMCO_UNO_TEMPERATURE_SENSOR_ENABLE_PIN, HIGH);

    pinMode(DRAMCO_UNO_LED_NAME, OUTPUT);
    
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
	LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  	LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
	LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
	LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
	// TTN defines an additional channel at 869.525Mhz using SF9 for class B
	// devices' ping slots. LMIC does not have an easy way to define set this
	// frequency and support for class B is spotty and untested, so this
	// frequency is not configured here.

	// Disable link check validation
	LMIC_setLinkCheckMode(0);

	// required for downlink
	LMIC.dn2Dr = SF9;

	// Set data rate and transmit power (note: txpow seems to be ignored by the library)
	LMIC_setDrTxpow(DR_SF7, 14);

    _cursor = 0;

    // Initialize accelerometer int pin
    pinMode(DRAMCO_UNO_ACCELEROMTER_INT_PIN, INPUT);
    digitalWrite(DRAMCO_UNO_ACCELEROMTER_INT_PIN, HIGH);

    pinMode(DRAMCO_UNO_BUTTON_INT_PIN, INPUT);
    digitalWrite(DRAMCO_UNO_BUTTON_INT_PIN, HIGH);

    pinMode(DRAMCO_UNO_SOIL_PIN_EN, OUTPUT);
    digitalWrite(DRAMCO_UNO_SOIL_PIN_EN, LOW);
    
    calibrateTemperature();
}

// --- General UTILs ---
void DramcoUnoClass::blink(){
    digitalWrite(DRAMCO_UNO_LED_NAME, HIGH);
    _sleep(100);
    digitalWrite(DRAMCO_UNO_LED_NAME, LOW);
}

void DramcoUnoClass::loop(){
    os_runloop_once();
}

void DramcoUnoClass::delay(uint32_t d){
    Serial.flush();
    if(!packetReadyForTransmission)
        DramcoUnoClass::_sleep(d);
    else{
        unsigned long startMillis = millis();
        while (millis() - startMillis < d) {
            loop();
        }
    }
}

// --- Message related things ---
void DramcoUnoClass::sendWithOS(){
    do_send(&sendjob);
    //! Clear message yourself after transmit with board.clearMessage()
}

void DramcoUnoClass::send(){
    do_send(&sendjob);
    while(packetReadyForTransmission){ // This makes it blocking
        os_runloop_once();
    }    
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);
    // TODO: duty cycle limit
}

void DramcoUnoClass::clearMessage(){
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);
}

void DramcoUnoClass::_lppAddToBuffer(float val, uint8_t channel, uint8_t type, uint8_t size, uint16_t mult){
    // check buffer overflow
    if ((_cursor + size + 2) > DRAMCO_UNO_BUFFER_SIZE) {
        error(DRAMCO_UNO_ERROR_BUFFER);
    }

    data[_cursor++] = channel;
    data[_cursor++] = type;

    bool sign = val < 0;
    if (sign) val = -val;
    
    uint32_t v = val * mult;
    
    // format an uint32_t as if it was an int32_t
    if (sign) {
        uint32_t mask = (1 << (size * 8)) - 1;
        v = v & mask;
        if (sign) v = mask - v + 1;
    }

    for (uint8_t i=1; i<=size; i++) {
        data[_cursor + size - i] = (v & 0xFF);
        v >>= 8;
    }
    _cursor += size;
}

void DramcoUnoClass::_lppAddAcceleration(uint8_t channel, float x, float y, float z) {
    // check buffer overflow
    if ((_cursor + DRAMCO_UNO_LPP_ACCELEROMETER_SIZE + 2) > DRAMCO_UNO_BUFFER_SIZE) {
        error(DRAMCO_UNO_ERROR_BUFFER);
    }

    int16_t vx = x * DRAMCO_UNO_LPP_ACCELEROMETER_MULT;
    int16_t vy = y * DRAMCO_UNO_LPP_ACCELEROMETER_MULT;
    int16_t vz = z * DRAMCO_UNO_LPP_ACCELEROMETER_MULT;

    data[_cursor++] = channel;
    data[_cursor++] = DRAMCO_UNO_LPP_ACCELEROMETER;
    data[_cursor++] = vx >> 8;
    data[_cursor++] = vx;
    data[_cursor++] = vy >> 8;
    data[_cursor++] = vy;
    data[_cursor++] = vz >> 8;
    data[_cursor++] = vz;

}

// --- Sensor readings ---

// - Temperature 
void DramcoUnoClass::calibrateTemperature(){
    _calibration = readTemperatureAccelerometer()/readTemperature();
}

float DramcoUnoClass::readTemperature(){
    digitalWrite(DRAMCO_UNO_TEMPERATURE_SENSOR_ENABLE_PIN, HIGH);
    float average=0;
    analogReference(EXTERNAL);
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    _keep3V3Active = true;
    sleep(500); // Wait for voltage to stabilize
    _keep3V3Active = false;
    for(int i = 0; i < DRAMCO_UNO_TEMPERATURE_AVERAGE; i++){
        float value = (float)(analogRead(DRAMCO_UNO_TEMPERATURE_SENSOR_PIN))*DRAMCO_UNO_TEMPERATURE_CALIBRATE; //Calibrated value of 1024/3.3V (AREF tied to 3.3V reg)
        value = (8.194 - sqrt(67.1416+0.01048*(1324-value)))/(-0.00524)+30;
        average += value;
    }
    average=average/DRAMCO_UNO_TEMPERATURE_AVERAGE;
    return average*_calibration;
}

void DramcoUnoClass::addTemperature(){
    float temp = readTemperature();
    #ifdef DEBUG
    Serial.println(temp);
    #endif
    addTemperature(temp);
}

void DramcoUnoClass::addTemperature(float temperature){
    _lppAddToBuffer(temperature, 0, DRAMCO_UNO_LPP_TEMPERATURE, DRAMCO_UNO_LPP_TEMPERATURE_SIZE, DRAMCO_UNO_LPP_TEMPERATURE_MULT);
}

void DramcoUnoClass::addTemperatureToMessage(){
    addTemperature();
}

void DramcoUnoClass::addTemperatureToMessage(float temperature){
    addTemperature(temperature);
}

void DramcoUnoClass::sendTemperature(){
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);

    addTemperature();
    send();
}


// - Luminosity
float DramcoUnoClass::readLuminosity(){
    analogReference(EXTERNAL);
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    float value = analogRead(DRAMCO_UNO_LIGHT_SENSOR_PIN)*0.625; // max light value = 160, *100
    if(value <= 100)
        return value;
    else
        return 100.0;
}

void DramcoUnoClass::sendLuminosity(){
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);

    addLuminosity();
    send();
}

void DramcoUnoClass::addLuminosity(){
    addLuminosity(readLuminosity());
}

void DramcoUnoClass::addLuminosity(float luminosity){
    _lppAddToBuffer(luminosity, 0, DRAMCO_UNO_LPP_LUMINOSITY, DRAMCO_UNO_LPP_LUMINOSITY_SIZE, DRAMCO_UNO_LPP_LUMINOSITY_MULT);
}

void DramcoUnoClass::addLuminosityToMessage(){
    addLuminosity();
}

void DramcoUnoClass::addLuminosityToMessage(float luminosity){
    addLuminosity(luminosity);
}

// - Acceleration
float DramcoUnoClass::readAccelerationX(){
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    _accelerometer.begin();
    return _accelerometer.readFloatAccelX();
}

float DramcoUnoClass::readAccelerationY(){
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    _accelerometer.begin();
    return _accelerometer.readFloatAccelY();
}

float DramcoUnoClass::readAccelerationZ(){
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    _accelerometer.begin();
    return _accelerometer.readFloatAccelZ();
}

float DramcoUnoClass::readTemperatureAccelerometer(){
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
    _accelerometer.begin();
    #ifdef DEBUG
    Serial.println("acc begin end");
    #endif
    return _accelerometer.readTempC();
}

void DramcoUnoClass::addAcceleration(){
    _accelerometer.begin();
    float x = _accelerometer.readFloatAccelX();
    float y = _accelerometer.readFloatAccelY();
    float z = _accelerometer.readFloatAccelZ();
    #ifdef DEBUG
    Serial.print("x: ");
    Serial.println(x);
    Serial.print("y: ");
    Serial.println(y);
    Serial.print("z: ");
    Serial.println(z);
    #endif
    _lppAddAcceleration(0, x, y, z);
}

void DramcoUnoClass::addAccelerationToMessage(){
    addAcceleration();
}

void DramcoUnoClass::sendAcceleration(){
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);
    
    DramcoUnoClass::addAcceleration();
    DramcoUnoClass::send();
}

void DramcoUnoClass::delayUntilShake(){
    _accelerometer.begin();
    _keep3V3Active = true;
    _accelerometerIntEnabled = true;
    pciInit(9);
    _accelerometer.initDoubleTap(4);
    sleep(-1);
}

void DramcoUnoClass::delayUntilFall(){
    _accelerometer.begin();
    _keep3V3Active = true;
    _accelerometerIntEnabled = true;
    pciInit(9);
    _accelerometer.initFreefall();
    sleep(-1);
}

void DramcoUnoClass::delayUntilMotion(){
    _accelerometer.begin();
    _keep3V3Active = true;
    _accelerometerIntEnabled = true;
    pciInit(9);
    _accelerometer.initWakeUp();
    sleep(-1);
}

void DramcoUnoClass::delayUntilMovement(){
    delayUntilMovement();
}


void DramcoUnoClass::delayUntilFreeFall(){
    delayUntilFall();
}


// - Button
void DramcoUnoClass::delayUntilButtonPress(){
    _buttonIntEnabled = true;
    pciInit(10);
    sleep(-1);
}

// - Soil Moisture
float DramcoUnoClass::readSoilMoisture(){
    digitalWrite(DRAMCO_UNO_SOIL_PIN_EN, HIGH);
    byte ADCSRAoriginal = ADCSRA; 
    ADCSRA = (ADCSRA & B11111000) | 4; 
    float i = 0;
    while(analogRead(DRAMCO_UNO_SOIL_PIN_ANALOG) < 1000 && i < 20000){ // Read until analog value at 1000, count cycles
        i++;
    }
    ADCSRA = ADCSRAoriginal;
    digitalWrite(DRAMCO_UNO_SOIL_PIN_EN, LOW);
    sleep(500); // cap discharge delay
    

    float value = (float(i)) / DRAMCO_UNO_SOIL_DIVIDER;


    if (value > 100)
        return 100;
    else
        return value;
}

float DramcoUnoClass::readSoil(){
    return readSoilMoisture();
}

void DramcoUnoClass::addSoilMoisture(float soilMoisture){
    _lppAddToBuffer(soilMoisture, 0, DRAMCO_UNO_LPP_ANALOG_INPUT, DRAMCO_UNO_LPP_ANALOG_INPUT_SIZE, DRAMCO_UNO_LPP_ANALOG_INPUT_MULT);
}

void DramcoUnoClass::addSoilMoisture(){
    addSoilMoisture(readSoilMoisture());
}

void DramcoUnoClass::addSoil(float soilMoisture){
    addSoilMoisture(soilMoisture);
}

void DramcoUnoClass::addSoil(){
    addSoilMoisture();
}

void DramcoUnoClass::sendSoilMoisture(){
    _cursor = 0;
    memset(data, '\0', DRAMCO_UNO_BUFFER_SIZE);
    addSoilMoisture();
    send();
}

void DramcoUnoClass::sendSoil(){
    sendSoilMoisture();
}


// --- Sleep ---
void DramcoUnoClass::sleep(uint32_t d){
    #ifdef DEBUG
    Serial.flush();
    Serial.end();
    #endif
    DramcoUnoClass::_sleep(d);
}

void DramcoUnoClass::_sleep(unsigned long maxWaitTimeMillis) {
    
    if(!_keep3V3Active){
        digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, LOW);
        digitalWrite(DRAMCO_UNO_ACCELEROMTER_INT_PIN, LOW);
        digitalWrite(DRAMCO_UNO_TEMPERATURE_SENSOR_ENABLE_PIN, LOW);
    }
    
    
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);

    // Adapted from https://github.com/PRosenb/DeepSleepScheduler/blob/1595995576be62041a1c9db1d51435550ca49c53/DeepSleepScheduler_avr_implementation.h

    // Enable sleep bit with sleep_enable() before the sleep time evaluation because it can happen
    // that the WDT interrupt occurs during sleep time evaluation but before the CPU
    // sleeps. In that case, the WDT interrupt clears the sleep bit and the CPU will not sleep
    // but continue execution immediatelly.

    byte adcsraSave = ADCSRA;

    _millisInDeepSleep = 0;
    while( _millisInDeepSleep <= maxWaitTimeMillis-1){ // -1 for enabling to stop sleeping
        sleep_enable(); // enables the sleep bit, a safety pin
        
        _wdtSleepTimeMillis = DramcoUnoClass::_wdtEnableForSleep(maxWaitTimeMillis-_millisInDeepSleep);
        DramcoUnoClass::_wdtEnableInterrupt();

        noInterrupts();
        set_sleep_mode(SLEEP_MODE);
        
        ADCSRA = 0;  // disable ADC
        if(maxWaitTimeMillis > 10000 && !_keep3V3Active){
            UCSR0A = 0x00; 
            UCSR0B = 0x00;
            UCSR0C = 0x00;
        }

        // turn off brown-out in software
        #if defined(BODS) && defined(BODSE)
        sleep_bod_disable();
        #endif
        interrupts (); // guarantees next instruction executed
        sleep_cpu(); // here the device is actually put to sleep
        
        // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
    }
    // re-enable ADC
    ADCSRA = adcsraSave;
    
    sleep_disable();
    wdt_reset();
    wdt_disable();

    #ifdef DEBUG
    Serial.begin(DRAMCO_UNO_SERIAL_BAUDRATE);
    #endif
    digitalWrite(DRAMCO_UNO_ACCELEROMTER_INT_PIN, HIGH);
    digitalWrite(DRAMCO_UNO_3V3_ENABLE_PIN, HIGH);
}

unsigned long DramcoUnoClass::_wdtEnableForSleep(const unsigned long maxWaitTimeMillis) {
    // From https://github.com/PRosenb/DeepSleepScheduler/blob/1595995576be62041a1c9db1d51435550ca49c53/DeepSleepScheduler_avr_implementation.h#L173
    unsigned long wdtSleepTimeMillis;
    if (maxWaitTimeMillis >= SLEEP_TIME_8S ) {
        wdtSleepTimeMillis = SLEEP_TIME_8S;
        wdt_enable(WDTO_8S);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_4S ) {
        wdtSleepTimeMillis = SLEEP_TIME_4S;
        wdt_enable(WDTO_4S);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_2S ) {
        wdtSleepTimeMillis = SLEEP_TIME_2S;
        wdt_enable(WDTO_2S);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_1S ) {
        wdtSleepTimeMillis = SLEEP_TIME_1S;
        wdt_enable(WDTO_1S);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_500MS ) {
        wdtSleepTimeMillis = SLEEP_TIME_500MS;
        wdt_enable(WDTO_500MS);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_250MS ) {
        wdtSleepTimeMillis = SLEEP_TIME_250MS;
        wdt_enable(WDTO_250MS);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_120MS ) {
        wdtSleepTimeMillis = SLEEP_TIME_120MS;
        wdt_enable(WDTO_120MS);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_60MS ) {
        wdtSleepTimeMillis = SLEEP_TIME_60MS;
        wdt_enable(WDTO_60MS);
    } else if (maxWaitTimeMillis >= SLEEP_TIME_30MS ) {
        wdtSleepTimeMillis = SLEEP_TIME_30MS;
        wdt_enable(WDTO_30MS);
    } else { // maxWaitTimeMs >= 17
        wdtSleepTimeMillis = SLEEP_TIME_15MS;
        wdt_enable(WDTO_15MS);
    }
    return wdtSleepTimeMillis;
}

void DramcoUnoClass::_isrWdt() {
    sleep_disable();
    _millisInDeepSleep += _wdtSleepTimeMillis;
}

void DramcoUnoClass::_wdtEnableInterrupt() { 
    WDTCSR |= (1 << WDCE) | (1 << WDIE);
}

ISR (WDT_vect) {
    DramcoUnoClass::_isrWdt();
}

ISR (PCINT0_vect){ // handle pin change interrupt for D8 to D13 here  
    if(_accelerometerIntEnabled){
        if (!(DRAMCO_UNO_ACCELEROMTER_INT_PORT & _BV(DRAMCO_UNO_ACCELEROMTER_INT_NAME))){ // If pin 9 is low
            DramcoUnoClass::blink();
            pciDeinit();
            _millisInDeepSleep = -1; // Stop WDT sleep
            _keep3V3Active = false; // Accelerometer can shut up now
            _accelerometerIntEnabled = false;
        }
    }
    if(_buttonIntEnabled){
        if (!(DRAMCO_UNO_BUTTON_INT_PORT & _BV(DRAMCO_UNO_BUTTON_INT_NAME))){ // If pin 10 is low
            delay(50);
            if (!(DRAMCO_UNO_BUTTON_INT_PORT & _BV(DRAMCO_UNO_BUTTON_INT_NAME))){ // Debounce
                DramcoUnoClass::blink();
                pciDeinit();
                _millisInDeepSleep = -1; // Stop WDT sleep
                _buttonIntEnabled = false;
            }
        }
    }
    
}

void (*resetptr)( void ) = 0x0000;

void error(uint8_t errorcode){
    int i = 0;
    while(true){
        for(byte i = 0; i < errorcode; i++){
            digitalWrite(DRAMCO_UNO_LED_NAME, HIGH);
            delay(100);
            digitalWrite(DRAMCO_UNO_LED_NAME, LOW);
            delay(100);
        }
        delay(500);
        i++;
        if(i > 240){
            resetptr();
        }
    }
}

