#include "users.h"
#include "hardware.h"
#include "display.h"
#include "network.h"

// ================== User Management State ==================
std::vector<User> staticUsers;
std::vector<User> dynamicUsers;
bool inputModeActive = false;
LastScanResult lastScan;
Preferences userPrefs;

// ================== Card Debounce State ==================
String lastCardUID = "";
unsigned long lastCardTime = 0;
const unsigned long CARD_DEBOUNCE_MS = 2000; // 2 seconds debounce

// ================== Constants ==================
const long COST_PER_EXIT = 3000;    // 3,000 VND per exit
const long DEFAULT_CREDIT = 100000; // 100,000 VND default credit
const size_t STATIC_COUNT = 0;      // No static users in modular version

// ================== User Management Initialization ==================
bool initializeUsers() {
    Serial.println("Initializing user management...");
    
    if (!userPrefs.begin("users", false)) {
        Serial.println("Failed to initialize user preferences");
        return false;
    }
    loadUsersFromNVS();
    
    Serial.printf("Loaded %d static users, %d dynamic users\n", 
                  staticUsers.size(), dynamicUsers.size());
    Serial.println("User management initialized");
    return true;
}

// ================== UID Processing Functions ==================
String normalizeUID(const String& uid) {
    String normalized = uid;
    normalized.toUpperCase();
    normalized.replace("-", ":");
    normalized.replace("_", ":");
    
    // If no colons and even length, insert colons every 2 chars
    if (normalized.indexOf(':') < 0 && (normalized.length() % 2 == 0)) {
        String formatted;
        for (size_t i = 0; i < normalized.length(); i += 2) {
            formatted += normalized.substring(i, i + 2);
            if (i + 2 < normalized.length()) {
                formatted += ":";
            }
        }
        normalized = formatted;
    }
    
    // Validate format
    int lastPos = 0, parts = 0;
    while (true) {
        int colonPos = normalized.indexOf(':', lastPos);
        String part = (colonPos < 0) ? normalized.substring(lastPos) : normalized.substring(lastPos, colonPos);
        
        if (part.length() != 2) return String(); // Invalid part length
        
        // Check if part contains valid hex characters
        for (int i = 0; i < 2; i++) {
            char c = part[i];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                return String(); // Invalid hex character
            }
        }
        
        parts++;
        if (colonPos < 0) break;
        lastPos = colonPos + 1;
    }
    
    if (parts < 4) return String(); // Need at least 4 parts for valid UID
    return normalized;
}

void loadUsersFromNVS() {
    // Load static users
    size_t staticCount = userPrefs.getUInt("static_count", 0);
    staticUsers.clear();
    staticUsers.reserve(staticCount);
    
    for (size_t i = 0; i < staticCount; i++) {
        String keyPrefix = "s" + String(i) + "_";
        
        String uid = userPrefs.getString((keyPrefix + "uid").c_str(), "");
        String name = userPrefs.getString((keyPrefix + "name").c_str(), "");
        long credit = userPrefs.getLong((keyPrefix + "credit").c_str(), DEFAULT_CREDIT);
        bool in = userPrefs.getBool((keyPrefix + "in").c_str(), false);
        
        if (uid.length() > 0 && name.length() > 0) {
            staticUsers.emplace_back(uid, name, credit, in, USER_STATIC);
        }
    }
    
    // Load dynamic users
    size_t dynamicCount = userPrefs.getUInt("dynamic_count", 0);
    dynamicUsers.clear();
    dynamicUsers.reserve(dynamicCount);
    
    for (size_t i = 0; i < dynamicCount; i++) {
        String keyPrefix = "d" + String(i) + "_";
        
        String uid = userPrefs.getString((keyPrefix + "uid").c_str(), "");
        String name = userPrefs.getString((keyPrefix + "name").c_str(), "");
        long credit = userPrefs.getLong((keyPrefix + "credit").c_str(), DEFAULT_CREDIT);
        bool in = userPrefs.getBool((keyPrefix + "in").c_str(), false);
        
        if (uid.length() > 0 && name.length() > 0) {
            dynamicUsers.emplace_back(uid, name, credit, in, USER_DYNAMIC);
        }
    }
}

void saveUsersToBothNVS() {
    saveStaticUsersToNVS();
    saveDynamicUsersToNVS();
}

void saveDynamicUsersToNVS() {
    // Clear old dynamic user data
    size_t oldCount = userPrefs.getUInt("dynamic_count", 0);
    for (size_t i = 0; i < oldCount; i++) {
        String keyPrefix = "d" + String(i) + "_";
        userPrefs.remove((keyPrefix + "uid").c_str());
        userPrefs.remove((keyPrefix + "name").c_str());
        userPrefs.remove((keyPrefix + "credit").c_str());
        userPrefs.remove((keyPrefix + "in").c_str());
    }
    
    // Save new dynamic user data
    userPrefs.putUInt("dynamic_count", dynamicUsers.size());
    for (size_t i = 0; i < dynamicUsers.size(); i++) {
        String keyPrefix = "d" + String(i) + "_";
        const User& user = dynamicUsers[i];
        
        userPrefs.putString((keyPrefix + "uid").c_str(), user.uid);
        userPrefs.putString((keyPrefix + "name").c_str(), user.name);
        userPrefs.putLong((keyPrefix + "credit").c_str(), user.credit);
        userPrefs.putBool((keyPrefix + "in").c_str(), user.in);
    }
}

void saveStaticUsersToNVS() {
    // Clear old static user data
    size_t oldCount = userPrefs.getUInt("static_count", 0);
    for (size_t i = 0; i < oldCount; i++) {
        String keyPrefix = "s" + String(i) + "_";
        userPrefs.remove((keyPrefix + "uid").c_str());
        userPrefs.remove((keyPrefix + "name").c_str());
        userPrefs.remove((keyPrefix + "credit").c_str());
        userPrefs.remove((keyPrefix + "in").c_str());
    }
    
    // Save new static user data
    userPrefs.putUInt("static_count", staticUsers.size());
    for (size_t i = 0; i < staticUsers.size(); i++) {
        String keyPrefix = "s" + String(i) + "_";
        const User& user = staticUsers[i];
        
        userPrefs.putString((keyPrefix + "uid").c_str(), user.uid);
        userPrefs.putString((keyPrefix + "name").c_str(), user.name);
        userPrefs.putLong((keyPrefix + "credit").c_str(), user.credit);
        userPrefs.putBool((keyPrefix + "in").c_str(), user.in);
    }
}

// ================== User Query Functions ==================
int findUserByUID(const String& uid) {
    String normalizedUID = normalizeUID(uid);
    
    // Check static users first
    for (size_t i = 0; i < staticUsers.size(); i++) {
        if (staticUsers[i].uid == normalizedUID) {
            return (int)i;
        }
    }
    
    // Check dynamic users
    for (size_t i = 0; i < dynamicUsers.size(); i++) {
        if (dynamicUsers[i].uid == normalizedUID) {
            return (int)(staticUsers.size() + i);
        }
    }
    
    return -1; // Not found
}

User* getUserByUID(const String& uid) {
    int index = findUserByUID(uid);
    if (index < 0) return nullptr;
    
    if (index < (int)staticUsers.size()) {
        return &staticUsers[index];
    } else {
        return &dynamicUsers[index - staticUsers.size()];
    }
}

User* getUserByIndex(int index) {
    if (index < 0) return nullptr;
    
    if (index < (int)staticUsers.size()) {
        return &staticUsers[index];
    } else {
        int dynamicIndex = index - staticUsers.size();
        if (dynamicIndex < (int)dynamicUsers.size()) {
            return &dynamicUsers[dynamicIndex];
        }
    }
    
    return nullptr;
}

int getTotalUserCount() {
    return staticUsers.size() + dynamicUsers.size();
}

int getStaticUserCount() {
    return staticUsers.size();
}

int getDynamicUserCount() {
    return dynamicUsers.size();
}

// ================== User CRUD Functions ==================
bool addUser(const String& uid, const String& name, long credit, UserType type) {
    String normalizedUID = normalizeUID(uid);
    
    if (!isValidUID(normalizedUID)) {
        Serial.println("Invalid UID format: " + uid);
        return false;
    }
    
    // Check if user already exists
    if (findUserByUID(normalizedUID) >= 0) {
        Serial.println("User already exists: " + normalizedUID);
        return false;
    }
    
    User newUser(normalizedUID, name, credit, false, type);
    
    if (type == USER_STATIC) {
        staticUsers.push_back(newUser);
        saveStaticUsersToNVS();
    } else {
        dynamicUsers.push_back(newUser);
        saveDynamicUsersToNVS();
    }
    
    Serial.println("Added user: " + name + " (" + normalizedUID + ")");
    return true;
}

bool updateUser(const String& uid, const String& name, long credit, bool in) {
    User* user = getUserByUID(uid);
    if (!user) {
        Serial.println("User not found for update: " + uid);
        return false;
    }
    
    if (name.length() > 0) {
        user->name = name;
    }
    if (credit >= 0) {
        user->credit = credit;
    }
    user->in = in;
    
    // Save to appropriate storage
    if (user->type == USER_STATIC) {
        saveStaticUsersToNVS();
    } else {
        saveDynamicUsersToNVS();
    }
    
    Serial.println("Updated user: " + user->name + " (" + user->uid + ")");
    return true;
}

bool deleteUser(const String& uid) {
    String normalizedUID = normalizeUID(uid);
    
    // Check static users
    for (auto it = staticUsers.begin(); it != staticUsers.end(); ++it) {
        if (it->uid == normalizedUID) {
            Serial.println("Deleted static user: " + it->name + " (" + it->uid + ")");
            staticUsers.erase(it);
            saveStaticUsersToNVS();
            return true;
        }
    }
    
    // Check dynamic users
    for (auto it = dynamicUsers.begin(); it != dynamicUsers.end(); ++it) {
        if (it->uid == normalizedUID) {
            Serial.println("Deleted dynamic user: " + it->name + " (" + it->uid + ")");
            dynamicUsers.erase(it);
            saveDynamicUsersToNVS();
            return true;
        }
    }
    
    Serial.println("User not found for deletion: " + normalizedUID);
    return false;
}

void clearDynamicUsers() {
    int count = dynamicUsers.size();
    dynamicUsers.clear();
    saveDynamicUsersToNVS();
    Serial.printf("Cleared %d dynamic users\n", count);
}

// ================== RFID Processing Functions ==================
String uidToHex(const MFRC522::Uid& uid) {
    String result = "";
    for (byte i = 0; i < uid.size; i++) {
        if (uid.uidByte[i] < 0x10) result += "0";
        result += String(uid.uidByte[i], HEX);
        if (i + 1 != uid.size) result += ":";
    }
    result.toUpperCase();
    return result;
}

bool isCardPresent() {
    return rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial();
}

String readRFIDCard() {
    if (!isCardPresent()) {
        return String();
    }
    
    String uid = uidToHex(rfid.uid);
    Serial.println("RFID Card detected: " + uid);
    
    rfid.PICC_HaltA(); // Stop reading
    rfid.PCD_StopCrypto1();
    
    return uid;
}

bool isValidUID(const String& uid) {
    if (uid.length() < 8) return false;
    
    // Check format: XX:XX:XX:XX (at least 4 bytes)
    int colonCount = 0;
    for (int i = 0; i < uid.length(); i++) {
        char c = uid.charAt(i);
        if (c == ':') {
            colonCount++;
        } else if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    
    return colonCount >= 3; // At least 4 hex bytes
}

// ================== Card Processing Functions ==================
bool processCardScan(const String& uid) {
    Serial.println("Processing card scan for UID: " + uid);
    String normalizedUID = normalizeUID(uid);
    
    // Card debounce - ignore same card within 2 seconds
    unsigned long currentTime = millis();
    if (normalizedUID == lastCardUID && (currentTime - lastCardTime) < CARD_DEBOUNCE_MS) {
        Serial.println("Card scan ignored - too soon after last scan");
        return false;
    }
    
    lastCardUID = normalizedUID;
    lastCardTime = currentTime;
    
    if (inputModeActive) {
        // In input mode, record the scan and notify server
        bool isNewCard = findUserByUID(normalizedUID) < 0;
        setLastScan(normalizedUID, isNewCard);
        
        // Send UID to server for admin panel processing
        Serial.println("Input mode: Sending new UID to server - " + normalizedUID);
        RPCResponse response = notifyNewUID(normalizedUID, isNewCard);
        
        if (response.success) {
            showInputModeScreen("Card sent to server!\nUID: " + normalizedUID);
            Serial.println("Successfully notified server of new UID");
        } else {
            showInputModeScreen("Card detected:\n" + normalizedUID + "\n(Server offline)");
            Serial.println("Failed to notify server: " + response.error);
        }
        
        return true;
    }
    
    User* user = getUserByUID(normalizedUID);
    if (!user) {
        showAccessDeniedScreen("Unknown card");
        ledAccessDenied();
        Serial.println("Access denied - unknown UID: " + normalizedUID);
        return false;
    }
    
    bool isEntry = !user->in; // Opposite of current state
    
    if (checkAccess(*user, isEntry)) {
        updateUserState(*user, isEntry, isEntry ? 0 : COST_PER_EXIT);
        showAccessGrantedScreen(user->name, user->credit, isEntry);
        ledAccessGranted();
        gateOpen();
        
        Serial.printf("Access granted - %s (%s) %s, Credit: %ld\n", 
                     user->name.c_str(), normalizedUID.c_str(),
                     isEntry ? "IN" : "OUT", user->credit);
        return true;
    } else {
        showAccessDeniedScreen("Insufficient credit");
        ledAccessDenied();
        Serial.printf("Access denied - insufficient credit: %s (%ld VND)\n", 
                     user->name.c_str(), user->credit);
        return false;
    }
}

bool checkAccess(const User& user, bool isEntry) {
    if (isEntry) {
        return true; // Entry is always allowed
    } else {
        return hasValidCredit(user, COST_PER_EXIT); // Exit requires payment
    }
}

void updateUserState(User& user, bool isEntry, long cost) {
    user.in = isEntry;
    if (cost > 0) {
        deductCredit(user, cost);
    }
    
    // Save changes locally
    if (user.type == USER_STATIC) {
        saveStaticUsersToNVS();
    } else {
        saveDynamicUsersToNVS();
    }
    
    // Sync changes to server
    Serial.println("Syncing user changes to server...");
    RPCResponse syncResponse = updateUserOnServer(user.uid, user.name, user.credit, user.in);
    if (syncResponse.success) {
        Serial.println("User data successfully synced to server");
    } else {
        Serial.println("Failed to sync user data to server: " + syncResponse.error);
        Serial.println("Will retry on next periodic sync");
    }
}

// ================== Input Mode Functions ==================
void setInputModeActive(bool active) {
    inputModeActive = active;
    if (!active) {
        clearLastScan();
        // Return to idle screen when input mode is disabled
        showIdleScreen();
    } else {
        // Show input mode screen when activated
        showInputModeScreen("Waiting for card scan...");
    }
    
    Serial.println("Input mode: " + String(active ? "ACTIVE" : "INACTIVE"));
}

bool isInputModeActive() {
    return inputModeActive;
}

void setLastScan(const String& uid, bool isNew) {
    lastScan.uid = uid;
    lastScan.timestamp = millis() / 1000;
    lastScan.isNew = isNew;
}

LastScanResult getLastScan() {
    return lastScan;
}

void clearLastScan() {
    lastScan.uid = "";
    lastScan.timestamp = 0;
    lastScan.isNew = false;
}

// ================== Server Sync Functions ==================
bool syncUsersFromJson(const DynamicJsonDocument& doc) {
    if (!doc.containsKey("users")) {
        Serial.println("No users array in sync data");
        return false;
    }
    
    JsonArrayConst users = doc["users"].as<JsonArrayConst>();
    dynamicUsers.clear();
    
    int syncedCount = 0;
    for (JsonVariantConst userVariant : users) {
        JsonObjectConst userObj = userVariant.as<JsonObjectConst>();
        String uid = userObj["uid"] | "";
        String name = userObj["name"] | "";
        long credit = userObj["credit"] | DEFAULT_CREDIT;
        bool in = userObj["in"] | false;
        
        if (uid.length() > 0 && name.length() > 0) {
            String normalizedUID = normalizeUID(uid);
            if (isValidUID(normalizedUID)) {
                dynamicUsers.emplace_back(normalizedUID, name, credit, in, USER_DYNAMIC);
                syncedCount++;
            }
        }
    }
    
    saveDynamicUsersToNVS();
    Serial.printf("Synced %d users from server\n", syncedCount);
    return true;
}

void populateUsersJson(JsonArray& usersArray) {
    // Add static users
    for (const User& user : staticUsers) {
        JsonObject userObj = usersArray.createNestedObject();
        userObj["uid"] = user.uid;
        userObj["name"] = user.name;
        userObj["credit"] = user.credit;
        userObj["in"] = user.in;
        userObj["type"] = "STATIC";
    }
    
    // Add dynamic users
    for (const User& user : dynamicUsers) {
        JsonObject userObj = usersArray.createNestedObject();
        userObj["uid"] = user.uid;
        userObj["name"] = user.name;
        userObj["credit"] = user.credit;
        userObj["in"] = user.in;
        userObj["type"] = "DYNAMIC";
    }
}

// ================== Authentication and Credit Functions ==================
bool hasValidCredit(const User& user, long requiredAmount) {
    return user.credit >= requiredAmount;
}

bool deductCredit(User& user, long amount) {
    if (user.credit >= amount) {
        user.credit -= amount;
        return true;
    }
    return false;
}

bool addCredit(User& user, long amount) {
    user.credit += amount;
    return true;
}

// ================== Utility Functions ==================
String formatCredit(long credit) {
    return String(credit) + " VND";
}

String getUserStatusString(const User& user) {
    return user.name + " (" + user.uid + ") " + 
           (user.in ? "IN" : "OUT") + " " + formatCredit(user.credit);
}

void printUserList() {
    Serial.println("\n=== USER LIST ===");
    Serial.printf("Static Users (%d):\n", staticUsers.size());
    for (const User& user : staticUsers) {
        Serial.println("  " + getUserStatusString(user));
    }
    
    Serial.printf("Dynamic Users (%d):\n", dynamicUsers.size());
    for (const User& user : dynamicUsers) {
        Serial.println("  " + getUserStatusString(user));
    }
    Serial.println("=================\n");
}

// ================== Legacy Compatibility Functions ==================
int findStaticIndex(const String& uid) {
    // In modular version, no static users - return -1
    return -1;
}

int findDynamicIndex(const String& uid) {
    for (size_t i = 0; i < dynamicUsers.size(); i++) {
        if (dynamicUsers[i].uid == uid) {
            return (int)i;
        }
    }
    return -1;
}

int findUserIndexCombined(const String& uid) {
    int staticIdx = findStaticIndex(uid);
    if (staticIdx >= 0) return staticIdx;
    
    int dynamicIdx = findDynamicIndex(uid);
    if (dynamicIdx >= 0) return (int)(STATIC_COUNT + dynamicIdx);
    
    return -1;
}

bool uidExistsExcept(const String& uid, int exceptIdx) {
    // Check static users (none in modular version)
    for (size_t i = 0; i < STATIC_COUNT; i++) {
        if ((int)i == exceptIdx) continue;
        // No static users to check
    }
    
    // Check dynamic users
    for (size_t j = 0; j < dynamicUsers.size(); j++) {
        int combinedIdx = STATIC_COUNT + (int)j;
        if (combinedIdx == exceptIdx) continue;
        if (dynamicUsers[j].uid == uid) return true;
    }
    
    return false;
}