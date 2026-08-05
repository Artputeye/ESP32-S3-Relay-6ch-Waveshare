let gateway = `ws://${window.location.hostname}/ws`;
let websocket;

document.addEventListener("DOMContentLoaded", function () {
    loadDeviceName(); // โหลดชื่อจาก setting.json ทันทีเมื่อเปิดหน้า
    initWebSocket();  // เชื่อมต่อ WebSocket เพื่ออัปเดตข้อมูล
});

// -------------------------------------------------------------
// 1. ดึงข้อมูล device_name จาก setting.json โดยตรง
// -------------------------------------------------------------
function loadDeviceName() {
    fetch('/setting.json')
        .then(response => {
            if (!response.ok) {
                throw new Error("Cannot fetch setting.json");
            }
            return response.json();
        })
        .then(data => {
            updateDeviceNameUI(data.device_name);
        })
        .catch(error => {
            console.warn("⚠️ Failed to load device_name from setting.json:", error);
        });
}

// -------------------------------------------------------------
// 2. เชื่อมต่อ WebSocket
// -------------------------------------------------------------
function initWebSocket() {
    console.log("Connecting to WebSocket...");
    websocket = new WebSocket(gateway);

    websocket.onopen = function (event) {
        console.log("✅ WebSocket Connected!");
    };

    websocket.onclose = function (event) {
        console.log("⚠️ WebSocket Disconnected. Retrying in 2 seconds...");
        setTimeout(initWebSocket, 2000);
    };

    websocket.onmessage = function (event) {
        try {
            // รองรับทั้ง Base64 และ Plain JSON
            let data;
            try {
                let decodedData = atob(event.data);
                data = JSON.parse(decodedData);
            } catch (e) {
                data = JSON.parse(event.data);
            }

            if (data && data.device_name) {
                updateDeviceNameUI(data.device_name);
            }
        } catch (err) {
            // ปล่อยผ่านข้อความที่ไม่ใช่ JSON
        }
    };
}

// -------------------------------------------------------------
// 3. ฟังก์ชันอัปเดตข้อความบน HTML
// -------------------------------------------------------------
function updateDeviceNameUI(name) {
    if (name && name.trim() !== "") {
        const nameElement = document.getElementById("device-name-head");
        if (nameElement) {
            nameElement.textContent = name;
        }
    }
}