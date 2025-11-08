const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const axios = require('axios');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = 3000;

// Database file paths
const DB_DIR = path.join(__dirname, 'database');
const USERS_DB_FILE = path.join(DB_DIR, 'users.json');
const SETTINGS_DB_FILE = path.join(DB_DIR, 'settings.json');
const DEVICES_DB_FILE = path.join(DB_DIR, 'devices.json');

// Ensure database directory exists
if (!fs.existsSync(DB_DIR)) {
  fs.mkdirSync(DB_DIR, { recursive: true });
}

// Initialize database files if they don't exist
if (!fs.existsSync(USERS_DB_FILE)) {
  fs.writeFileSync(USERS_DB_FILE, JSON.stringify({ users: [] }, null, 2));
}
if (!fs.existsSync(SETTINGS_DB_FILE)) {
  fs.writeFileSync(SETTINGS_DB_FILE, JSON.stringify({ 
    costPerExit: 3000,
    defaultCredit: 100000,
    adminMode: false
  }, null, 2));
}
if (!fs.existsSync(DEVICES_DB_FILE)) {
  fs.writeFileSync(DEVICES_DB_FILE, JSON.stringify({ devices: [], activeDeviceId: null }, null, 2));
}

// Load database
function loadDatabase(filePath) {
  try {
    const data = fs.readFileSync(filePath, 'utf8');
    return JSON.parse(data);
  } catch (error) {
    console.error(`Error loading ${filePath}:`, error.message);
    return null;
  }
}

// Save database
function saveDatabase(filePath, data) {
  try {
    fs.writeFileSync(filePath, JSON.stringify(data, null, 2));
    return true;
  } catch (error) {
    console.error(`Error saving ${filePath}:`, error.message);
    return false;
  }
}

// Middleware
app.use(cors());
app.use(bodyParser.json());
app.use(express.static('public'));

// Multiple ESP32 devices storage
let devices = new Map(); // deviceId -> device info
let activeDeviceId = null; // Currently selected device

// Load devices from database on startup
const devicesData = loadDatabase(DEVICES_DB_FILE);
if (devicesData && devicesData.devices) {
  devicesData.devices.forEach(device => {
    devices.set(device.id, device);
  });
  activeDeviceId = devicesData.activeDeviceId;
  console.log(`Loaded ${devices.size} devices from database`);
}

// Store alerts
let alerts = [];

// Store connection logs
let connectionLogs = [];

// Save devices to database
function saveDevices() {
  const devicesArray = Array.from(devices.values());
  saveDatabase(DEVICES_DB_FILE, {
    devices: devicesArray,
    activeDeviceId
  });
}

// Helper function to add connection log
function addConnectionLog(deviceId, action, status, message) {
  const log = {
    id: Date.now(),
    timestamp: new Date().toISOString(),
    deviceId,
    action,
    status,
    message
  };
  connectionLogs.unshift(log);
  if (connectionLogs.length > 500) {
    connectionLogs = connectionLogs.slice(0, 500);
  }
  return log;
}

// Helper function to get active device
function getActiveDevice() {
  if (!activeDeviceId) return null;
  return devices.get(activeDeviceId);
}

// Helper function to sync time with ESP32
async function syncDeviceTime(deviceIp) {
  try {
    const timestamp = Math.floor(Date.now() / 1000); // Unix timestamp in seconds
    await axios.post(`${deviceIp}/api/time/sync`, { timestamp }, { timeout: 3000 });
    console.log(`Time synced with device ${deviceIp}: ${new Date().toISOString()}`);
    return true;
  } catch (error) {
    console.error(`Failed to sync time with ${deviceIp}:`, error.message);
    return false;
  }
}

// Helper function to auto-sync database to all connected devices
async function autoSyncToAllDevices(action, message) {
  const connectedDevices = Array.from(devices.values()).filter(device => device.connected);
  
  if (connectedDevices.length === 0) {
    console.log('No connected devices to sync');
    return;
  }
  
  console.log(`Auto-syncing to ${connectedDevices.length} connected devices - ${action}: ${message}`);
  
  const usersData = loadDatabase(USERS_DB_FILE);
  const settingsData = loadDatabase(SETTINGS_DB_FILE);
  
  for (const device of connectedDevices) {
    try {
      await axios.post(`${device.ip}/api/database/sync`, {
        users: usersData.users,
        settings: settingsData
      }, { timeout: 5000 });
      
      addConnectionLog(device.id, 'AUTO_SYNC', 'SUCCESS', `${action}: ${message}`);
      console.log(`Auto-synced to device ${device.name} (${device.ip})`);
    } catch (error) {
      addConnectionLog(device.id, 'AUTO_SYNC', 'FAILED', `${action} sync failed: ${error.message}`);
      console.error(`Failed to auto-sync to device ${device.name}:`, error.message);
    }
  }
}

// ==================== Device Management Routes ====================

// Add or update a device
app.post('/api/devices/add', async (req, res) => {
  const { name, ip, location, description } = req.body;
  
  if (!name || !ip) {
    return res.status(400).json({ error: 'Name and IP are required' });
  }
  
  const baseUrl = ip.startsWith('http') ? ip : `http://${ip}`;
  
  // Check if device with this IP already exists
  const existingDevice = Array.from(devices.values()).find(device => device.ip === baseUrl);
  if (existingDevice) {
    return res.status(400).json({ 
      error: `Device with IP ${baseUrl} already exists (ID: ${existingDevice.id})` 
    });
  }
  
  const deviceId = `device_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  
  const device = {
    id: deviceId,
    name,
    ip: baseUrl,
    location: location || 'Not specified',
    description: description || '',
    addedAt: new Date().toISOString(),
    lastConnected: null,
    connected: false,
    status: 'disconnected',
    info: null,
    error: null
  };
  
  // Try to connect and get info
  try {
    const response = await axios.get(`${baseUrl}/api/info`, { timeout: 3000 });
    device.connected = true;
    device.status = 'connected';
    device.lastConnected = new Date().toISOString();
    device.info = response.data;
    
    // Sync time with device
    await syncDeviceTime(baseUrl);
    
    addConnectionLog(deviceId, 'ADD', 'SUCCESS', `Device "${name}" added and connected`);
  } catch (error) {
    device.error = error.message;
    addConnectionLog(deviceId, 'ADD', 'WARNING', `Device "${name}" added but not connected: ${error.message}`);
  }
  
  devices.set(deviceId, device);
  
  // Set as active if it's the first device
  if (devices.size === 1) {
    activeDeviceId = deviceId;
  }
  
  // Save devices to database
  saveDevices();
  
  // Auto-sync central database to new device if connected
  if (device.connected) {
    try {
      const usersData = loadDatabase(USERS_DB_FILE);
      const settingsData = loadDatabase(SETTINGS_DB_FILE);
      await axios.post(`${device.ip}/api/database/sync`, {
        users: usersData.users,
        settings: settingsData
      }, { timeout: 5000 });
      addConnectionLog(deviceId, 'DATABASE_SYNC', 'SUCCESS', `Database synced to new device "${name}"`);
    } catch (error) {
      console.log(`Auto-sync failed for new device: ${error.message}`);
    }
  }
  
  res.json({ success: true, device });
});

// Get all devices
app.get('/api/devices', (req, res) => {
  const deviceList = Array.from(devices.values()).map(device => ({
    ...device,
    isActive: device.id === activeDeviceId
  }));
  res.json({ devices: deviceList, activeDeviceId });
});

// Set active device
app.post('/api/devices/set-active', (req, res) => {
  const { deviceId } = req.body;
  
  if (!deviceId || !devices.has(deviceId)) {
    return res.status(400).json({ error: 'Invalid device ID' });
  }
  
  activeDeviceId = deviceId;
  const device = devices.get(deviceId);
  addConnectionLog(deviceId, 'SWITCH', 'INFO', `Switched to device "${device.name}"`);
  
  // Save to database
  saveDevices();
  
  res.json({ success: true, deviceId, device });
});

// Delete device
app.delete('/api/devices/:deviceId', (req, res) => {
  const { deviceId } = req.params;
  
  if (!devices.has(deviceId)) {
    return res.status(404).json({ error: 'Device not found' });
  }
  
  const device = devices.get(deviceId);
  devices.delete(deviceId);
  
  // If deleted device was active, set another device as active
  if (activeDeviceId === deviceId) {
    const deviceArray = Array.from(devices.keys());
    activeDeviceId = deviceArray.length > 0 ? deviceArray[0] : null;
  }
  
  addConnectionLog(deviceId, 'DELETE', 'INFO', `Device "${device.name}" removed`);
  
  // Save to database
  saveDevices();
  
  res.json({ success: true, newActiveDeviceId: activeDeviceId });
});

// Retry connection for a specific device
app.post('/api/devices/:deviceId/retry', async (req, res) => {
  const { deviceId } = req.params;
  
  if (!devices.has(deviceId)) {
    return res.status(404).json({ error: 'Device not found' });
  }
  
  const device = devices.get(deviceId);
  
  try {
    const response = await axios.get(`${device.ip}/api/info`, { timeout: 3000 });
    device.connected = true;
    device.status = 'connected';
    device.lastConnected = new Date().toISOString();
    device.info = response.data;
    device.error = null;
    devices.set(deviceId, device);
    
    // Sync time with device
    await syncDeviceTime(device.ip);
    
    // Auto-sync central database to reconnected device
    try {
      const usersData = loadDatabase(USERS_DB_FILE);
      const settingsData = loadDatabase(SETTINGS_DB_FILE);
      await axios.post(`${device.ip}/api/database/sync`, {
        users: usersData.users,
        settings: settingsData
      }, { timeout: 5000 });
      addConnectionLog(deviceId, 'DATABASE_SYNC', 'SUCCESS', `Database auto-synced on reconnection to "${device.name}"`);
    } catch (syncError) {
      console.log(`Auto-sync failed for reconnected device: ${syncError.message}`);
      addConnectionLog(deviceId, 'DATABASE_SYNC', 'FAILED', `Auto-sync failed on reconnection: ${syncError.message}`);
    }
    
    // Save to database
    saveDevices();
    
    addConnectionLog(deviceId, 'RETRY', 'SUCCESS', `Reconnected to "${device.name}"`);
    
    res.json({ success: true, device });
  } catch (error) {
    device.connected = false;
    device.status = 'error';
    device.error = error.message;
    devices.set(deviceId, device);
    
    // Save to database
    saveDevices();
    
    addConnectionLog(deviceId, 'RETRY', 'FAILED', `Failed to reconnect to "${device.name}": ${error.message}`);
    
    res.status(500).json({ success: false, error: error.message, device });
  }
});

// Sync time with a specific device
app.post('/api/devices/:deviceId/sync-time', async (req, res) => {
  const { deviceId } = req.params;
  
  if (!devices.has(deviceId)) {
    return res.status(404).json({ error: 'Device not found' });
  }
  
  const device = devices.get(deviceId);
  
  if (!device.connected) {
    return res.status(400).json({ error: 'Device is not connected' });
  }
  
  const synced = await syncDeviceTime(device.ip);
  
  if (synced) {
    addConnectionLog(deviceId, 'TIME_SYNC', 'SUCCESS', `Time synced with "${device.name}"`);
    res.json({ success: true, message: 'Time synchronized' });
  } else {
    addConnectionLog(deviceId, 'TIME_SYNC', 'FAILED', `Failed to sync time with "${device.name}"`);
    res.status(500).json({ success: false, error: 'Time sync failed' });
  }
});

// Check connection status for active device
app.get('/api/connection/status', async (req, res) => {
  if (!activeDeviceId || !devices.has(activeDeviceId)) {
    return res.json({ 
      connected: false, 
      error: 'No active device',
      activeDeviceId: null 
    });
  }
  
  const device = devices.get(activeDeviceId);
  
  try {
    const response = await axios.get(`${device.ip}/api/info`, { timeout: 3000 });
    device.connected = true;
    device.status = 'connected';
    device.lastConnected = new Date().toISOString();
    device.info = response.data;
    device.error = null;
    devices.set(activeDeviceId, device);
    
    res.json({ 
      connected: true, 
      device,
      activeDeviceId 
    });
  } catch (error) {
    device.connected = false;
    device.status = 'error';
    device.error = error.message;
    devices.set(activeDeviceId, device);
    
    res.json({ 
      connected: false, 
      error: error.message,
      device,
      activeDeviceId 
    });
  }
});

// Get connection logs
app.get('/api/logs/connections', (req, res) => {
  res.json({ logs: connectionLogs });
});

// Clear connection logs
app.delete('/api/logs/connections', (req, res) => {
  connectionLogs = [];
  res.json({ success: true });
});

// Receive alerts from ESP32 or other sources
app.post('/api/alerts', (req, res) => {
  const alert = {
    id: Date.now(),
    timestamp: new Date().toISOString(),
    ...req.body
  };
  alerts.unshift(alert);
  // Keep only last 100 alerts
  if (alerts.length > 100) {
    alerts = alerts.slice(0, 100);
  }
  res.json({ success: true, alert });
});

// Get all alerts
app.get('/api/alerts', (req, res) => {
  res.json({ alerts });
});

// Clear alerts
app.delete('/api/alerts', (req, res) => {
  alerts = [];
  res.json({ success: true });
});

// ==================== Proxy Routes to ESP32 ====================

// Helper to get active device URL
function getActiveDeviceUrl() {
  const device = getActiveDevice();
  if (!device) {
    throw new Error('No active device selected');
  }
  if (!device.connected) {
    throw new Error('Active device is not connected');
  }
  return device.ip;
}

// Get ESP32 info
app.get('/api/esp32/info', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.get(`${url}/api/info`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Get ESP32 state
app.get('/api/esp32/state', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.get(`${url}/api/state`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Get events
app.get('/api/esp32/events', async (req, res) => {
  try {
    if (!activeDeviceId) {
      return res.status(400).json({ error: 'No active device selected', events: [] });
    }
    
    const device = getActiveDevice();
    if (!device) {
      return res.status(404).json({ error: 'Active device not found', events: [] });
    }
    
    if (!device.connected) {
      return res.status(503).json({ error: 'Active device is not connected', events: [] });
    }

    const url = getActiveDeviceUrl();
    const response = await axios.get(`${url}/api/events`, { timeout: 3000 });
    res.json(response.data);
  } catch (error) {
    console.error('Error fetching events:', error.message);
    res.status(500).json({ error: error.message, events: [] });
  }
});

// Get last input scan (enhanced for input mode)
app.get('/api/esp32/input/last', async (req, res) => {
  try {
    if (!activeDeviceId) {
      return res.status(400).json({ error: 'No active device selected', hasInput: false });
    }
    
    const device = getActiveDevice();
    if (!device) {
      return res.status(404).json({ error: 'Active device not found', hasInput: false });
    }
    
    if (!device.connected) {
      return res.status(503).json({ error: 'Active device is not connected', hasInput: false });
    }

    const url = getActiveDeviceUrl();
    const clearParam = req.query.clear === 'true' ? '?clear=true' : '';
    const response = await axios.get(`${url}/api/input/last${clearParam}`, { timeout: 3000 });
    res.json(response.data);
  } catch (error) {
    console.error('Error fetching last input:', error.message);
    res.status(500).json({ error: error.message, hasInput: false });
  }
});

// Toggle input mode on ESP32
app.post('/api/esp32/input/mode', async (req, res) => {
  try {
    if (!activeDeviceId) {
      return res.status(400).json({ error: 'No active device selected' });
    }
    
    const device = getActiveDevice();
    if (!device) {
      return res.status(404).json({ error: 'Active device not found' });
    }
    
    if (!device.connected) {
      return res.status(503).json({ error: 'Active device is not connected' });
    }

    const url = getActiveDeviceUrl();
    const { mode } = req.body;
    
    const response = await axios.post(`${url}/api/input/mode`, { mode }, { timeout: 3000 });
    res.json(response.data);
  } catch (error) {
    console.error('Error toggling input mode:', error.message);
    res.status(500).json({ error: error.message });
  }
});

// Store for pending new UIDs (for input mode)
let pendingNewUIDs = [];

// Receive new UID from ESP32 (input mode)
app.post('/api/input/new-uid', (req, res) => {
  try {
    const { uid, isNew, device_ip } = req.body;
    
    if (!uid) {
      return res.status(400).json({ error: 'UID is required' });
    }
    
    console.log(`New UID received from device ${device_ip}: ${uid} (isNew: ${isNew})`);
    
    // Store the new UID with timestamp
    const newUID = {
      uid: uid,
      isNew: isNew,
      timestamp: new Date().toISOString(),
      device_ip: device_ip,
      id: Date.now() // Simple ID for tracking
    };
    
    pendingNewUIDs.push(newUID);
    
    // Keep only the last 10 UIDs to prevent memory issues
    if (pendingNewUIDs.length > 10) {
      pendingNewUIDs = pendingNewUIDs.slice(-10);
    }
    
    res.json({ success: true, message: 'UID received' });
  } catch (error) {
    console.error('Error processing new UID:', error);
    res.status(500).json({ error: error.message });
  }
});

// Get pending new UIDs (for admin panel polling)
app.get('/api/input/pending-uids', (req, res) => {
  try {
    res.json({ 
      success: true, 
      uids: pendingNewUIDs,
      count: pendingNewUIDs.length 
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Clear pending UIDs (after processing)
app.delete('/api/input/pending-uids/:id', (req, res) => {
  try {
    const { id } = req.params;
    const initialLength = pendingNewUIDs.length;
    pendingNewUIDs = pendingNewUIDs.filter(uid => uid.id != id);
    
    if (pendingNewUIDs.length === initialLength) {
      return res.status(404).json({ error: 'UID not found' });
    }
    
    res.json({ success: true, message: 'UID removed from pending list' });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Export users
app.get('/api/esp32/users/export', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.get(`${url}/api/users/export`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Open gate
app.post('/api/esp32/open', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/open`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Control LED
app.post('/api/esp32/led', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/led`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Toggle admin mode
app.post('/api/esp32/admin', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/admin`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Add user
app.post('/api/esp32/users/add', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/users/add`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Update user
app.post('/api/esp32/users/update', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/users/update`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Delete user
app.post('/api/esp32/users/delete', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/users/delete`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Clear users
app.post('/api/esp32/users/clear', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/users/clear`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Add credit
app.post('/api/esp32/credit/add', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/credit/add`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Set state
app.post('/api/esp32/state/set', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.post(`${url}/api/state/set`, req.body);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Self test
app.get('/api/esp32/selftest', async (req, res) => {
  try {
    const url = getActiveDeviceUrl();
    const response = await axios.get(`${url}/api/selftest`);
    res.json(response.data);
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// ==================== Central Database API ====================

// Get all users from central database
app.get('/api/database/users', (req, res) => {
  try {
    const data = loadDatabase(USERS_DB_FILE);
    if (data) {
      res.json({ success: true, users: data.users || [] });
    } else {
      res.json({ success: true, users: [] });
    }
  } catch (error) {
    console.error('Error loading database users:', error.message);
    res.status(500).json({ success: false, error: error.message, users: [] });
  }
});

// Add user to central database
app.post('/api/database/users/add', (req, res) => {
  const { uid, name, credit, type } = req.body;
  
  if (!uid || !name) {
    return res.status(400).json({ error: 'UID and name are required' });
  }
  
  const data = loadDatabase(USERS_DB_FILE);
  
  // Check if user already exists
  const exists = data.users.find(u => u.uid === uid);
  if (exists) {
    return res.status(400).json({ error: 'User with this UID already exists' });
  }
  
  const newUser = {
    uid,
    name,
    credit: credit || 100000,
    type: type || 'DYN',
    in: false,
    createdAt: new Date().toISOString()
  };
  
  data.users.push(newUser);
  
  if (saveDatabase(USERS_DB_FILE, data)) {
    // Auto-sync to all connected devices
    autoSyncToAllDevices('USER_ADDED', `New user "${name}" added`);
    
    res.json({ success: true, user: newUser });
  } else {
    res.status(500).json({ error: 'Failed to save database' });
  }
});

// Update user in central database
app.post('/api/database/users/update', (req, res) => {
  const { uid, name, credit, in: userIn } = req.body;
  
  if (!uid) {
    return res.status(400).json({ error: 'UID is required' });
  }
  
  const data = loadDatabase(USERS_DB_FILE);
  const userIndex = data.users.findIndex(u => u.uid === uid);
  
  if (userIndex === -1) {
    return res.status(404).json({ error: 'User not found' });
  }
  
  if (name !== undefined) data.users[userIndex].name = name;
  if (credit !== undefined) data.users[userIndex].credit = credit;
  if (userIn !== undefined) data.users[userIndex].in = userIn;
  data.users[userIndex].updatedAt = new Date().toISOString();
  
  if (saveDatabase(USERS_DB_FILE, data)) {
    // Auto-sync to all connected devices
    autoSyncToAllDevices('USER_UPDATED', `User "${data.users[userIndex].name}" updated`);
    
    res.json({ success: true, user: data.users[userIndex] });
  } else {
    res.status(500).json({ error: 'Failed to save database' });
  }
});

// Delete user from central database
app.delete('/api/database/users/:uid', (req, res) => {
  const { uid } = req.params;
  
  const data = loadDatabase(USERS_DB_FILE);
  const initialLength = data.users.length;
  data.users = data.users.filter(u => u.uid !== uid);
  
  if (data.users.length === initialLength) {
    return res.status(404).json({ error: 'User not found' });
  }
  
  if (saveDatabase(USERS_DB_FILE, data)) {
    // Auto-sync to all connected devices
    autoSyncToAllDevices('USER_DELETED', `User with UID "${uid}" deleted`);
    
    res.json({ success: true, message: 'User deleted' });
  } else {
    res.status(500).json({ error: 'Failed to save database' });
  }
});

// Add credit to user in central database
app.post('/api/database/users/:uid/credit', (req, res) => {
  const { uid } = req.params;
  const { amount } = req.body;
  
  if (!amount || isNaN(amount)) {
    return res.status(400).json({ error: 'Valid amount is required' });
  }
  
  const data = loadDatabase(USERS_DB_FILE);
  const user = data.users.find(u => u.uid === uid);
  
  if (!user) {
    return res.status(404).json({ error: 'User not found' });
  }
  
  user.credit = (user.credit || 0) + parseInt(amount);
  user.updatedAt = new Date().toISOString();
  
  if (saveDatabase(USERS_DB_FILE, data)) {
    // Auto-sync to all connected devices
    autoSyncToAllDevices('CREDIT_UPDATED', `Credit updated for "${user.name}" (${amount > 0 ? '+' : ''}${amount})`);
    
    res.json({ success: true, user });
  } else {
    res.status(500).json({ error: 'Failed to save database' });
  }
});

// Get settings from central database
app.get('/api/database/settings', (req, res) => {
  const data = loadDatabase(SETTINGS_DB_FILE);
  res.json(data || { costPerExit: 3000, defaultCredit: 100000, adminMode: false });
});

// Update settings in central database
app.post('/api/database/settings', (req, res) => {
  const { costPerExit, defaultCredit, adminMode } = req.body;
  
  const data = loadDatabase(SETTINGS_DB_FILE);
  
  if (costPerExit !== undefined) data.costPerExit = costPerExit;
  if (defaultCredit !== undefined) data.defaultCredit = defaultCredit;
  if (adminMode !== undefined) data.adminMode = adminMode;
  
  if (saveDatabase(SETTINGS_DB_FILE, data)) {
    res.json({ success: true, settings: data });
  } else {
    res.status(500).json({ error: 'Failed to save database' });
  }
});

// Sync database to a specific ESP32 device
app.post('/api/database/sync/:deviceId', async (req, res) => {
  const { deviceId } = req.params;
  
  if (!devices.has(deviceId)) {
    return res.status(404).json({ error: 'Device not found' });
  }
  
  const device = devices.get(deviceId);
  
  if (!device.connected) {
    return res.status(400).json({ error: 'Device is not connected' });
  }
  
  try {
    const usersData = loadDatabase(USERS_DB_FILE);
    const settingsData = loadDatabase(SETTINGS_DB_FILE);
    
    // Send database to ESP32
    const response = await axios.post(`${device.ip}/api/database/sync`, {
      users: usersData.users,
      settings: settingsData
    }, { timeout: 5000 });
    
    addConnectionLog(deviceId, 'DATABASE_SYNC', 'SUCCESS', `Database synced to "${device.name}"`);
    res.json({ success: true, message: 'Database synced to device', syncedUsers: usersData.users.length });
  } catch (error) {
    addConnectionLog(deviceId, 'DATABASE_SYNC', 'FAILED', `Failed to sync database to "${device.name}": ${error.message}`);
    res.status(500).json({ error: error.message });
  }
});

// Sync database to all connected devices
app.post('/api/database/sync-all', async (req, res) => {
  const usersData = loadDatabase(USERS_DB_FILE);
  const settingsData = loadDatabase(SETTINGS_DB_FILE);
  
  const results = [];
  
  for (const [deviceId, device] of devices) {
    if (!device.connected) {
      results.push({ deviceId, name: device.name, success: false, error: 'Device not connected' });
      continue;
    }
    
    try {
      await axios.post(`${device.ip}/api/database/sync`, {
        users: usersData.users,
        settings: settingsData
      }, { timeout: 5000 });
      
      results.push({ deviceId, name: device.name, success: true });
      addConnectionLog(deviceId, 'DATABASE_SYNC', 'SUCCESS', `Database synced to "${device.name}"`);
    } catch (error) {
      results.push({ deviceId, name: device.name, success: false, error: error.message });
      addConnectionLog(deviceId, 'DATABASE_SYNC', 'FAILED', `Failed to sync database to "${device.name}": ${error.message}`);
    }
  }
  
  const successCount = results.filter(r => r.success).length;
  res.json({ 
    success: true, 
    message: `Synced to ${successCount} out of ${results.length} devices`,
    results 
  });
});

// Serve main dashboard
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Start server
app.listen(PORT, () => {
  console.log(` Admin Panel Server running on http://localhost:${PORT}`);
  console.log(` Multi-Device Support Enabled`);
  console.log(`\n  Add ESP32 devices through the Devices page in the dashboard.`);
  console.log(`   Supports multiple devices with automatic device switching.`);
});
