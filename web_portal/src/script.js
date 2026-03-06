// 1. Patient Name
const patientName = "Vedzze";

// 2. Battery Level
let currentBatteryLevel = 85; 

// 3. Delivery Object
const deliveryOrder = {
    orderID: 4002,
    destinationRoom: "Room 305",
    medicineList: ["Paracetamol", "Vitamin D3"],
    isUrgent: false
};

// 4. Template Literal output
console.log(`Order #${deliveryOrder.orderID} for ${deliveryOrder.destinationRoom} is now in transit. Battery: ${currentBatteryLevel}%`);