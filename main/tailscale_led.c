#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "microlink.h"
#include "mongoose.h"
#include "dht.h"       // Biblioteca do sensor DHT
#include "secrets.h"   // Suas credenciais seguras

#define LED_PIN         2  // Pino do seu LED
#define DHT_PIN         4  // Pino de dados do DHT11 (GP4)

static const char *TAG = "TS_IOT_DASHBOARD";
static bool s_led_state = false;
static float s_temperature = 0.0f;
static float s_humidity = 0.0f;

// Frontend Mongoose Device Dashboard atualizado com Temperatura e Umidade
static const char *html_dashboard =
"<!DOCTYPE html><html lang='pt'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>ESP32-S3 IoT Dashboard</title>"
"<style>"
":root { --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --muted: #94a3b8; --accent: #3b82f6; --green: #22c55e; --orange: #f97316; --cyan: #06b6d4; }"
"body { font-family: system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 24px; }"
".container { max-width: 800px; margin: 0 auto; }"
"header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; border-bottom: 1px solid #334155; padding-bottom: 16px; }"
".badge { padding: 6px 12px; border-radius: 9999px; font-size: 13px; font-weight: 600; background: #334155; }"
".badge.online { background: rgba(34,197,94,0.2); color: var(--green); }"
".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 16px; margin-bottom: 24px; }"
".card { background: var(--card); border-radius: 12px; padding: 20px; border: 1px solid #334155; }"
".card-title { color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; }"
".card-value { font-size: 26px; font-weight: 700; }"
".control-row { display: flex; justify-content: space-between; align-items: center; }"
".switch { position: relative; display: inline-block; width: 60px; height: 34px; }"
".switch input { opacity: 0; width: 0; height: 0; }"
".slider { position: absolute; cursor: pointer; inset: 0; background-color: #475569; transition: .3s; border-radius: 34px; }"
".slider:before { position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .3s; border-radius: 50%; }"
"input:checked + .slider { background-color: var(--accent); }"
"input:checked + .slider:before { transform: translateX(26px); }"
"</style></head><body>"
"<div class='container'>"
"  <header>"
"    <h2 style='margin:0'>ESP32-S3 Tailscale Node</h2>"
"    <span id='ws-status' class='badge'>A ligar...</span>"
"  </header>"
"  <div class='grid'>"
"    <div class='card'><div class='card-title'>Temperatura (DHT11)</div><div class='card-value' id='temp' style='color:var(--orange)'>-- °C</div></div>"
"    <div class='card'><div class='card-title'>Umidade (DHT11)</div><div class='card-value' id='hum' style='color:var(--cyan)'>-- %</div></div>"
"    <div class='card'><div class='card-title'>Estado do LED</div><div class='card-value' id='led-text'>DESLIGADO</div></div>"
"    <div class='card'><div class='card-title'>RAM Livre</div><div class='card-value' id='ram'>-- KB</div></div>"
"    <div class='card'><div class='card-title'>Uptime</div><div class='card-value' id='uptime'>-- s</div></div>"
"  </div>"
"  <div class='card control-row'>"
"    <div><h3 style='margin:0 0 4px 0'>Controlo de Saída Digital (GP2)</h3><span style='color:var(--muted);font-size:14px'>Acionamento em tempo real via WebSocket/REST</span></div>"
"    <label class='switch'><input type='checkbox' id='led-toggle' onchange='toggleLed(this.checked)'><span class='slider'></span></label>"
"  </div>"
"</div>"
"<script>"
"let ws;"
"function updateUI(data) {"
"  if (data.temp !== undefined) document.getElementById('temp').innerText = data.temp.toFixed(1) + ' °C';"
"  if (data.hum !== undefined) document.getElementById('hum').innerText = data.hum.toFixed(1) + ' %';"
"  if (data.ram !== undefined) document.getElementById('ram').innerText = (data.ram / 1024).toFixed(1) + ' KB';"
"  if (data.uptime !== undefined) document.getElementById('uptime').innerText = data.uptime + ' s';"
"  if (data.led !== undefined) {"
"    document.getElementById('led-toggle').checked = data.led;"
"    const el = document.getElementById('led-text');"
"    el.innerText = data.led ? 'LIGADO' : 'DESLIGADO';"
"    el.style.color = data.led ? '#22c55e' : '#f8fafc';"
"  }"
"}"
"function connectWS() {"
"  ws = new WebSocket('ws://' + location.host + '/api/ws');"
"  ws.onopen = () => { const b = document.getElementById('ws-status'); b.innerText = 'Conectado (WS)'; b.className = 'badge online'; };"
"  ws.onmessage = (ev) => updateUI(JSON.parse(ev.data));"
"  ws.onclose = () => { const b = document.getElementById('ws-status'); b.innerText = 'Desconectado'; b.className = 'badge'; setTimeout(connectWS, 2000); };"
"}"
"function toggleLed(state) {"
"  fetch('/api/led', { method: 'POST', body: JSON.stringify({ on: state }) });"
"}"
"connectWS();"
"</script></body></html>";

// Envia JSON com LED, RAM, Uptime, Temperatura e Umidade via WebSocket
static void send_status_ws(struct mg_connection *c) {
    uint32_t free_ram = esp_get_free_heap_size();
    uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    mg_ws_printf(c, WEBSOCKET_OP_TEXT,
                 "{\"led\":%s,\"ram\":%lu,\"uptime\":%lu,\"temp\":%.1f,\"hum\":%.1f}",
                 s_led_state ? "true" : "false",
                 (unsigned long)free_ram,
                 (unsigned long)uptime,
                 s_temperature,
                 s_humidity);
}

static void broadcast_state(struct mg_mgr *mgr) {
    for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
        if (c->data[0] == 'W') {
            send_status_ws(c);
        }
    }
}

static void timer_metrics_fn(void *arg) {
    struct mg_mgr *mgr = (struct mg_mgr *) arg;
    broadcast_state(mgr);
}

static void http_server_fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        if (mg_match(hm->uri, mg_str("/api/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            c->data[0] = 'W';
            send_status_ws(c);
        }
        else if (mg_match(hm->uri, mg_str("/api/led"), NULL)) {
            bool new_state = s_led_state;
            if (mg_json_get_bool(hm->body, "$.on", &new_state)) {
                s_led_state = new_state;
                gpio_set_level(LED_PIN, s_led_state ? 1 : 0);
                ESP_LOGI(TAG, "LED atualizado: %s", s_led_state ? "ON" : "OFF");
                broadcast_state(c->mgr);
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "{\"led\":%s}", s_led_state ? "true" : "false");
        }
        else if (mg_match(hm->uri, mg_str("/api/stats"), NULL)) {
            uint32_t free_ram = esp_get_free_heap_size();
            uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "{\"led\":%s,\"ram\":%lu,\"uptime\":%lu,\"temp\":%.1f,\"hum\":%.1f}",
                          s_led_state ? "true" : "false",
                          (unsigned long)free_ram,
                          (unsigned long)uptime,
                          s_temperature,
                          s_humidity);
        }
        else {
            mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\n", "%s", html_dashboard);
        }
    }
}

// Tarefa dedicada para ler o sensor DHT11 a cada 3 segundos
static void dht_task(void *param) {
    // Ativa o pull-up interno do pino para ajudar no sinal do DHT11
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Aguarda estabilização inicial do sensor

    while (1) {
        float temp = 0.0f, hum = 0.0f;
        if (dht_read_float_data(DHT_TYPE_DHT11, DHT_PIN, &hum, &temp) == ESP_OK) {
            s_temperature = temp;
            s_humidity = hum;
            ESP_LOGI(TAG, "DHT11 -> Temp: %.1f C | Umidade: %.1f %%", s_temperature, s_humidity);
        } else {
            ESP_LOGW(TAG, "Falha ao ler DHT11 no pino %d (verifique os fios)", DHT_PIN);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static void mongoose_task(void *param) {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    mg_timer_add(&mgr, 2000, MG_TIMER_REPEAT, timer_metrics_fn, &mgr);
    mg_http_listen(&mgr, "http://0.0.0.0:80", http_server_fn, &mgr);
    ESP_LOGI(TAG, "Mongoose Dashboard rodando na porta 80");

    while (1) {
        mg_mgr_poll(&mgr, 100);
    }
    mg_mgr_free(&mgr);
}

static void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_restore();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    esp_wifi_set_max_tx_power(34); // 8.5 dBm
    esp_wifi_connect();

    int tentativas = 0;
    wifi_ap_record_t ap_info;
    while (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK && tentativas < 25) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        tentativas++;
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    wifi_init_sta();

    microlink_config_t ml_cfg = {
        .auth_key = TS_AUTH_KEY,
        .enable_derp = true,
        .enable_disco = true,
        .enable_stun = true,
        .device_name = "esp32-zero-dashboard"
    };

    microlink_t* ml = microlink_init(&ml_cfg);
    if (ml) {
        microlink_start(ml);
    }

    // Inicia a task de leitura do DHT11 e a task do servidor web
    xTaskCreate(dht_task, "dht_task", 4096, NULL, 4, NULL);
    xTaskCreate(mongoose_task, "mongoose_task", 8192, NULL, 5, NULL);
}
