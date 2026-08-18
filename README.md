

# ESP32 + Cloudflare Worker 物联网远程可视化控制系统

这是一个基于 **Cloudflare Worker (WebSocket)** 与 **ESP32** 的低延迟物联网远程控制与环境监控方案。支持在全世界任何地方通过网页远程控制 ESP32 绑定的继电器和 LED 灯，并实时查看传感器温湿度。用ESP32用作IOT的控制器，接 3 个 LED小灯泡，一个 4 路继电器，一个温湿度传感器。在公司，就可以通过网页远程控制家里的ESP32间接控制电器了。

项目零资金成本（利用 Cloudflare Worker 免费额度），且支持 GitHub 仓库绑定后的全自动 CI/CD 编译部署。

---

## 硬件清单

- **主控**：ESP32 开发板 (如 ESP32-WROOM-32)
- **温湿度传感器**：DHT11 或 DHT22 (接 GPIO 4)
- **LED 灯泡**：3 个 (接 GPIO 16, 17, 18)
- **继电器模块**：4 路继电器 (接 GPIO 19, 21, 22, 23)

---

## 项目结构

```text
esp32-cf-iot/
├── .gitignore                  # Git 忽略文件（防止上传敏感秘钥）
├── package.json                # NPM 项目依赖管理
├── wrangler.toml               # Cloudflare Worker 配置文件
├── README.md                   # 项目说明
├── src/
│   └── index.js                # Cloudflare Worker 服务端 (WebSocket 中继 + WebGUI 界面)
└── firmware/
    └── esp32-firmware/
        └── esp32-firmware.ino  # ESP32 端 C++ 源代码

```

---

## 代码功能与作用

1. **`src/index.js` (Worker 端)**：
* **WebSocket 服务**：作为长连接消息中枢，实现 ESP32 与网页客户端之间的双向数据指令实时透传。
* **Web 控制面板**：在浏览器访问 Worker 域名时返回 HTML 控制界面，提供按键控制及温湿度仪表盘。
* **安全鉴权**：校验连接请求中的 `AUTH_SECRET` 秘钥，防止未授权访问。


2. **`firmware/esp32-firmware/esp32-firmware.ino` (ESP32 端)**：
* **网页热点配网 (WiFiManager)**：无需将 Wi-Fi 密码和 Worker 域名硬编码在代码中。首次开机自动启动热点，手机连接后即可通过网页配置网络与服务秘钥。
* **硬件控制**：响应 Web 端发出的控制指令，独立操作 3 个 LED 及 4 路继电器。
* **自动化控制逻辑**：内置本地温湿度联动逻辑（如温度高于阈值自动开启特定继电器/LED）。即使与服务器断开连接，本地自动化逻辑依然安全运行。



---

## 部署与环境变量配置

### 1. Cloudflare Worker 侧配置

#### 步骤 A：绑定 GitHub 自动部署

1. 登录 [Cloudflare Dashboard](https://dash.cloudflare.com/)。
2. 进入 **Workers & Pages** -> **Create application** -> 选择 **Pages** 或 **Workers**，选择 **Connect to Git**。
3. 授权绑定你的 GitHub 账户，选择 `esp32-cf-iot` 仓库。
4. 构建设置保留默认即可（Build command 保持为空，Root directory 为 `/`），点击 **Save and Deploy**。之后每次向 `main` 分支提交代码，Cloudflare 将自动同步更新。

#### 步骤 B：配置环境变量 (`AUTH_SECRET`)

为了防止外部非法侵入你的控制中枢，需在 Cloudflare 设置设备通信密钥：

1. 在 Cloudflare Dashboard 中，进入部署好的 Worker。
2. 依次点击 **Settings** -> **Variables and Secrets**。
3. 在 **Environment Variables** 处新增一个变量：
* **Variable name**: `AUTH_SECRET`
* **Value**: `自定义一串安全密码` (例如: `MyIotPasswd_2026`)


4. 点击 **Save and Deploy** 刷新 Worker。

---

### 2. ESP32 端烧录与配网

#### 步骤 A：依赖库安装

在 Arduino IDE 中，打开 **Tools -> Manage Libraries** 安装以下依赖：

* `WiFiManager` (by tzapu)
* `WebSockets` (by Markus Sattler)
* `ArduinoJson` (by Benoit Blanchon)
* `DHT sensor library` (by Adafruit)

#### 步骤 B：烧录与首次热点配置

1. 打开 `firmware/esp32-firmware/esp32-firmware.ino` 并直接烧录至 ESP32。
2. ESP32 启动后如果无法连接已知 Wi-Fi，会自动开启名为 **`ESP32-AP-Config`** 的 Wi-Fi 热点。
3. 用手机或电脑连接该热点，会自动弹出（或手动浏览器访问 `192.168.4.1`）配网页面。
4. 在页面中填写：
* 家里的 **Wi-Fi 名称与密码**。
* **CF Worker 域名**：你的 Worker 访问 URL（无需带 `https://`，如 `esp32-cf-iot.your-name.workers.dev`）。
* **Auth Secret 密钥**：填写上一步在 Cloudflare 后台设置的 `AUTH_SECRET` 变量值。


5. 保存后 ESP32 将自动重启并建立长连接。

---

## 本地开发调试

项目根目录下已配置 `wrangler` 支持本地运行：

```bash
# 安装依赖
npm install

# 启动本地开发服务器调试
npm run dev

```
