# ESP32-S3 Zero + Tailscale (MicroLink) + Mongoose Web Dashboard

Projeto embarcado utilizando o microcontrolador **ESP32-S3-Zero**, conectado a uma rede VPN segura via protocolo **Tailscale** utilizando o componente nativo **MicroLink (v3.2.0)** e hospedando um servidor web HTTP leve via **Cesanta Mongoose** para controle de atuadores (LED) remotamente de qualquer lugar do mundo.

---

## 🚀 Tecnologias e Bibliotecas

- **Hardware:** Waveshare ESP32-S3-Zero (Xtensa Dual-Core LX7, 4MB Flash, 2MB PSRAM).
- **Framework:** ESP-IDF v6.0.x nativo.
- **Rede Privada (Mesh VPN):** [fugo101/microlink](https://components.espressif.com/components/fugo101/microlink) (Tailscale / WireGuard data plane).
- **Servidor Web:** [Cesanta Mongoose](https://github.com/cesanta/mongoose).

---

## 📌 Pinagem (ESP32-S3-Zero)

| Componente | Pino GPIO | Observação |
|---|---|---|
| **LED Externo** | \`GPIO 45\` (ou \`GPIO 42\`) | Saída digital comum (com resistor de proteção) |
| **LED RGB WS2812** | \`GPIO 21\` | LED endereçável embutido na placa |

---

## ⚙️ Configurações Importantes (\`idf.py menuconfig\`)

Para o correto funcionamento do WireGuard e do console no ESP32-S3 Zero, foram aplicadas as seguintes configurações essenciais:

1. **Console USB Nativo:**
   - \`Component config\` ➔ \`ESP-STDIO\` ➔ \`Channel for console output\` ➔ **USB Serial/JTAG Controller**.
2. **PSRAM (Memória externa para buffers de criptografia):**
   - \`Component config\` ➔ \`ESP PSRAM\` ➔ Marcar **Support for external, SPI-connected RAM** (Modo: Quad SPI).
3. **Memória Flash:**
   - \`Serial flasher config\` ➔ \`Flash size\` ➔ **4 MB**.
4. **Criptografia Simétrica MbedTLS (Obrigatório para o WireGuard/MicroLink):**
   - \`Component config\` ➔ \`mbedTLS\` ➔ \`Symmetric Ciphers\` ➔ Habilitar **ChaCha20**, **Poly1305** e **ChaCha20-Poly1305 AEAD algorithm**.

---

## 🔧 Estabilidade Elétrica de RF (WiFi)

Placas compactas com reguladores de tensão LDO pequenos (como a C3 Supermini e S3 Zero) podem sofrer quedas de tensão bruscas (*brownout*) durante a transmissão de RF na potência máxima. Foi implementada a atenuação de potência para **8.5 dBm** logo após a inicialização:

\`\`\`c
// ESP-IDF: Incrementos de 0.25 dBm (8.5 * 4 = 34)
esp_wifi_set_max_tx_power(34);
\`\`\`

---

## 🛠️ Como Compilar e Gravar

1. Clone o repositório:
   \`\`\`bash
   git clone https://github.com/SEU_USUARIO/tailscale_led.git
   cd tailscale_led
   \`\`\`

2. Abra o arquivo de código e configure suas credenciais de WiFi e a **Auth Key** gerada no painel do Tailscale:
   \`\`\`c
   #define WIFI_SSID       "SUA_REDE_WIFI"
   #define WIFI_PASSWORD   "SUA_SENHA_WIFI"
   #define TS_AUTH_KEY     "tskey-auth-xxxxxx-xxxxxxxxxxxxxxxxxxxx"
   \`\`\`

3. Defina o target para ESP32-S3:
   \`\`\`bash
   idf.py set-target esp32s3
   \`\`\`

4. Compile, grave e abra o monitor serial:
   \`\`\`bash
   idf.py build flash monitor
   \`\`\`

---

## 📱 Acessando o Dashboard

1. Após o boot, o terminal exibirá o handshake com os servidores DERP e a atribuição do endereço VPN seguro (faixa \`100.x.x.x\`).
2. Com qualquer dispositivo conectado à sua conta Tailscale (PC ou celular), abra o navegador e acesse:
   \`\`\`
   http://<IP_DO_TAILSCALE>
   \`\`\`
3. Use os botões da interface web para ligar ou desligar o LED em tempo real.
