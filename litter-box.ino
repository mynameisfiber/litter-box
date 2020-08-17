#include "HX711ADC.h"
#include <math.h>

// HX711.DOUT	- pin #A1
// HX711.PD_SCK	- pin #A0

HX711ADC scale(A1, A0);

/******* CONFIG *********/
static bool CALIBRATE = false;
static float SCALE = 152;  // Aquired by running calibration mode
static int N_READINGS = 10;
static unsigned int TARE_INTERVAL = 50;  // Number of low-res loops before taring
static double HIGHRES_CHANGE = 100;  // Grams changed in reading to trigger high-res
static double HIGHRES_TIMEOUT = 10;  // Number of seconds to stay in high-res mode
static byte server[] = { 192, 168, 1, 2 }; 
static int port = 2003;
/************************/

float prev_unit = -1;
time_t highres_start = 0;
unsigned int n_lowres = 0;

TCPClient client;

void setup() {
    Particle.publish("starting");
    scale.begin();
  
    if (CALIBRATE) {
        scale.set_scale();
        scale.tare(N_READINGS);
    } else {
        scale.set_scale(SCALE);
        scale.tare(N_READINGS);
    }
  
    Particle.function("tare", cloud_tare);
}

int cloud_tare(String extra) {
    Particle.publish("tare-cloud");
    scale.tare(N_READINGS);
    return 0;
}

void send_buffer(String buffer) {
    if (!WiFi.ready()) {
        Particle.publish("no-wifi");
        return;
    }
    
    if (!client.status()) {
        Particle.publish("graphite-connection-started");
        client.connect(server, port);
    }
    
    if (client.status())
    {
        Particle.publish("sending-to-server", buffer.c_str());
        client.println(buffer.c_str());
        client.flush();
    } else {
        Particle.publish("graphite-connection-failed");
    }
}

void send_float(char* field, float value) {
    time_t now = Time.now();
    String buffer = String::format("litterbox.%s %f %lu", field, value, now);
    send_buffer(buffer);
}

void send_long(char* field, long value) {
    time_t now = Time.now();
    String buffer = String::format("litterbox.%s %ld %lu", field, value, now);
    send_buffer(buffer);
}

int calculate_dt(float unit) {
    time_t now = Time.now();
    float change = unit - prev_unit;
    if (prev_unit <= 0) {
        change = 0;
    }
    prev_unit = unit;
    
    if (highres_start != 0 && now - highres_start <= HIGHRES_TIMEOUT) {
        Particle.publish("highres-staying");
        return 500;
    } else if (change > HIGHRES_CHANGE) {
        Particle.publish("highres-entering");
        highres_start = now;
        n_lowres = 0;
        return 500;
    }
    
    n_lowres += 1;
    Particle.publish("lowres");
    return 10000;
}

void calibrate() {
    // Calibrates the scale to grams using a 1kg weight
    long value = scale.get_value(N_READINGS);
    send_long("calibrate.value", value);
    send_long("calibrate.scale", value / 1000);
    scale.set_scale(value / 1000);
    
    long unit = scale.get_units(N_READINGS);
    send_long("calibrate.units", unit);
    delay(2500);
}

void loop() {
    if (CALIBRATE) {
        calibrate();
        return;
    }
    if ((n_lowres + 1) % TARE_INTERVAL == 0) {
        Particle.publish("taring");
        scale.tare(N_READINGS);
    }
    
    long reading = scale.read_median(N_READINGS);
    send_long("raw.read", reading);
    
    float units = scale.reading_to_units(reading);
    send_float("raw.units", units);
    
    delay(calculate_dt(units));
}
