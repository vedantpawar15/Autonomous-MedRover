// // 1. Patient Name
// const patientName = "Vedzze";

// // 2. Battery Level
// let currentBatteryLevel = 85; 

// // 3. Delivery Object
// const deliveryOrder = {
//     orderID: 4002,
//     destinationRoom: "Room 305",
//     medicineList: ["Paracetamol", "Vitamin D3"],
//     isUrgent: false
// };

// // 4. Template Literal output
// console.log(`Order #${deliveryOrder.orderID} for ${deliveryOrder.destinationRoom} is now in transit. Battery: ${currentBatteryLevel}%`);



// const medicineItems = [
//     { name: "Aspirin", price: 150 },
//     { name: "Vitamin C", price: 200 }
// ];

// const calculateTotal = (items) => {
//     return items.reduce((sum, item) => sum + item.price, 0);
// };

// console.log(`Total Order Value: ₹${calculateTotal(medicineItems)}`);


// const hospitalInventory = [
//     { id: 1, name: "Paracetamol", category: "Painkiller", stock: 50 },
//     { id: 2, name: "Aspirin", category: "Painkiller", stock: 0 },
//     { id: 3, name: "Insulin", category: "Diabetes", stock: 20 },
//     { id: 4, name: "Amoxicillin", category: "Antibiotic", stock: 15 }
// ];

// // 1. FILTER: Search for Painkillers
// const painkillers = hospitalInventory.filter(med => med.category === "Painkiller");

// // 2. FIND: Find the specific medicine with ID 3
// const insulinData = hospitalInventory.find(med => med.id === 3);

// // 3. MAP: Create a simple list of just the medicine names
// const medicineNames = hospitalInventory.map(med => med.name);

// console.log("Painkillers found:", painkillers);
// console.log("Medicine list:", medicineNames);

// // --- 1. Object Destructuring ---
// const currentMedicine = {
//     medName: "Paracetamol",
//     price: 150,
//     stock: 45,
//     supplier: "Apollo Pharmacy"
// };

// // Instead of medicine.medName, we "extract" them:
// const { medName, stock } = currentMedicine;

// console.log("--- Destructuring Test ---");
// console.log(`Medicine: ${medName}, In Stock: ${stock}`); 


// // --- 2. Spread Operator (The 'Add to Cart' Logic) ---
// const basicInfo = { id: 101, name: "Aspirin" };
// const pricingInfo = { price: 200, tax: "5%" };

// // Merging objects and adding new properties
// const fullProductProfile = { 
//     ...basicInfo, 
//     ...pricingInfo, 
//     quantity: 1,
//     status: "Added to Cart" 
// };

// console.log("\n--- Spread Operator Test ---");
// console.log("Full Product Object:", fullProductProfile);


// // --- 3. Optional Chaining (The Crash Protector) ---
// const robotDiagnostic = {
//     model: "MedBot-v2",
//     sensors: {
//         lidar: "Active"
//         // Notice 'camera' is missing here
//     }
// };

// console.log("\n--- Optional Chaining Test ---");
// // This would normally crash the app if we didn't use '?.'
// console.log("Camera Status:", robotDiagnostic?.sensors?.camera || "Sensor Not Found");


// // --- 4. Object.keys() and Object.values() ---
// console.log("\n--- Object Methods Test ---");
// console.log("Property Names:", Object.keys(currentMedicine));
// console.log("Property Values:", Object.values(currentMedicine));


// // --- 1.5 Conditional Logic Test ---
// const medInStock = true;
// const medPrice = 500;

// // Ternary Operator: deciding what text to show on a button
// const buttonText = medInStock ? "Add to Cart" : "Out of Stock";
// console.log("Button Label:", buttonText);

// // Short-circuit (&&): Only show 'Expensive' if price > 400
// medPrice > 400 && console.log("Alert: This is a premium medicine.");


// // --- 1.6 Async/Await Simulation ---
// // We simulate a database delay using a 'Promise'
// const mockSupabaseFetch = () => {
//     return new Promise((resolve, reject) => {
//         const success = true; // Change to false to test the 'catch' block
//         setTimeout(() => {
//             if (success) {
//                 resolve([{ id: 1, name: "Aspirin" }, { id: 2, name: "Insulin" }]);
//             } else {
//                 reject("Network Error: Could not connect to Supabase");
//             }
//         }, 1500); // 1.5 second delay
//     });
// };

// const getMedicines = async () => {
//     console.log("\nFetching medicine data from database...");
    
//     try {
//         const data = await mockSupabaseFetch(); // Wait for the "database"
//         console.log("Success! Data received:", data);
//     } catch (error) {
//         console.error("Critical Error:", error);
//     } finally {
//         console.log("Fetch attempt finished.");
//     }
// };

// getMedicines();


// 1. Data (Simulation)
const robotData = { model: "MedRover-V1", battery: 92, status: "Active" };
const medicineInventory = [
    { id: 101, name: "Aspirin", price: 150, inStock: true },
    { id: 102, name: "Insulin", price: 500, inStock: false },
    { id: 103, name: "Vitamin C", price: 120, inStock: true }
];

// 2. The Implementation Function
const renderPractice = async () => {
    // Select our new HTML elements
    const robotDiv = document.querySelector('#robot-display');
    const medList = document.querySelector('#medicine-display');

    // Destructuring (1.4)
    const { model, battery, status } = robotData;

    // Async delay to simulate loading (1.6)
    robotDiv.innerHTML = "<p>Loading Robot Systems...</p>";
    await new Promise(res => setTimeout(res, 1000));

    // Conditional Logic & Template Literals (1.1 & 1.5)
    robotDiv.innerHTML = `
        <p><strong>Model:</strong> ${model}</p>
        <p><strong>Battery:</strong> ${battery}% ${battery < 20 ? "⚠️" : "✅"}</p>
        <p><strong>Status:</strong> ${status}</p>
    `;

    // Array Map to generate list (1.3)
    const medItemsHTML = medicineInventory.map(med => {
        return `
            <li style="color: ${med.inStock ? 'black' : 'red'}">
                ${med.name} - ₹${med.price} 
                ${med.inStock ? "(Available)" : "(Out of Stock)"}
            </li>
        `;
    }).join('');

    medList.innerHTML = medItemsHTML;
};

// Start the process
renderPractice();