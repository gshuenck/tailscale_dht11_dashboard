ESP32-S3 Zero + Tailscale (MicroLink) + Mongoose Dashboard & DHT11

Projeto IoT embarcado utilizando o microcontrolador **ESP32-S3-Zero**, conectado diretamente a uma rede VPN privada via protocolo **Tailscale** (**MicroLink v3.2.0**) e hospedando um **Device Dashboard em tempo real (WebSockets)** com **Cesanta Mongoose** para monitoramento de Temperatura/Umidade (**DHT11**), telemetria de hardware e acionamento de atuadores (LED) de qualquer lugar do mundo.

---

## 🚀 Tecnologias e Bibliotecas

- **Hardware:** Waveshare ESP32-S3-Zero (Xtensa Dual-Core LX7, 4MB Flash, 2MB PSRAM) + Sensor DHT11.
- **Framework:** ESP-IDF v6.0.x nativo.
- **Rede Privada (Mesh VPN):** [fugo101/microlink](https://components.espressif.com/components/fugo101/microlink) (Tailscale / WireGuard data plane).
- **Servidor Web & WebSockets:** [Cesanta Mongoose](https://github.com/cesanta/mongoose).
- **Driver do Sensor:** [esp-idf-lib/dht](https://components.espressif.com/components/esp-idf-lib/dht).

---

## ✨ Funcionalidades do Dashboard

- **Monitoramento Climático:** Leitura assíncrona de Temperatura (°C) e Umidade Relativa (%) via task dedicada no FreeRTOS a cada 3 segundos.
- **Comunicação Bidirecional (WebSockets):** Atualização instantânea da interface web sem necessidade de recarregar a página (`mg_ws_upgrade`).
- **Telemetria do Sistema:** Exibição de memória RAM livre (Heap) e tempo de atividade (Uptime) a cada 2 segundos.
- **Controle Remoto Seguro:** Acionamento de saída digital acessível apenas por dispositivos autenticados na mesma malha Tailscale (`100.x.x.x`).

---

## 📌 Pinagem (ESP32-S3-Zero)

| Componente | Pino GPIO | Observação |
|---|---|---|
| **LED Externo** | `GPIO 2` | Saída digital comum (com resistor de proteção) |
| **Sensor DHT11 (DATA)** | `GPIO 4` | Pino de dados (com pull-up interno ativado via software) |
| **Sensor DHT11 (VCC / GND)** | `3V3` / `GND` | Alimentação 3.3V da placa |
| **LED RGB WS2812** | `GPIO 21` | LED endereçável embutido na placa |

---

## ⚙️ Configurações Importantes (`idf.py menuconfig`)

Para o correto funcionamento do WireGuard, Mongoose e do console no ESP32-S3 Zero, as seguintes opções foram habilitadas:

1. **Console USB Nativo:**
   - `Component config` ➔ `ESP-STDIO` ➔ `Channel for console output` ➔ **USB Serial/JTAG Controller**.
2. **PSRAM (Memória externa para buffers de criptografia):**
   - `Component config` ➔ `ESP PSRAM` ➔ Marcar **Support for external, SPI-connected RAM** (Modo: Quad SPI).
3. **Memória Flash:**
   - `Serial flasher config` ➔ `Flash size` ➔ **4 MB**.
4. **Criptografia Simétrica MbedTLS (Obrigatório para o WireGuard/MicroLink):**
   - `Component config` ➔ `mbedTLS` ➔ `Symmetric Ciphers` ➔ Habilitar **ChaCha20**, **Poly1305** e **ChaCha20-Poly1305 AEAD algorithm**.

---

## 🔧 Estabilidade Elétrica de RF (WiFi)

Placas compactas com reguladores de tensão LDO pequenos podem sofrer quedas de tensão (*brownout*) durante picos de transmissão WiFi. Foi implementada a redução da potência máxima de TX para **8.5 dBm**:

// ESP-IDF: Incrementos de 0.25 dBm (8.5 * 4 = 34)
esp_wifi_set_max_tx_power(34);

---

## 🔐 Segurança e Credenciais (`secrets.h`)

Para evitar o vazamento de dados sensíveis (senha do WiFi e chave de autenticação da VPN) em repositórios públicos no GitHub, as credenciais **não ficam no código-fonte principal**.

1. **Arquivo Ignorado pelo Git:** O arquivo real de senhas (`main/secrets.h`) está listado no `.gitignore` e existe apenas localmente na máquina de desenvolvimento.
2. **Arquivo de Modelo:** O repositório conta com o arquivo `main/secrets.example.h`, que serve como molde para quem clonar o projeto.
3. **Como configurar suas credenciais:**
   Copie o arquivo de exemplo para criar o seu `secrets.h` local:
   ```bash
   cp main/secrets.example.h main/secrets.h
