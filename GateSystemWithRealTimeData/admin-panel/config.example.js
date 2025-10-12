// ============================================
//  Admin Dashboard Configuration Example
// ============================================

// This file shows you how to configure the admin dashboard
// Copy the relevant sections to server.js as needed

// ====================
// SERVER CONFIGURATION
// ====================

// Change the port if 3000 is already in use
const PORT = 3000; // Try 3001, 3002, etc. if needed

// ====================
// ESP32 CONFIGURATION
// ====================

// Method 1: Direct IP Address
let ESP32_BASE_URL = 'http://192.168.1.100';

// Method 2: Using hostname (if your ESP32 has mDNS enabled)
// let ESP32_BASE_URL = 'http://esp32-gate.local';

// Method 3: Multiple ESP32 devices (for future multi-device support)
// const ESP32_DEVICES = {
//   'gate-1': 'http://192.168.1.100',
//   'gate-2': 'http://192.168.1.101',
// };

// ====================
// AUTO-REFRESH SETTINGS
// ====================

// In public/app.js, you can change the refresh interval
// Default: 5000 milliseconds (5 seconds)
// 
// refreshInterval = setInterval(async () => {
//   // ... refresh logic
// }, 5000); // Change this value

// ====================
// ALERT CONFIGURATION
// ====================

// Maximum number of alerts to keep in memory
// Default: 100
// In server.js, look for:
// if (alerts.length > 100) {
//   alerts = alerts.slice(0, 100);
// }

// ====================
// CORS CONFIGURATION
// ====================

// If you need to allow specific origins (for security)
// In server.js, replace:
// app.use(cors());
//
// With:
// app.use(cors({
//   origin: ['http://localhost:3000', 'http://192.168.1.50'],
//   credentials: true
// }));

// ====================
// NETWORK CONFIGURATION
// ====================

// To make the dashboard accessible from other devices on your network
// In server.js, change:
// app.listen(PORT, () => {
//   console.log(`Server running on http://localhost:${PORT}`);
// });
//
// To:
// app.listen(PORT, '0.0.0.0', () => {
//   console.log(`Server running on http://0.0.0.0:${PORT}`);
// });
//
// Then access from other devices using your computer's IP address
// Example: http://192.168.1.50:3000

// ====================
// TIMEOUT CONFIGURATION
// ====================

// Adjust ESP32 connection timeout (default: 3000ms)
// In server.js, look for axios requests with timeout:
// const response = await axios.get(`${ESP32_BASE_URL}/api/info`, { 
//   timeout: 3000 // Change this value
// });

// ====================
// ENVIRONMENT VARIABLES
// ====================

// For production, use environment variables instead of hardcoded values
// Create a .env file in admin-panel folder:
// 
// PORT=3000
// ESP32_IP=http://192.168.1.100
// NODE_ENV=production
//
// Then install dotenv: npm install dotenv
//
// And in server.js, add at the top:
// require('dotenv').config();
// const PORT = process.env.PORT || 3000;
// let ESP32_BASE_URL = process.env.ESP32_IP || 'http://192.168.1.100';

// ====================
// LOGGING CONFIGURATION
// ====================

// For better logging, install morgan: npm install morgan
// Then in server.js, add:
// const morgan = require('morgan');
// app.use(morgan('combined'));

// ====================
// HTTPS CONFIGURATION
// ====================

// For HTTPS (production use):
// 1. Generate SSL certificates
// 2. Install https module
// 3. In server.js:
//
// const https = require('https');
// const fs = require('fs');
// 
// const options = {
//   key: fs.readFileSync('path/to/private-key.pem'),
//   cert: fs.readFileSync('path/to/certificate.pem')
// };
// 
// https.createServer(options, app).listen(443, () => {
//   console.log('HTTPS Server running on port 443');
// });

// ====================
// DATABASE INTEGRATION
// ====================

// For persistent storage (optional):
// 
// Install MongoDB driver: npm install mongodb
// Or MySQL: npm install mysql2
// Or PostgreSQL: npm install pg
//
// Example with MongoDB:
// const { MongoClient } = require('mongodb');
// const client = new MongoClient('mongodb://localhost:27017');
// 
// async function connectDB() {
//   await client.connect();
//   console.log('Connected to MongoDB');
// }

// ====================
// AUTHENTICATION
// ====================

// For adding basic authentication (production use):
// npm install express-basic-auth
//
// const basicAuth = require('express-basic-auth');
// 
// app.use(basicAuth({
//   users: { 'admin': 'supersecretpassword' },
//   challenge: true,
//   realm: 'RFID Gate Admin'
// }));

// ====================
// USEFUL TIPS
// ====================

// 1. Find your ESP32 IP:
//    - Check Arduino Serial Monitor
//    - Look at your router's DHCP client list
//    - Use network scanning tools (Advanced IP Scanner, Fing)

// 2. Set static IP for ESP32:
//    - Configure in your router's DHCP settings
//    - Or use WiFi.config() in Arduino code

// 3. Port forwarding (access from internet):
//    - NOT RECOMMENDED without proper security
//    - If needed, forward port 3000 in your router
//    - Use strong authentication and HTTPS

// 4. Reverse proxy (advanced):
//    - Use nginx or Apache as reverse proxy
//    - Better security and SSL termination
//    - Can serve multiple services

// 5. Process manager (production):
//    - Use PM2 to keep server running
//    - npm install -g pm2
//    - pm2 start server.js --name "rfid-admin"
//    - pm2 startup (to start on boot)

// ====================
// EXAMPLE: Custom Alert Handler
// ====================

// Add webhook notifications for alerts
// In server.js, modify the /api/alerts endpoint:
//
// app.post('/api/alerts', async (req, res) => {
//   const alert = {
//     id: Date.now(),
//     timestamp: new Date().toISOString(),
//     ...req.body
//   };
//   alerts.unshift(alert);
//   
//   // Send to webhook (Slack, Discord, etc.)
//   try {
//     await axios.post('YOUR_WEBHOOK_URL', {
//       text: `Alert: ${alert.message}`
//     });
//   } catch (error) {
//     console.error('Failed to send webhook:', error);
//   }
//   
//   res.json({ success: true, alert });
// });

// ====================
// PERFORMANCE OPTIMIZATION
// ====================

// 1. Enable compression
// npm install compression
// const compression = require('compression');
// app.use(compression());

// 2. Add request rate limiting
// npm install express-rate-limit
// const rateLimit = require('express-rate-limit');
// const limiter = rateLimit({
//   windowMs: 15 * 60 * 1000, // 15 minutes
//   max: 100 // limit each IP to 100 requests per windowMs
// });
// app.use(limiter);

// ====================
// END OF CONFIGURATION
// ====================

// For more help, check:
// - README.md for full documentation
// - QUICKSTART.md for quick setup
// - PROJECT_OVERVIEW.md for architecture details
