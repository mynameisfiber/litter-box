#include "HX711ADC.h"
#include <math.h>
#include "HttpClient.h"

// HX711.DOUT	- pin #A1
// HX711.PD_SCK	- pin #A0

HX711ADC scale(A1, A0);

/******* CONFIG *********/
static bool CALIBRATE = false;
static float SCALE = -24;  // Aquired by running calibration mode
static int N_READINGS = 10;
static unsigned int TARE_INTERVAL = 50;  // Number of low-res loops before taring
static double HIGHRES_CHANGE = 0.25;  // Percent change in reading to trigger high-res
static double HIGHRES_TIMEOUT = 10;  // Number of seconds to stay in high-res mode
static byte server[] = { 192, 168, 1, 2 }; 
static char litter_box_host[] = "192.168.1.2";
static int graphite_port = 2003;
static int litter_box_port = 11773;
/************************/

time_t highres_start = 0;
unsigned int n_lowres = 0;
float unit_last = 0;

TCPClient client_graphite;
HttpClient client_litter_box;

http_header_t headers[] = {
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;

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
    Particle.function("scale", cloud_scale);
}

int cloud_tare(String extra) {
    Particle.publish("tare-cloud");
    scale.tare(N_READINGS);
    return 0;
}

int cloud_scale(String extra) {
    int units_expected = extra.toInt();
    int value = scale.get_value(N_READINGS);
    float scale_value = value / units_expected;
    scale.set_scale(scale_value);
    Particle.publish("scale-cloud", String(scale_value).c_str());
    return 0;
}

void send_buffer(String buffer) {
    if (!WiFi.ready()) {
        Particle.publish("no-wifi");
        return;
    }
    
    if (!client_graphite.status()) {
        Particle.publish("graphite-connection-started");
        client_graphite.connect(server, graphite_port);
    }
    
    if (client_graphite.status())
    {
        client_graphite.println(buffer.c_str());
        client_graphite.flush();
    } else {
        Particle.publish("graphite-connection-failed");
    }
}

void send_litter_box(float value) {
    request.hostname = litter_box_host;
    request.port = litter_box_port;
    String path = String::format("/measure?value=%f", value);
    request.path = path.c_str();

    client_litter_box.get(request, response, headers);
    if (response.status == 200) {
        Particle.publish("litter_box_write_success", response.body.c_str());
    } else {
        Particle.publish("litter_box_write_failure", response.body.c_str());
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
    float rel_change = 1 - unit / unit_last;
    unit_last = unit;
    String message = String(rel_change);

    if (highres_start != 0 && now - highres_start <= HIGHRES_TIMEOUT) {
        Particle.publish("highres-staying", message.c_str());
        return 500;
    } else if (rel_change > HIGHRES_CHANGE && n_lowres >= 1) {
        Particle.publish("highres-entering", message.c_str());
        highres_start = now;
        n_lowres = 0;
        return 500;
    }
    
    n_lowres += 1;
    Particle.publish("lowres", message.c_str());
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
    send_litter_box(units);
    
    delay(calculate_dt(units));
}
