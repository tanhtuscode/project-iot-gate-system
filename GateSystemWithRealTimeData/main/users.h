#ifndef USERS_H
#define USERS_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <MFRC522.h>

// ================== User Data Structures ==================
enum UserType : uint8_t {
    USER_STATIC = 0,
    USER_DYNAMIC = 1
};

struct User {
    String uid;
    String name;
    long credit;
    bool in;        // true = IN, false = OUT
    UserType type;
    
    User() : uid(""), name(""), credit(100000), in(false), type(USER_DYNAMIC) {}
    User(const String& u, const String& n, long c, bool i, UserType t) 
        : uid(u), name(n), credit(c), in(i), type(t) {}
};

struct LastScanResult {
    String uid;
    uint32_t timestamp;
    bool isNew;
    
    LastScanResult() : uid(""), timestamp(0), isNew(false) {}
};

// ================== User Management State ==================
extern std::vector<User> staticUsers;
extern std::vector<User> dynamicUsers;
extern bool inputModeActive;
extern LastScanResult lastScan;
extern Preferences userPrefs;

// ================== Constants ==================
extern const long COST_PER_EXIT;
extern const long DEFAULT_CREDIT;
extern const size_t STATIC_COUNT;

// ================== Access Result Enums ==================
enum AccessStatus {
    ACCESS_GRANTED,
    ACCESS_DENIED_INSUFFICIENT_CREDIT,
    ACCESS_DENIED_UNKNOWN_CARD,
    ACCESS_DENIED_ERROR
};

struct AccessResult {
    AccessStatus status;
    String userName;
    long newCredit;
    
    AccessResult() : status(ACCESS_DENIED_ERROR), userName(""), newCredit(0) {}
    AccessResult(AccessStatus s, const String& name, long credit) 
        : status(s), userName(name), newCredit(credit) {}
};

// ================== User Management Functions ==================
bool initializeUsers();
void loadUsersFromNVS();
void saveUsersToBothNVS();
void saveDynamicUsersToNVS();
void saveStaticUsersToNVS();

// ================== User Query Functions ==================
int findUserByUID(const String& uid);
User* getUserByUID(const String& uid);
User* getUserByIndex(int index);
int getTotalUserCount();
int getStaticUserCount();
int getDynamicUserCount();
int findStaticIndex(const String& uid);
int findDynamicIndex(const String& uid);
int findUserIndexCombined(const String& uid);
bool uidExistsExcept(const String& uid, int exceptIdx);

// ================== User CRUD Functions ==================
bool addUser(const String& uid, const String& name, long credit = DEFAULT_CREDIT, UserType type = USER_DYNAMIC);
bool updateUser(const String& uid, const String& name = "", long credit = -1, bool in = false);
bool deleteUser(const String& uid);
void clearDynamicUsers();

// ================== RFID Processing Functions ==================
String uidToHex(const MFRC522::Uid& uid);
String normalizeUID(const String& uid);
bool isValidUID(const String& uid);

// ================== Card Processing Functions ==================
String readRFIDCard();
bool isCardPresent();
AccessResult processCardAccess(const String& uid);
bool processCardScan(const String& uid);
bool checkAccess(const User& user, bool isEntry);
void updateUserState(User& user, bool isEntry, long cost = 0);
void logCardScan(const String& uid);
void addEventLog(const String& message);

// ================== Input Mode Functions ==================
void setInputModeActive(bool active);
bool isInputModeActive();
void setLastScan(const String& uid, bool isNew);
LastScanResult getLastScan();
void clearLastScan();

// ================== Server Sync Functions ==================
bool syncUsersFromJson(const DynamicJsonDocument& doc);
void populateUsersJson(JsonArray& usersArray);
bool syncUserToServer(const User& user);

// ================== Authentication and Credit Functions ==================
bool hasValidCredit(const User& user, long requiredAmount);
bool deductCredit(User& user, long amount);
bool addCredit(User& user, long amount);

// ================== Utility Functions ==================
String formatCredit(long credit);
String getUserStatusString(const User& user);
void printUserList();

#endif // USERS_H