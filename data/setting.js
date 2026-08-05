document.addEventListener("DOMContentLoaded", function () {
  // โหลดค่า Friendly Names จากเซิร์ฟเวอร์มาแสดงใน Input
  fetch("/setting.json")
    .then((response) => response.json())
    .then((data) => {
      // ดึงค่าจาก JSON มาใส่ใน input [type="text"] ตาม id
      for (const key in data) {
        const inputElement = document.getElementById(key);
        if (inputElement && inputElement.type === "text") {
          inputElement.value = data[key];
        }
      }
    })
    .catch((error) => {
      console.error("Error fetching setting data:", error);
    });
});

// ฟังก์ชันบันทึกข้อมูลทั้งหมดเมื่อกดปุ่ม SAVE
function sendConfig() {
  const data = {};

  // วนลูปเก็บค่าจาก input [type="text"] ทุกช่องที่มี id
  document.querySelectorAll('.setting-input input[type="text"]').forEach((input) => {
    const id = input.id;
    if (id) {
      data[id] = input.value.trim();
    }
  });

  console.log("Saving settings data:", data);

  // ส่ง JSON ไปยัง ESP32 / Server
  fetch("/setting.json", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  })
    .then((response) => response.text())
    .then((result) => {
      console.log("✅ Settings saved successfully:", result);
      alert("บันทึกข้อมูลเรียบร้อยแล้ว");
    })
    .catch((error) => {
      console.error("❌ Error saving settings:", error);
      alert("เกิดข้อผิดพลาดในการบันทึกข้อมูล");
    });
}