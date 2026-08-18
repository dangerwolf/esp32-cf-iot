export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    // 1. 处理 WebSocket 长连接中继
    if (url.pathname === "/ws") {
      if (request.headers.get("Upgrade") !== "websocket") {
        return new Response("Expected WebSocket", { status: 426 });
      }

      // 验证连接 Key
      const clientAuth = url.searchParams.get("auth");
      if (env.AUTH_SECRET && clientAuth !== env.AUTH_SECRET) {
        return new Response("Unauthorized", { status: 401 });
      }

      const pair = new WebSocketPair();
      const [client, server] = Object.values(pair);

      if (!globalThis.sockets) globalThis.sockets = new Set();
      globalThis.sockets.add(server);

      server.accept();
      server.addEventListener("message", (event) => {
        // 广播控制指令与数据给所有连接端
        for (const socket of globalThis.sockets) {
          if (socket !== server && socket.readyState === 1) {
            socket.send(event.data);
          }
        }
      });

      server.addEventListener("close", () => globalThis.sockets.delete(server));
      return new Response(null, { status: 101, webSocket: client });
    }

    // 2. 网页控制面板 HTML
    const html = `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 IoT 控制中心</title>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 600px; margin: 20px auto; padding: 0 10px; background: #f0f2f5; }
    .card { background: white; padding: 15px; border-radius: 8px; margin-bottom: 15px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
    .btn { padding: 10px; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; width: 100%; color: white; }
    .on { background: #52c41a; } .off { background: #ff4d4f; }
    .auto-mode { background: #e6f7ff; border: 1px solid #91d5ff; padding: 10px; border-radius: 5px; margin-top: 10px; }
  </style>
</head>
<body>
  <h2>ESP32 智能控制中心</h2>
  
  <div class="card">
    <h3>环境监控与自动化开关</h3>
    <p>温度: <b id="temp">--</b> °C | 湿度: <b id="humi">--</b> %</p>
    <label>
      <input type="checkbox" id="autoSwitch" onchange="toggleAuto(this.checked)"> **开启温湿度自动联动控制**
    </label>
  </div>

  <div class="card">
    <h3>LED 小灯泡 (3路)</h3>
    <div class="grid">
      <button class="btn on" onclick="send('LED1_ON')">LED 1 开</button><button class="btn off" onclick="send('LED1_OFF')">LED 1 关</button>
      <button class="btn on" onclick="send('LED2_ON')">LED 2 开</button><button class="btn off" onclick="send('LED2_OFF')">LED 2 关</button>
      <button class="btn on" onclick="send('LED3_ON')">LED 3 开</button><button class="btn off" onclick="send('LED3_OFF')">LED 3 关</button>
    </div>
  </div>

  <div class="card">
    <h3>继电器 (4路)</h3>
    <div class="grid">
      <button class="btn on" onclick="send('R1_ON')">继电器 1 开</button><button class="btn off" onclick="send('R1_OFF')">继电器 1 关</button>
      <button class="btn on" onclick="send('R2_ON')">继电器 2 开</button><button class="btn off" onclick="send('R2_OFF')">继电器 2 关</button>
      <button class="btn on" onclick="send('R3_ON')">继电器 3 开</button><button class="btn off" onclick="send('R3_OFF')">继电器 3 关</button>
      <button class="btn on" onclick="send('R4_ON')">继电器 4 开</button><button class="btn off" onclick="send('R4_OFF')">继电器 4 关</button>
    </div>
  </div>

  <script>
    const secret = prompt("请输入环境变量 AUTH_SECRET (如未设置留空即可):") || "";
    const protocol = location.protocol === 'https:' ? 'wss://' : 'ws://';
    const ws = new WebSocket(protocol + location.host + '/ws?auth=' + secret);

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        if(data.temp !== undefined) document.getElementById('temp').innerText = data.temp;
        if(data.humi !== undefined) document.getElementById('humi').innerText = data.humi;
        if(data.autoMode !== undefined) document.getElementById('autoSwitch').checked = data.autoMode;
      } catch(err){}
    };

    function send(cmd) { ws.send(JSON.stringify({ action: cmd })); }
    function toggleAuto(enable) { ws.send(JSON.stringify({ action: enable ? 'AUTO_ON' : 'AUTO_OFF' })); }
  </script>
</body>
</html>`;

    return new Response(html, { headers: { "content-type": "text/html;charset=UTF-8" } });
  }
};
