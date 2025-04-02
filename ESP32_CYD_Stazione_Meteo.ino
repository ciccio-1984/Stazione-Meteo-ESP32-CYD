
// Copyright - Mirko Corazzesi - aka Ciccio-1984
// modificato su progetto originale di Rui Santos & Sara Santos
// *modified from original design by Rui Santos & Sara Santos "https://randomnerdtutorials.com/esp32-cyd-lvgl-weather-station/"*
// https://open-meteo.com/
// Ottimizzato per la scheda ESP32-2432S028R - *optimize for board ESP32-2432S028R*


#include <lvgl.h>
#include <TFT_eSPI.h>
#include "weather_images.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Credenziali di accesso alla rete
const char* ssid = "il nome della tua rete";
const char* password = "la password della tua rete";

// Variabili per il timer
bool wifiConnessa = false;
unsigned long previousMillis = 0;
const long interval = 30 * 1000; // 30 secondi

// Sostituisci con latitudine e longitudine per ottenere le previsioni meteo dal sito "https://open-meteo.com/"
String latitude = "Latitudine quì";
String longitude = "Longitudine quì";
// Inserisci la tua posizione
String location = "Posizione quì";
// Digita il fuso orario per cui vuoi ottenere l'ora, espempio Europe/Rome
String timezone = "Europe/Rome";

String current_date;
String last_weather_update;
String temperature;
String humidity;
int is_day;
int weather_code = 0;
String weather_description;

// IMPOSTA LA VARIABILE SU 0 PER LA TEMPERATURA IN GRADI FAHRENHEIT
#define TEMP_CELSIUS 1

#if TEMP_CELSIUS
  String temperature_unit = "";
  const char degree_symbol[] = "\u00B0C";
#else
  String temperature_unit = "&temperature_unit=fahrenheit";
  const char degree_symbol[] = "\u00B0F";
#endif

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

static lv_obj_t * weather_image;
static lv_obj_t * text_label_date;
static lv_obj_t * text_label_temperature;
static lv_obj_t * text_label_humidity;
static lv_obj_t * text_label_weather_description;
static lv_obj_t * text_label_time_location;

static void timer_cb(lv_timer_t * timer){
  LV_UNUSED(timer);
  get_weather_data();
  get_weather_description(weather_code);
  lv_label_set_text(text_label_date, current_date.c_str());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_label_set_text(text_label_time_location, String("Aggiornamento: " + last_weather_update + "  |  " + location).c_str());
}

void lv_create_main_gui(void) {
  LV_IMAGE_DECLARE(image_weather_sun);
  LV_IMAGE_DECLARE(image_weather_cloud);
  LV_IMAGE_DECLARE(image_weather_rain);
  LV_IMAGE_DECLARE(image_weather_thunder);
  LV_IMAGE_DECLARE(image_weather_snow);
  LV_IMAGE_DECLARE(image_weather_night);
  LV_IMAGE_DECLARE(image_weather_temperature);
  LV_IMAGE_DECLARE(image_weather_humidity);

  get_weather_data();

  weather_image = lv_image_create(lv_screen_active());
  lv_obj_align(weather_image, LV_ALIGN_CENTER, -80, -20);
  
  get_weather_description(weather_code);

  text_label_date = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_date, current_date.c_str());
  lv_obj_align(text_label_date, LV_ALIGN_CENTER, 70, -70);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_date, &lv_font_montserrat_26, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_date, lv_palette_main(LV_PALETTE_TEAL), 0);

  lv_obj_t * weather_image_temperature = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_temperature, &image_weather_temperature);
  lv_obj_align(weather_image_temperature, LV_ALIGN_CENTER, 30, -25);
  text_label_temperature = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_obj_align(text_label_temperature, LV_ALIGN_CENTER, 70, -25);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_temperature, &lv_font_montserrat_22, 0);

  lv_obj_t * weather_image_humidity = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_humidity, &image_weather_humidity);
  lv_obj_align(weather_image_humidity, LV_ALIGN_CENTER, 30, 20);
  text_label_humidity = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_obj_align(text_label_humidity, LV_ALIGN_CENTER, 70, 20);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_humidity, &lv_font_montserrat_22, 0);

  text_label_weather_description = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_obj_align(text_label_weather_description, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_weather_description, &lv_font_montserrat_18, 0);

  // Create a text label for the time and timezone aligned center in the bottom of the screen
  text_label_time_location = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_time_location, String("Aggiornamento: " + last_weather_update + "  |  " + location).c_str());
  lv_obj_align(text_label_time_location, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_time_location, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_time_location, lv_palette_main(LV_PALETTE_GREY), 0);

  lv_timer_t * timer = lv_timer_create(timer_cb, 600000, NULL);
  lv_timer_ready(timer);
}

void get_weather_description(int code) {
  switch (code) {
    case 0:
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "CIELO LIMPIDO";
      break;
    case 1: 
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "CIELO VELATO";
      break;
    case 2: 
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "PARZIALMENTE NUVOLOSO";
      break;
    case 3:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "NUVOLOSO";
      break;
    case 45:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "NEBBIA";
      break;
    case 48:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "NEBBIA GHIACCIATA";
      break;
    case 51:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA DI LEGGERA INTENSITA'";
      break;
    case 53:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA DI MODERATA INTENSITA'";
      break;
    case 55:
      lv_image_set_src(weather_image, &image_weather_rain); 
      weather_description = "PIOGGIA INTENSA";
      break;
    case 56:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA CONGELATA LEGGERA";
      break;
    case 57:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA CONGELATA INTENSA";
      break;
    case 61:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA LEGGERA";
      break;
    case 63:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA MODERATA";
      break;
    case 65:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA PESANTE";
      break;
    case 66:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA GHIACCIATA LEGGERA";
      break;
    case 67:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA GHIACCIATA PESANTE";
      break;
    case 71:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "NEVISCHIO";
      break;
    case 73:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "NEVE MODERATA";
      break;
    case 75:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "NEVE INTENSA";
      break;
    case 77:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "GRANELLI DI NEVE";
      break;
    case 80:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA LEGGERA";
      break;
    case 81:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA MODERATA";
      break;
    case 82:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "PIOGGIA FORTE";
      break;
    case 85:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "NEVE LEGGERA";
      break;
    case 86:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "NEVE PESANTE";
      break;
    case 95:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "TEMPORALE";
      break;
    case 96:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "GRANDINE LEGGERA";
      break;
    case 99:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "GRANDINE PESANTE";
      break;
    default: 
      weather_description = "CODICE METEO SCONOSCIUTO";
      break;
  }
}

void connectToWiFi() {
  Serial.print("Connettendo a WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // aspetta che la connessione venga stabilita o fallisca dopo 10s
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(100);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connesso ");
  } else {
    Serial.println ("Connessione fallita ");
  }
}



void get_weather_data() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://api.open-meteo.com/v1/forecast?latitude=" + latitude + "&longitude=" + longitude + "&current=temperature_2m,relative_humidity_2m,is_day,precipitation,rain,weather_code" + temperature_unit + "&timezone=" + timezone + "&forecast_days=1");
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* datetime = doc["current"]["time"];
          temperature = String(doc["current"]["temperature_2m"]);
          humidity = String(doc["current"]["relative_humidity_2m"]);
          is_day = String(doc["current"]["is_day"]).toInt();
          weather_code = String(doc["current"]["weather_code"]).toInt();
          String datetime_str = String(datetime);
          int splitIndex = datetime_str.indexOf('T');
          current_date = datetime_str.substring(0, splitIndex);
          last_weather_update = datetime_str.substring(splitIndex + 1, splitIndex + 9);
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
      else {
        Serial.println("Fallito");
      }
    } else {
      Serial.printf("Richiesta fallita, errore: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Connessione WiFi assente");
  }
}

void turnOffWiFi() {
  WiFi.disconnect(true); // Disconnette il WiFi
  WiFi.mode(WIFI_OFF);   // Spegne il modulo WiFi
  btStop();              // Spegne il modulo Bluetooth, se attivo, per risparmiare energia
  Serial.println("WiFi e Bluetooth spenti.");
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);
  
  lv_init();  
  lv_log_register_print_cb(log_print);  
  lv_display_t * disp;  
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);  
  
  lv_create_main_gui();

}

void loop() {
  unsigned long currentMillis = millis();
  // Connessione alla WiFi ogni ora (3600000 ms)
  if (!wifiConnessa && (currentMillis % 3600000 < interval)) {
    connectToWiFi();
    previousMillis = currentMillis;
    wifiConnessa = true;
  }
  // Altre operazioni da fare durante i 30 secondi
  if (wifiConnessa) {
    get_weather_data();
    Serial.println("Sto aggiornando i dati meteo...");
  }
  // Disconnessione dopo 30 secondi
  if (wifiConnessa && (currentMillis - previousMillis >= interval)) {
    turnOffWiFi();
    wifiConnessa = false;
  }
  lv_task_handler();
  lv_tick_inc(5);
  delay(5);
}
