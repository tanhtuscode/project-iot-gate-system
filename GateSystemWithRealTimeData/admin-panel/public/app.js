// Configuration
let devices = [];
let activeDeviceId = null;
let esp32Connected = false;
let refreshInterval = null;

// Initialize app
document.addEventListener('DOMContentLoaded', async () => {
    await loadDevices();
    await checkConnection();
    updateInputModeUI(); // Initialize input mode UI
    startAutoRefresh();
    loadAlerts();
    loadConnectionLogs();
    switchTab('overview');
});

// ==================== Device Management ====================

async function loadDevices() {
    try {
        const response = await fetch('/api/devices');
        const data = await response.json();
        devices = data.devices || [];
        activeDeviceId = data.activeDeviceId;
        
        updateDevicesList();
        updateActiveDeviceDisplay();
        updateConnectedDevicesCount();
    } catch (error) {
        console.error('Failed to load devices:', error);
    }
}

function updateConnectedDevicesCount() {
    const connectedCount = devices.filter(device => device.connected).length;
    const connectedDevicesEl = document.getElementById('connectedDevices');
    if (connectedDevicesEl) {
        connectedDevicesEl.textContent = connectedCount;
    }
}

function updateDevicesList() {
    const container = document.getElementById('devicesList');
    
    if (devices.length === 0) {
        container.innerHTML = `
            <div class="col-span-full text-center py-12 text-gray-500">
                <i class="fas fa-server text-6xl mb-4 opacity-50"></i>
                <p class="text-lg">No devices added yet</p>
                <p class="text-sm mt-2">Click "Add Device" to connect your first ESP32</p>
            </div>
        `;
        return;
    }
    
    container.innerHTML = devices.map(device => `
        <div class="bg-white rounded-lg shadow-md p-6 ${device.isActive ? 'ring-2 ring-blue-500' : ''} transition-all">
            <div class="flex justify-between items-start mb-4">
                <div class="flex items-start space-x-3">
                    <div class="flex-shrink-0">
                        ${device.isActive ? 
                            '<div class="w-12 h-12 bg-blue-100 rounded-lg flex items-center justify-center"><i class="fas fa-server text-blue-600 text-xl"></i></div>' :
                            '<div class="w-12 h-12 bg-gray-100 rounded-lg flex items-center justify-center"><i class="fas fa-server text-gray-600 text-xl"></i></div>'
                        }
                    </div>
                    <div class="flex-1">
                        <h4 class="font-bold text-lg text-gray-800">${device.name}</h4>
                        <p class="text-sm text-gray-500">${device.location}</p>
                    </div>
                </div>
                <div class="flex items-center space-x-2">
                    ${device.connected ? 
                        '<span class="px-2 py-1 bg-green-100 text-green-800 text-xs font-semibold rounded-full"><i class="fas fa-check-circle mr-1"></i>Connected</span>' :
                        '<span class="px-2 py-1 bg-red-100 text-red-800 text-xs font-semibold rounded-full"><i class="fas fa-times-circle mr-1"></i>Offline</span>'
                    }
                </div>
            </div>
            
            <div class="space-y-2 mb-4 text-sm">
                <div class="flex justify-between">
                    <span class="text-gray-600">IP Address:</span>
                    <span class="font-mono font-semibold">${device.ip.replace('http://', '')}</span>
                </div>
                ${device.info ? `
                    <div class="flex justify-between">
                        <span class="text-gray-600">Firmware:</span>
                        <span class="font-semibold">${device.info.fwVersion || 'N/A'}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">Uptime:</span>
                        <span class="font-semibold">${formatUptime(device.info.uptime)}</span>
                    </div>
                ` : ''}
                ${device.lastConnected ? `
                    <div class="flex justify-between">
                        <span class="text-gray-600">Last Connected:</span>
                        <span class="font-semibold">${new Date(device.lastConnected).toLocaleString()}</span>
                    </div>
                ` : ''}
                ${device.description ? `
                    <div class="col-span-2 mt-2">
                        <p class="text-gray-600 text-xs">${device.description}</p>
                    </div>
                ` : ''}
            </div>
            
            ${device.error ? `
                <div class="mb-4 p-3 bg-red-50 border border-red-200 rounded text-xs text-red-700">
                    <i class="fas fa-exclamation-triangle mr-1"></i>${device.error}
                </div>
            ` : ''}
            
            <div class="flex flex-wrap gap-2">
                ${!device.isActive ? 
                    `<button onclick="setActiveDevice('${device.id}')" class="flex-1 bg-blue-600 hover:bg-blue-700 text-white py-2 px-4 rounded transition text-sm">
                        <i class="fas fa-check mr-2"></i>Set Active
                    </button>` :
                    `<button disabled class="flex-1 bg-blue-500 text-white py-2 px-4 rounded text-sm opacity-75 cursor-not-allowed">
                        <i class="fas fa-star mr-2"></i>Active Device
                    </button>`
                }
                <button onclick="retryDeviceConnection('${device.id}')" class="bg-green-600 hover:bg-green-700 text-white py-2 px-4 rounded transition text-sm" title="Retry Connection">
                    <i class="fas fa-sync"></i>
                </button>
                <button onclick="syncDeviceTime('${device.id}')" class="bg-purple-600 hover:bg-purple-700 text-white py-2 px-4 rounded transition text-sm" title="Sync Time" ${!device.connected ? 'disabled style="opacity:0.5;cursor:not-allowed;"' : ''}>
                    <i class="fas fa-clock"></i>
                </button>
                <button onclick="deleteDevice('${device.id}')" class="bg-red-600 hover:bg-red-700 text-white py-2 px-4 rounded transition text-sm" title="Delete Device">
                    <i class="fas fa-trash"></i>
                </button>
            </div>

            ${device.isActive ? `
                <div class="mt-4 border-t pt-4">
                    <h5 class="text-sm font-semibold text-gray-700 mb-2"><i class="fas fa-sliders-h mr-2"></i>Live Controls</h5>
                    <div class="flex flex-wrap gap-2">
                        <button onclick="openGate('${device.id}')" class="bg-indigo-600 hover:bg-indigo-700 text-white py-2 px-4 rounded transition text-sm">
                            <i class="fas fa-door-open mr-2"></i>Open Gate
                        </button>
                        <button onclick="setLED('green','${device.id}')" class="bg-green-600 hover:bg-green-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-lightbulb mr-2"></i>LED Green
                        </button>
                        <button onclick="setLED('blue','${device.id}')" class="bg-blue-600 hover:bg-blue-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-lightbulb mr-2"></i>LED Blue
                        </button>
                        <button onclick="setLED('red','${device.id}')" class="bg-red-600 hover:bg-red-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-lightbulb mr-2"></i>LED Red
                        </button>
                        <button onclick="setLED('off','${device.id}')" class="bg-gray-600 hover:bg-gray-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-toggle-off mr-2"></i>LED Off
                        </button>
                        <button onclick="syncDatabaseToDevice('${device.id}')" class="bg-purple-600 hover:bg-purple-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-database mr-2"></i>Sync Database
                        </button>
                        <button onclick="toggleInputMode('${device.id}')" class="bg-purple-500 hover:bg-purple-600 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-keyboard mr-2"></i>Input Mode
                        </button>
                        <button onclick="runSelfTest('${device.id}')" class="bg-slate-600 hover:bg-slate-700 text-white py-2 px-3 rounded transition text-sm">
                            <i class="fas fa-stethoscope mr-2"></i>Self Test
                        </button>
                    </div>
                </div>
            ` : `
                <p class="mt-4 text-xs text-gray-500">Set this device as active to access live controls.</p>
            `}
        </div>
    `).join('');
}

function updateActiveDeviceDisplay() {
    const activeDeviceDisplay = document.getElementById('activeDeviceDisplay');
    const activeDeviceName = document.getElementById('activeDeviceName');
    const retryBtn = document.getElementById('retryBtn');
    
    if (activeDeviceId) {
        const device = devices.find(d => d.id === activeDeviceId);
        if (device) {
            activeDeviceDisplay.classList.remove('hidden');
            activeDeviceName.textContent = device.name;
            
            if (!device.connected) {
                retryBtn.classList.remove('hidden');
            } else {
                retryBtn.classList.add('hidden');
            }
        }
    } else {
        activeDeviceDisplay.classList.add('hidden');
        retryBtn.classList.add('hidden');
    }
}

function showAddDeviceModal() {
    document.getElementById('addDeviceModal').classList.remove('hidden');
}

function hideAddDeviceModal() {
    document.getElementById('addDeviceModal').classList.add('hidden');
    document.getElementById('newDeviceName').value = '';
    document.getElementById('newDeviceIP').value = '';
    document.getElementById('newDeviceLocation').value = '';
    document.getElementById('newDeviceDescription').value = '';
}

async function addDevice() {
    const name = document.getElementById('newDeviceName').value.trim();
    const ip = document.getElementById('newDeviceIP').value.trim();
    const location = document.getElementById('newDeviceLocation').value.trim();
    const description = document.getElementById('newDeviceDescription').value.trim();
    
    if (!name || !ip) {
        showNotification('Please fill in device name and IP address', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/devices/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, ip, location, description })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification(`Device "${name}" added successfully`, 'success');
            hideAddDeviceModal();
            await loadDevices();
            await checkConnection();
        } else {
            showNotification('Failed to add device', 'error');
        }
    } catch (error) {
        showNotification('Failed to add device: ' + error.message, 'error');
    }
}

async function setActiveDevice(deviceId, options = {}) {
    const { silent = false } = options;
    try {
        const response = await fetch('/api/devices/set-active', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ deviceId })
        });
        
        const data = await response.json();
        
        if (data.success) {
            activeDeviceId = deviceId;
            if (!silent) showNotification('Active device switched', 'success');
            await loadDevices();
            await checkConnection();
            
            // Refresh current tab data
            const activeTab = document.querySelector('.tab-btn.active').id.replace('tab-', '');
            if (activeTab === 'overview') {
                await loadSystemInfo();
                await loadState();
            } else if (activeTab === 'users') {
                await loadUsers();
            } else if (activeTab === 'events') {
                await loadEvents();
            }
            return true;
        }
        if (!silent) showNotification('Failed to switch device', 'error');
        return false;
    } catch (error) {
        if (!silent) showNotification('Failed to switch device', 'error');
        return false;
    }
}

async function deleteDevice(deviceId) {
    const device = devices.find(d => d.id === deviceId);
    if (!confirm(`Are you sure you want to delete device "${device.name}"?`)) return;
    
    try {
        const response = await fetch(`/api/devices/${deviceId}`, {
            method: 'DELETE'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification('Device deleted', 'success');
            await loadDevices();
            await checkConnection();
        }
    } catch (error) {
        showNotification('Failed to delete device', 'error');
    }
}

async function retryDeviceConnection(deviceId) {
    try {
        const response = await fetch(`/api/devices/${deviceId}/retry`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification('Reconnected successfully', 'success');
            await loadDevices();
            await checkConnection();
        } else {
            showNotification('Connection failed: ' + data.error, 'error');
        }
    } catch (error) {
        showNotification('Retry failed: ' + error.message, 'error');
    }
}

async function syncDeviceTime(deviceId) {
    try {
        const response = await fetch(`/api/devices/${deviceId}/sync-time`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification('Time synchronized successfully', 'success');
        } else {
            showNotification('Time sync failed: ' + data.error, 'error');
        }
    } catch (error) {
        showNotification('Time sync failed: ' + error.message, 'error');
    }
}

async function retryConnection() {
    if (activeDeviceId) {
        await retryDeviceConnection(activeDeviceId);
    }
}

// Check ESP32 connection
async function checkConnection() {
    try {
        const response = await fetch('/api/connection/status');
        const data = await response.json();
        
        esp32Connected = data.connected;
        updateConnectionStatus(data);
        
        if (data.connected && data.device) {
            document.getElementById('totalEvents').textContent = '-';
        }
    } catch (error) {
        console.error('Connection check failed:', error);
        updateConnectionStatus({ connected: false, error: error.message });
    }
}

// Update connection status UI
function updateConnectionStatus(data) {
    const statusEl = document.getElementById('connectionStatus');
    const retryBtn = document.getElementById('retryBtn');
    
    if (data.connected && data.device) {
        statusEl.innerHTML = `
            <div class="w-3 h-3 bg-green-500 rounded-full pulse-green"></div>
            <span class="text-sm">Connected</span>
        `;
        retryBtn.classList.add('hidden');
    } else if (data.activeDeviceId) {
        statusEl.innerHTML = `
            <div class="w-3 h-3 bg-red-500 rounded-full pulse-red"></div>
            <span class="text-sm">Disconnected</span>
        `;
        retryBtn.classList.remove('hidden');
    } else {
        statusEl.innerHTML = `
            <div class="w-3 h-3 bg-gray-400 rounded-full"></div>
            <span class="text-sm">No Device Selected</span>
        `;
        retryBtn.classList.add('hidden');
    }
}

// ==================== Connection Logs ====================

async function loadConnectionLogs() {
    try {
        const response = await fetch('/api/logs/connections');
        const data = await response.json();
        
        const logsList = document.getElementById('connectionLogsList');
        
        if (!data.logs || data.logs.length === 0) {
            logsList.innerHTML = '<p class="text-gray-500 text-center py-4">No connection logs</p>';
            return;
        }
        
        const statusColors = {
            SUCCESS: 'green',
            FAILED: 'red',
            WARNING: 'yellow',
            INFO: 'blue'
        };
        
        logsList.innerHTML = data.logs.map(log => {
            const color = statusColors[log.status] || 'gray';
            return `
                <div class="bg-${color}-50 border-l-4 border-${color}-400 p-4 rounded">
                    <div class="flex justify-between items-start">
                        <div class="flex-1">
                            <div class="flex items-center space-x-2 mb-1">
                                <span class="px-2 py-1 bg-${color}-100 text-${color}-800 text-xs font-semibold rounded">${log.action}</span>
                                <span class="px-2 py-1 bg-${color}-100 text-${color}-800 text-xs font-semibold rounded">${log.status}</span>
                            </div>
                            <p class="text-sm text-${color}-800 mt-2">${log.message}</p>
                        </div>
                        <span class="text-xs text-${color}-600">${new Date(log.timestamp).toLocaleString()}</span>
                    </div>
                </div>
            `;
        }).join('');
    } catch (error) {
        console.error('Failed to load connection logs:', error);
    }
}

async function clearConnectionLogs() {
    if (!confirm('Are you sure you want to clear all connection logs?')) return;
    
    try {
        await fetch('/api/logs/connections', { method: 'DELETE' });
        await loadConnectionLogs();
        showNotification('Connection logs cleared', 'success');
    } catch (error) {
        showNotification('Failed to clear logs', 'error');
    }
}

// ==================== System Info & State ====================

// Load system info
async function loadSystemInfo() {
    if (!activeDeviceId) return;
    
    try {
        const response = await fetch('/api/esp32/info');
        const data = await response.json();
        
        document.getElementById('fwVersion').textContent = data.fwVersion || '-';
        document.getElementById('wifiSSID').textContent = data.ssid || '-';
        document.getElementById('ipAddress').textContent = data.ip || '-';
        document.getElementById('uptime').textContent = formatUptime(data.uptime) || '-';
        document.getElementById('freeHeap').textContent = formatBytes(data.freeHeap) || '-';
    } catch (error) {
        console.error('Failed to load system info:', error);
    }
}

// Load state
async function loadState() {
    if (!activeDeviceId) return;
    
    try {
        const response = await fetch('/api/esp32/state');
        const data = await response.json();
        
        // Update stats
        document.getElementById('totalUsers').textContent = data.users?.length || 0;
        document.getElementById('usersInside').textContent = data.users?.filter(u => u.in).length || 0;
        
        // Update header card stats
        const totalUsersCard = document.getElementById('totalUsersCard');
        if (totalUsersCard) {
            totalUsersCard.textContent = data.users?.length || 0;
        }
        
        // Update input mode status based on ESP32 admin mode
        const inputModeEl = document.getElementById('inputMode');
        if (inputModeEl) {
            inputModeEl.innerHTML = data.adminOn ? 
                '<span class="text-green-600"><i class="fas fa-check mr-1"></i>Ready</span>' : 
                '<span class="text-gray-600"><i class="fas fa-times mr-1"></i>Off</span>';
        }
        
        // Update status
        document.getElementById('gateStatus').innerHTML = data.gateOpen ? 
            '<span class="text-green-600"><i class="fas fa-lock-open mr-1"></i>Open</span>' : 
            '<span class="text-red-600"><i class="fas fa-lock mr-1"></i>Closed</span>';
        document.getElementById('rfidStatus').innerHTML = '<span class="text-green-600"><i class="fas fa-check mr-1"></i>Active</span>';
        document.getElementById('oledStatus').innerHTML = '<span class="text-green-600"><i class="fas fa-check mr-1"></i>Active</span>';
        
    } catch (error) {
        console.error('Failed to load state:', error);
    }
}

// Load users
async function loadUsers() {
    if (!activeDeviceId) {
        document.getElementById('usersTable').innerHTML = '<tr><td colspan="6" class="px-6 py-4 text-center text-gray-500">Please select a device first</td></tr>';
        return;
    }
    
    try {
        const response = await fetch('/api/esp32/state');
        const data = await response.json();
        
        const tbody = document.getElementById('usersTable');
        if (!data.users || data.users.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" class="px-6 py-4 text-center text-gray-500">No users found</td></tr>';
            return;
        }
        
        tbody.innerHTML = data.users.map(user => `
            <tr class="hover:bg-gray-50">
                <td class="px-6 py-4 whitespace-nowrap text-sm font-mono">${user.uid}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">${user.name}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                    <span class="px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${user.type === 'STA' ? 'bg-blue-100 text-blue-800' : 'bg-green-100 text-green-800'}">
                        ${user.type === 'STA' ? 'Static' : 'Dynamic'}
                    </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-gray-500">${formatCurrency(user.credit)}</td>
                <td class="px-6 py-4 whitespace-nowrap text-sm">
                    <span class="px-2 inline-flex text-xs leading-5 font-semibold rounded-full ${user.in ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'}">
                        ${user.in ? 'Inside' : 'Outside'}
                    </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm font-medium space-x-2">
                    <button onclick="addCredit('${user.uid}')" class="text-blue-600 hover:text-blue-900" title="Add Credit">
                        <i class="fas fa-plus-circle"></i>
                    </button>
                    <button onclick="deleteUser('${user.uid}')" class="text-red-600 hover:text-red-900" title="Delete">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            </tr>
        `).join('');
    } catch (error) {
        console.error('Failed to load users:', error);
    }
}

// Load events
async function loadEvents() {
    if (!activeDeviceId) {
        document.getElementById('eventsList').innerHTML = '<p class="text-gray-500 text-center py-4">Please select a device first</p>';
        document.getElementById('recentActivity').innerHTML = '<p class="text-gray-500 text-center py-4">No recent activity</p>';
        return;
    }
    
    try {
        const response = await fetch('/api/esp32/events');
        const data = await response.json();
        
        document.getElementById('totalEvents').textContent = data.events?.length || 0;
        
        const eventsList = document.getElementById('eventsList');
        const recentActivity = document.getElementById('recentActivity');
        
        if (!data.events || data.events.length === 0) {
            eventsList.innerHTML = '<p class="text-gray-500 text-center py-4">No events</p>';
            recentActivity.innerHTML = '<p class="text-gray-500 text-center py-4">No recent activity</p>';
            return;
        }
        
        const eventsHTML = data.events.map(event => `
            <div class="bg-gray-50 rounded-lg p-3 flex justify-between items-start">
                <div>
                    <p class="text-sm text-gray-800">${event.msg}</p>
                    <p class="text-xs text-gray-500 mt-1">${formatTimestamp(event.t)}</p>
                </div>
            </div>
        `).join('');
        
        eventsList.innerHTML = eventsHTML;
        recentActivity.innerHTML = data.events.slice(0, 5).map(event => `
            <div class="flex justify-between items-start py-2 border-b border-gray-200 last:border-0">
                <p class="text-sm text-gray-800">${event.msg}</p>
                <p class="text-xs text-gray-500">${formatTimestamp(event.t)}</p>
            </div>
        `).join('');
    } catch (error) {
        console.error('Failed to load events:', error);
    }
}

// Load alerts
async function loadAlerts() {
    try {
        const response = await fetch('/api/alerts');
        const data = await response.json();
        
        const alertsList = document.getElementById('alertsList');
        const alertBadge = document.getElementById('alertBadge');
        
        if (!data.alerts || data.alerts.length === 0) {
            alertsList.innerHTML = '<p class="text-gray-500 text-center py-4">No alerts</p>';
            alertBadge.classList.add('hidden');
            return;
        }
        
        alertBadge.textContent = data.alerts.length;
        alertBadge.classList.remove('hidden');
        
        alertsList.innerHTML = data.alerts.map(alert => `
            <div class="bg-yellow-50 border-l-4 border-yellow-400 p-4 rounded">
                <div class="flex">
                    <div class="flex-shrink-0">
                        <i class="fas fa-exclamation-triangle text-yellow-400"></i>
                    </div>
                    <div class="ml-3">
                        <p class="text-sm text-yellow-800">${alert.message || JSON.stringify(alert)}</p>
                        <p class="text-xs text-yellow-600 mt-1">${new Date(alert.timestamp).toLocaleString()}</p>
                    </div>
                </div>
            </div>
        `).join('');
    } catch (error) {
        console.error('Failed to load alerts:', error);
    }
}

// Clear alerts
async function clearAlerts() {
    if (!confirm('Are you sure you want to clear all alerts?')) return;
    
    try {
        await fetch('/api/alerts', { method: 'DELETE' });
        await loadAlerts();
        showNotification('Alerts cleared successfully', 'success');
    } catch (error) {
        showNotification('Failed to clear alerts', 'error');
    }
}

// ==================== Control Functions ====================

async function ensureActiveDevice(targetDeviceId) {
    if (!targetDeviceId) return false;
    if (activeDeviceId === targetDeviceId) return true;
    const switched = await setActiveDevice(targetDeviceId, { silent: true });
    if (switched) {
        activeDeviceId = targetDeviceId;
    }
    return switched;
}

async function openGate(deviceId = null) {
    const targetId = deviceId || activeDeviceId;
    if (!targetId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    const ready = await ensureActiveDevice(targetId);
    if (!ready) {
        showNotification('Unable to reach the selected device', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/esp32/open', { method: 'POST' });
        const data = await response.json();
        showNotification('Gate opened successfully', 'success');
        setTimeout(() => loadState(), 1000);
    } catch (error) {
        showNotification('Failed to open gate', 'error');
    }
}

async function setLED(color, deviceId = null) {
    const targetId = deviceId || activeDeviceId;
    if (!targetId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    const ready = await ensureActiveDevice(targetId);
    if (!ready) {
        showNotification('Unable to reach the selected device', 'error');
        return;
    }
    
    try {
        const colorMap = {
            red: { r: true, g: false, b: false },
            green: { r: false, g: true, b: false },
            blue: { r: false, g: false, b: true },
            off: { r: false, g: false, b: false }
        };
        const key = (color || 'off').toLowerCase();
        const payload = colorMap[key] || colorMap.off;

        await fetch('/api/esp32/led', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        showNotification(`LED set to ${key}`, 'success');
    } catch (error) {
        showNotification('Failed to control LED', 'error');
    }
}

// Input mode for scanning and adding new users
let inputModeActive = false;
let inputModeDeviceId = null;

async function toggleInputMode(deviceId = null) {
    const targetId = deviceId || activeDeviceId;
    if (!targetId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    const ready = await ensureActiveDevice(targetId);
    if (!ready) {
        showNotification('Unable to reach the selected device', 'error');
        return;
    }
    
    inputModeActive = !inputModeActive;
    inputModeDeviceId = inputModeActive ? targetId : null;
    
    try {
        // Toggle input mode on ESP32
        const response = await fetch('/api/esp32/input/mode', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mode: inputModeActive ? 'on' : 'off' })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        if (inputModeActive) {
            showNotification('Input mode enabled - scan a card to add new user', 'success');
            startInputModePolling();
        } else {
            showNotification('Input mode disabled', 'info');
            stopInputModePolling();
        }
        
        updateInputModeUI();
        setTimeout(() => loadState(), 1000);
    } catch (error) {
        console.error('Toggle input mode error:', error);
        showNotification('Failed to toggle input mode: ' + error.message, 'error');
        inputModeActive = false;
        inputModeDeviceId = null;
        updateInputModeUI();
    }
}

let inputModePollingInterval = null;

function startInputModePolling() {
    if (inputModePollingInterval) return;
    
    inputModePollingInterval = setInterval(async () => {
        if (!inputModeActive || !inputModeDeviceId) {
            stopInputModePolling();
            return;
        }
        
        try {
            // Use the enhanced input endpoint
            const response = await fetch('/api/esp32/input/last?clear=true');
            if (!response.ok) {
                if (response.status === 400) {
                    console.log('No active device selected for input mode');
                    return;
                }
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const data = await response.json();
            
            // Check if there's a new card scan
            if (data.hasInput && data.uid && data.isNew) {
                console.log('New card detected in input mode:', data.uid);
                handleNewCardScanned(data.uid);
            }
        } catch (error) {
            console.error('Error polling for card scans:', error);
            // If device is not connected, stop input mode
            if (error.message.includes('503') || error.message.includes('not connected')) {
                inputModeActive = false;
                inputModeDeviceId = null;
                updateInputModeUI();
                stopInputModePolling();
                showNotification('Device disconnected - input mode disabled', 'warning');
            }
        }
    }, 2000); // Poll every 2 seconds
}

function stopInputModePolling() {
    if (inputModePollingInterval) {
        clearInterval(inputModePollingInterval);
        inputModePollingInterval = null;
    }
}

function extractUIDFromEvent(message) {
    const uidMatch = message.match(/UID:\s*([A-F0-9:]+)/i);
    return uidMatch ? uidMatch[1] : null;
}

function handleNewCardScanned(uid) {
    // Stop input mode
    inputModeActive = false;
    inputModeDeviceId = null;
    stopInputModePolling();
    
    // Update UI
    updateInputModeUI();
    
    // Show modal to add user with pre-filled UID
    showAddDatabaseUserModal();
    document.getElementById('newDbUserUID').value = uid;
    
    showNotification(`Card scanned! UID: ${uid}`, 'success');
}

function updateInputModeUI() {
    const statusEl = document.getElementById('inputModeStatus');
    const deviceInputEl = document.getElementById('inputMode');
    
    if (statusEl) {
        statusEl.textContent = inputModeActive ? 'ON' : 'OFF';
        statusEl.className = inputModeActive ? 
            'text-3xl font-bold text-green-600' : 
            'text-3xl font-bold text-gray-800';
    }
    
    if (deviceInputEl) {
        deviceInputEl.innerHTML = inputModeActive ? 
            '<span class="text-green-600"><i class="fas fa-check mr-1"></i>Scanning</span>' : 
            '<span class="text-gray-600"><i class="fas fa-times mr-1"></i>Off</span>';
    }
}

async function runSelfTest(deviceId = null) {
    const targetId = deviceId || activeDeviceId;
    if (!targetId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    const ready = await ensureActiveDevice(targetId);
    if (!ready) {
        showNotification('Unable to reach the selected device', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/esp32/selftest');
        const data = await response.json();
        alert('Self Test Results:\n\n' + JSON.stringify(data, null, 2));
    } catch (error) {
        showNotification('Self test failed', 'error');
    }
}

// ==================== User Management ====================

function showAddUserModal() {
    if (!activeDeviceId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    document.getElementById('addUserModal').classList.remove('hidden');
}

function hideAddUserModal() {
    document.getElementById('addUserModal').classList.add('hidden');
    document.getElementById('newUserUID').value = '';
    document.getElementById('newUserName').value = '';
    document.getElementById('newUserCredit').value = '100000';
}

async function addUser() {
    const uid = document.getElementById('newUserUID').value;
    const name = document.getElementById('newUserName').value;
    const credit = parseInt(document.getElementById('newUserCredit').value);
    
    if (!uid || !name) {
        showNotification('Please fill all fields', 'error');
        return;
    }
    
    try {
        await fetch('/api/esp32/users/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ uid, name, credit })
        });
        showNotification('User added successfully', 'success');
        hideAddUserModal();
        setTimeout(() => loadUsers(), 1000);
    } catch (error) {
        showNotification('Failed to add user', 'error');
    }
}

async function deleteUser(uid) {
    if (!confirm(`Are you sure you want to delete user ${uid}?`)) return;
    
    try {
        await fetch('/api/esp32/users/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ uid })
        });
        showNotification('User deleted successfully', 'success');
        setTimeout(() => loadUsers(), 1000);
    } catch (error) {
        showNotification('Failed to delete user', 'error');
    }
}

async function addCredit(uid) {
    const amount = prompt('Enter credit amount to add (VND):');
    if (!amount) return;
    
    try {
        await fetch('/api/esp32/credit/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ uid, amount: parseInt(amount) })
        });
        showNotification('Credit added successfully', 'success');
        setTimeout(() => loadUsers(), 1000);
    } catch (error) {
        showNotification('Failed to add credit', 'error');
    }
}

async function exportUsers() {
    if (!activeDeviceId) {
        showNotification('Please select a device first', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/esp32/users/export');
        const data = await response.json();
        
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `users-export-${new Date().toISOString().split('T')[0]}.json`;
        a.click();
        window.URL.revokeObjectURL(url);
        
        showNotification('Users exported successfully', 'success');
    } catch (error) {
        showNotification('Failed to export users', 'error');
    }
}

// Settings (now redirects to devices tab)
function showSettings() {
    document.getElementById('settingsModal').classList.remove('hidden');
}

function hideSettings() {
    document.getElementById('settingsModal').classList.add('hidden');
}

// Tab switching
function switchTab(tabName) {
    // Update tab buttons
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active', 'border-b-2', 'border-blue-600', 'text-blue-600');
        btn.classList.add('text-gray-500');
    });
    document.getElementById(`tab-${tabName}`).classList.add('active', 'border-b-2', 'border-blue-600', 'text-blue-600');
    document.getElementById(`tab-${tabName}`).classList.remove('text-gray-500');
    
    // Update content
    document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.add('hidden');
    });
    document.getElementById(`content-${tabName}`).classList.remove('hidden');
    
    // Refresh data for specific tabs
    if (tabName === 'devices') loadDevices();
    if (tabName === 'users') loadUsers();
    if (tabName === 'database') loadDatabaseUsers();
    if (tabName === 'events') loadEvents();
    if (tabName === 'alerts') loadAlerts();
    if (tabName === 'logs') loadConnectionLogs();
    if (tabName === 'overview') {
        loadSystemInfo();
        loadState();
        loadEvents();
    }
}

// Auto refresh
function startAutoRefresh() {
    refreshInterval = setInterval(async () => {
        await checkConnection();
        await loadDevices();
        
        if (esp32Connected && activeDeviceId) {
            const activeTab = document.querySelector('.tab-btn.active')?.id.replace('tab-', '');
            if (activeTab === 'overview') {
                await loadSystemInfo();
                await loadState();
                await loadEvents();
            }
        }
        await loadAlerts();
    }, 5000); // Refresh every 5 seconds
}

// Utility functions
function formatUptime(seconds) {
    if (!seconds) return '-';
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    return `${days}d ${hours}h ${minutes}m`;
}

function formatBytes(bytes) {
    if (!bytes) return '-';
    return `${(bytes / 1024).toFixed(2)} KB`;
}

function formatCurrency(amount) {
    if (amount === undefined) return '-';
    return new Intl.NumberFormat('vi-VN', { style: 'currency', currency: 'VND' }).format(amount);
}

function formatTimestamp(seconds) {
    if (!seconds) return '-';
    const date = new Date(seconds * 1000);
    return date.toLocaleString();
}

function showNotification(message, type = 'info') {
    const colors = {
        success: 'bg-green-500',
        error: 'bg-red-500',
        info: 'bg-blue-500'
    };
    
    const notification = document.createElement('div');
    notification.className = `fixed top-4 right-4 ${colors[type]} text-white px-6 py-3 rounded-lg shadow-lg z-50 transition-opacity duration-300`;
    notification.textContent = message;
    
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.style.opacity = '0';
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

// ==================== Central Database Management ====================

async function loadDatabaseUsers() {
    try {
        const response = await fetch('/api/database/users');
        const data = await response.json();
        
        if (data.success) {
            updateDatabaseUsersTable(data.users);
        } else {
            console.error('Failed to load database users:', data.error);
            showNotification('Failed to load database users', 'error');
        }
    } catch (error) {
        console.error('Error loading database users:', error);
        showNotification('Error loading database users', 'error');
    }
}

function updateDatabaseUsersTable(users) {
    const tbody = document.getElementById('databaseUsersTable');
    
    if (!users || users.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="7" class="px-6 py-4 text-center text-gray-500">
                    <i class="fas fa-database text-4xl mb-2 opacity-50"></i>
                    <p>No users in central database</p>
                    <p class="text-sm mt-1">Click "Add User" to add users to the central database</p>
                </td>
            </tr>
        `;
        return;
    }
    
    tbody.innerHTML = users.map(user => {
        const statusColor = user.in ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800';
        const statusText = user.in ? 'Inside' : 'Outside';
        const typeColor = user.type === 'static' ? 'bg-purple-100 text-purple-800' : 'bg-blue-100 text-blue-800';
        const typeText = user.type === 'static' ? 'Admin' : 'Regular';
        
        return `
            <tr class="hover:bg-gray-50">
                <td class="px-6 py-4 whitespace-nowrap">
                    <code class="text-sm bg-gray-100 px-2 py-1 rounded">${user.uid}</code>
                </td>
                <td class="px-6 py-4 whitespace-nowrap font-medium">${user.name}</td>
                <td class="px-6 py-4 whitespace-nowrap">
                    <span class="px-2 py-1 inline-flex text-xs leading-5 font-semibold rounded-full ${typeColor}">
                        ${typeText}
                    </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap">${formatCurrency(user.credit)}</td>
                <td class="px-6 py-4 whitespace-nowrap">
                    <span class="px-2 py-1 inline-flex text-xs leading-5 font-semibold rounded-full ${statusColor}">
                        ${statusText}
                    </span>
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                    ${user.createdAt ? new Date(user.createdAt).toLocaleString() : '-'}
                </td>
                <td class="px-6 py-4 whitespace-nowrap text-sm space-x-2">
                    <button onclick="addCreditToDbUser('${user.uid}')" class="text-green-600 hover:text-green-900" title="Add Credit">
                        <i class="fas fa-plus-circle"></i>
                    </button>
                    <button onclick="editDatabaseUser('${user.uid}')" class="text-blue-600 hover:text-blue-900" title="Edit User">
                        <i class="fas fa-edit"></i>
                    </button>
                    <button onclick="deleteDatabaseUser('${user.uid}')" class="text-red-600 hover:text-red-900" title="Delete User">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            </tr>
        `;
    }).join('');
}

// Modal functions for database user management
function showAddDatabaseUserModal() {
    document.getElementById('addDatabaseUserModal').classList.remove('hidden');
    document.getElementById('newDbUserUID').value = '';
    document.getElementById('newDbUserName').value = '';
    document.getElementById('newDbUserCredit').value = '100000';
    document.getElementById('newDbUserType').value = 'dynamic';
}

function hideAddDatabaseUserModal() {
    document.getElementById('addDatabaseUserModal').classList.add('hidden');
}

async function addDatabaseUser() {
    const uid = document.getElementById('newDbUserUID').value.trim();
    const name = document.getElementById('newDbUserName').value.trim();
    const credit = parseInt(document.getElementById('newDbUserCredit').value);
    const type = document.getElementById('newDbUserType').value;
    
    if (!uid || !name) {
        showNotification('Please fill in all fields', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/database/users/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ uid, name, credit, type })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification('User added to central database successfully', 'success');
            hideAddDatabaseUserModal();
            await loadDatabaseUsers();
        } else {
            showNotification(data.error || 'Failed to add user', 'error');
        }
    } catch (error) {
        console.error('Error adding database user:', error);
        showNotification('Error adding user to database', 'error');
    }
}

async function deleteDatabaseUser(uid) {
    if (!confirm(`Are you sure you want to delete user ${uid} from the central database?\n\nThis will NOT automatically remove them from devices. You need to sync to devices after deletion.`)) {
        return;
    }
    
    try {
        const response = await fetch(`/api/database/users/${encodeURIComponent(uid)}`, {
            method: 'DELETE'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification('User deleted from central database', 'success');
            await loadDatabaseUsers();
        } else {
            showNotification(data.error || 'Failed to delete user', 'error');
        }
    } catch (error) {
        console.error('Error deleting database user:', error);
        showNotification('Error deleting user', 'error');
    }
}

async function addCreditToDbUser(uid) {
    const amount = prompt('Enter credit amount to add (VND):');
    if (!amount || isNaN(amount)) return;
    
    try {
        const response = await fetch(`/api/database/users/${encodeURIComponent(uid)}/credit`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ amount: parseInt(amount) })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification(`Added ${formatCurrency(amount)} to user credit`, 'success');
            await loadDatabaseUsers();
        } else {
            showNotification(data.error || 'Failed to add credit', 'error');
        }
    } catch (error) {
        console.error('Error adding credit:', error);
        showNotification('Error adding credit', 'error');
    }
}

async function editDatabaseUser(uid) {
    showNotification('Edit functionality coming soon', 'info');
    // TODO: Implement edit modal
}

async function syncDatabaseToAll() {
    if (!confirm('Sync central database to all connected devices?\n\nThis will overwrite the user data on all devices.')) {
        return;
    }
    
    try {
        const response = await fetch('/api/database/sync-all', {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (data.success) {
            const successCount = data.results.filter(r => r.success).length;
            const totalCount = data.results.length;
            showNotification(`Synced to ${successCount}/${totalCount} devices`, 'success');
            
            // Show details
            data.results.forEach(result => {
                if (!result.success) {
                    console.error(`Failed to sync to ${result.deviceId}:`, result.error);
                }
            });
        } else {
            showNotification(data.error || 'Failed to sync database', 'error');
        }
    } catch (error) {
        console.error('Error syncing database:', error);
        showNotification('Error syncing database to devices', 'error');
    }
}

async function syncDatabaseToDevice(deviceId) {
    try {
        const response = await fetch(`/api/database/sync/${encodeURIComponent(deviceId)}`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showNotification(`Database synced to device ${deviceId}`, 'success');
        } else {
            showNotification(data.error || 'Failed to sync to device', 'error');
        }
    } catch (error) {
        console.error('Error syncing to device:', error);
        showNotification('Error syncing database to device', 'error');
    }
}

async function exportDatabaseUsers() {
    try {
        const response = await fetch('/api/database/users');
        const data = await response.json();
        
        if (data.success) {
            const blob = new Blob([JSON.stringify(data.users, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `database-users-${new Date().toISOString()}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            showNotification('Database users exported', 'success');
        }
    } catch (error) {
        console.error('Error exporting database users:', error);
        showNotification('Error exporting users', 'error');
    }
}
