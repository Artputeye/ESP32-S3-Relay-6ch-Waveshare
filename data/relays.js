let gateway = `ws://${window.location.hostname}/ws`;
let websocket;

document.addEventListener("DOMContentLoaded", function () {
  loadFriendlyNames(); // ดึงชื่อ Relay และ device_name จาก setting.json มาแสดง
  initWebSocket();     // เชื่อมต่อ WebSocket ตามปกติ
});

// -------------------------------------------------------------
// โหลดชื่อ Friendly Name และ Device Name จาก setting.json
// -------------------------------------------------------------
function loadFriendlyNames() {
  fetch('/setting.json')
    .then(response => {
      if (!response.ok) {
        throw new Error("Cannot fetch setting.json");
      }
      return response.json();
    })
    .then(data => {
      // 1. อัปเดต device_name ใน <p class="namehead">
      const deviceNameElement = document.getElementById("device-name-head");
      if (deviceNameElement && data.device_name && data.device_name.trim() !== "") {
        deviceNameElement.textContent = data.device_name;
      }

      // 2. อัปเดตชื่อ Friendly Name ของ Relay ทั้ง 6 ช่อง
      for (let i = 1; i <= 6; i++) {
        const nameElement = document.getElementById(`relay-name-${i}`);
        const friendlyName = data[`friendly_name_${i}`];

        if (nameElement) {
          if (friendlyName && friendlyName.trim() !== "") {
            nameElement.textContent = friendlyName;
          } else {
            nameElement.textContent = `Relay Channel ${i}`;
          }
        }
      }
    })
    .catch(error => {
      console.warn("⚠️ Using default relay names. (Failed to load setting.json):", error);
      for (let i = 1; i <= 6; i++) {
        const nameElement = document.getElementById(`relay-name-${i}`);
        if (nameElement && !nameElement.textContent) {
          nameElement.textContent = `Relay Channel ${i}`;
        }
      }
    });
}

// -------------------------------------------------------------
// เริ่มต้นเชื่อมต่อ WebSocket
// -------------------------------------------------------------
function initWebSocket() {
  console.log("Connecting to WebSocket...");
  websocket = new WebSocket(gateway);
  
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
  websocket.onerror = onError;
}

function onOpen(event) {
  console.log("✅ WebSocket Connected!");
}

function onClose(event) {
  console.log("⚠️ WebSocket Disconnected. Trying to reconnect in 2 seconds...");
  setTimeout(initWebSocket, 2000);
}

function onError(error) {
  console.error("❌ WebSocket Error:", error);
}

// -------------------------------------------------------------
// ฟังก์ชันช่วยอัปเดต UI จากวัตถุ JSON Data
// -------------------------------------------------------------
function updateUI(data) {
  // อัปเดต device_name ถ้ามีส่งมาจาก WebSocket
  if (data.device_name && data.device_name.trim() !== "") {
    const deviceNameElement = document.getElementById("device-name-head");
    if (deviceNameElement) {
      deviceNameElement.textContent = data.device_name;
    }
  }

  // อัปเดตสถานะ Relay ทั้ง 6 ช่อง
  if (data.relays) {
    for (let i = 1; i <= 6; i++) {
      let relayKey = `Relay${i}`;
      let checkbox = document.getElementById(`relay${i}`);

      if (checkbox && data.relays[relayKey] !== undefined) {
        checkbox.checked = data.relays[relayKey] === "ON";
      }
    }
  }
}

// -------------------------------------------------------------
// รับข้อมูลจาก ESP32 ผ่าน WebSocket ( Base64 Decode )
// -------------------------------------------------------------
function onMessage(event) {
  try {
    // ถ้ารับข้อมูลมาเป็น Base64 ให้ Decode ออกเป็น JSON String ก่อน
    let decodedData = atob(event.data);
    let data = JSON.parse(decodedData);

    console.log("📥 Received WebSocket Data:", data);
    updateUI(data);

  } catch (err) {
    // กรณีที่ไม่ได้ส่งแบบ Base64 มา ให้ลอง parse แบบธรรมดา
    try {
      let data = JSON.parse(event.data);
      updateUI(data);
    } catch (e) {
      console.log("Raw Message:", event.data);
    }
  }
}

// -------------------------------------------------------------
// ส่งคำสั่ง ควบคุม เปิด/ปิด Relay ไปยัง ESP32 ผ่าน WebSocket
// -------------------------------------------------------------
function toggleRelay(relayNum, state) {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    let payload = {
      relay: relayNum,  // เลขช่อง 1 - 6
      state: state      // true = ON, false = OFF
    };

    let jsonString = JSON.stringify(payload);
    websocket.send(jsonString);
    console.log(`📤 Sent command to WebSocket: Relay ${relayNum} -> ${state ? "ON" : "OFF"}`);
  } else {
    console.error("❌ WebSocket is not connected!");
    alert("ไม่สามารถเชื่อมต่อ WebSocket ได้ กรุณาเช็คการเชื่อมต่อ");
  }
}