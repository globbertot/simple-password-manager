#include <iostream>
#include <random>
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <cstring>
#include "libraries/sql/sqlite3.h"
#include "libraries/openssl/aes.h"
#include "libraries/openssl/rand.h"

const std::string dbName = "data.db";
const int FLAG_PASSWORD_FOUND = 0;
const int FLAG_SERVICE_EXISTS = 1;
const int FLAG_PASSWORD_FOUND_ENCRYPTED = 2;
const int MIN_PASSWORD_LENGTH = 8;
const int MAX_PASSWORD_LENGTH = 18;
unsigned char key[EVP_MAX_IV_LENGTH];

// Helper function for generating the encryption key
// PARAMATERS:
/*
TYPE: UNSIGNED CHAR || NAME: key || USAGE: The key variable we will be generating
*/
void generateEncryptionKey(unsigned char* key) {
    if (RAND_bytes(key, EVP_MAX_IV_LENGTH) != 1){
        std::cout << "KEY GENERATION FAILED";exit(-1);
    }
}

// Helper function for searching the database 
// PARAMATERS:
/*
TYPE: UNSIGNED CHAR || NAME: key || USAGE: The key variable we will be initiating
*/
bool setupKey(unsigned char* key){
    sqlite3* db;
    int rc = sqlite3_open(dbName.c_str(), &db);if (rc) { return false; }
    std::string sqlCode = "SELECT key FROM lost;";
    sqlite3_stmt* stmt;
    // We initiate and open the database and create the sql code and stmt //
    
    rc = sqlite3_prepare_v2(db, sqlCode.c_str(), -1, &stmt, 0);if (rc) { sqlite3_close(db); return false; }

    rc = sqlite3_step(stmt);
    // We run the sql code and if we find a result..
    if (rc == SQLITE_ROW) {
        const char* k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (k) { memcpy(key, k, EVP_MAX_IV_LENGTH); }// We extract it and copy it at our key variable //
    } else {
        // Otherwise we generate the key, and add it to the database
        generateEncryptionKey(key);
        std::string sqlCode2 = "INSERT INTO lost (key) VALUES (?);";
        sqlite3_stmt* stmt2;
        
        rc = sqlite3_prepare_v2(db, sqlCode2.c_str(), -1, &stmt2, 0);if (rc) { sqlite3_close(db);return false; }

        const char* keyChar = reinterpret_cast<const char*>(key);
        rc = sqlite3_bind_text(stmt2, 1, keyChar, -1, SQLITE_STATIC);if (rc) { sqlite3_finalize(stmt2);sqlite3_close(db);return false; }

        rc = sqlite3_step(stmt2);if (rc != SQLITE_DONE) { sqlite3_finalize(stmt2);sqlite3_close(db);return false; }

        sqlite3_finalize(stmt2);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
}

// Helper function for encrypting each password
// PARAMATERS:
/*
TYPE: UNSIGNED CHAR || NAME: text || USAGE: The text itself
TYPE: int || NAME: text_len || USAGE: The length of the text
TYPE: UNSIGNED CHAR || NAME: key || USAGE: The encryption key itself
TYPE: UNSIGNED CHAR || NAME: chipher || USAGE: The chipher variable we will store our encrypted text
*/
int encrypt(unsigned char* text, int text_len, unsigned char* key, unsigned char* chipher){
    int cL = 0;int L = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { std::cout<<"CTX - ERROR";exit(-1); }
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL)) { std::cout<<"INITIATE ENCRYPT - ERROR";exit(-1); }
    if (!EVP_EncryptUpdate(ctx, chipher, &L, text, text_len)) { std::cout<<"UPDATING ENCRYPT - ERROR";exit(-1); }
    cL += L;
    if (!EVP_EncryptFinal_ex(ctx, chipher + L, &L)) { std::cout<<"FAILED TO ENCRYPT - ERROR";exit(-1); }
    cL += L;
    EVP_CIPHER_CTX_free(ctx);
    return cL;
}

// Helper function for decrypting each password
// PARAMATERS:
/*
TYPE: UNSIGNED CHAR || NAME: chipher || USAGE: The encrypted text
TYPE: int || NAME: cipher_len || USAGE: The length of the encrypted text
TYPE: UNSIGNED CHAR || NAME: key || USAGE: The encryption key itself
TYPE: UNSIGNED CHAR || NAME: text || USAGE: The text variable we will store our decrypted text
*/
int decrypt(unsigned char* chipher, int cipher_len, unsigned char* key, unsigned char* text){
    int tL = 0;int L = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { std::cout<<"CTX - ERROR";exit(-1); }
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL)) { std::cout<<"INITIATE DECRYPT - ERROR";exit(-1); }
    if (!EVP_DecryptUpdate(ctx, text, &L, chipher, cipher_len)) { std::cout<<"UPDATING DECRYPT - ERROR";exit(-1); }
    tL += L;
    if (!EVP_DecryptFinal_ex(ctx, text + L, &L)) { std::cout<<"FAILED TO DECRYPT - ERROR";exit(-1); }
    tL += L;
    EVP_CIPHER_CTX_free(ctx);
    return tL;
}

// Helper function for searching the database 
// PARAMATERS:
/*
TYPE: STRING || NAME: W || USAGE: The name of the service we will be searching
TYPE: INT || NAME: FLAG || USAGE: A Flag to check what our final output will be (if everything goes correctly)
*/
std::string search(const std::string &W, const int &FLAG){
    sqlite3* db;
    int rc = sqlite3_open(dbName.c_str(), &db);
    if (rc || FLAG < FLAG_PASSWORD_FOUND || FLAG >= 3 || W.empty()){ return "FAILED - Flag is not correct / service is empty or there was an error opening the database"; }
    // We initiate the database and check if we can not open the database / flag is not correct / service is empty //
    
    std::string sqlCode = "SELECT password FROM data WHERE service = ?;";
    sqlite3_stmt* stmp;
    rc = sqlite3_prepare_v2(db, sqlCode.c_str(), -1, &stmp, 0);if (rc != SQLITE_OK) { sqlite3_finalize(stmp);sqlite3_close(db);return "FAILED - Preparation"; }
    rc = sqlite3_bind_text(stmp, 1, W.c_str(), -1, SQLITE_STATIC);if (rc != SQLITE_OK) { sqlite3_finalize(stmp);sqlite3_close(db);return "FAILED - Binding"; }
    // We create the sql code, prepare the database, and bind our W //

    rc = sqlite3_step(stmp);
    if (rc == SQLITE_ROW){
        // If we have any data, we grab it in a variable P, and make sure it isnt NULL //
        const char* P = reinterpret_cast<const char*>(sqlite3_column_text(stmp, 0));if (!P) { return "NOSERVICE"; }
        if (FLAG == FLAG_SERVICE_EXISTS) { sqlite3_finalize(stmp);sqlite3_close(db);return "EXISTS"; }
        // We also check if our flag is to print exists, if yes we do as following //

        int L = sqlite3_column_bytes(stmp, 0);
        std::string password(P, L);sqlite3_finalize(stmp);sqlite3_close(db);
        if (FLAG == FLAG_PASSWORD_FOUND_ENCRYPTED) { return password; }
        // We get it's size for memory issues and make it into a string //


        unsigned char decrypted[64];
        int decrypted_len = decrypt((unsigned char*)password.c_str(), strlen(password.c_str()), key, decrypted);
        std::string rtn;
        for (int i = 0; i<decrypted_len;++i){
            rtn.push_back(decrypted[i]);
        }
        return rtn;
        // Finally we return
    } else {
        // If we cant find any data that means that the service did not exist //
        sqlite3_finalize(stmp);
        sqlite3_close(db);
        return "NOSERVICE";
    }
    return "ENDED";
}

// Helper function for creating new password and adding them to the database //
// PARAMATERS:
/*
TYPE: STRING || NAME: W || USAGE: The actual service we will add
TYPE: STRING || NAME: P || USAGE: The actual password we will add
*/
std::string createPass(const std::string W, const std::string P) {
    if (W.empty() || P.empty()) { return "FAILED - Password or service is empty"; }
    std::string rtn = search(W, FLAG_SERVICE_EXISTS);
    // We quickly make sure both of our paramaters are not empty and perform a search to see if the service
    // Already exists //

    if (rtn != "EXISTS"){
        sqlite3* db;
        int rc = sqlite3_open(dbName.c_str(), &db);if (rc) { return "FAILED - Could not open database"; }
        // Like before we initiate the database and check if we can actually open it //

        std::string sqlCode = "INSERT INTO data (service, password) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        // We define our sql code and a prepared sql statement for later usage //

        rc = sqlite3_prepare_v2(db, sqlCode.c_str(), -1, &stmt, 0);if(rc != SQLITE_OK) { sqlite3_close(db); return "FAILED - Preparation"; }
        rc = sqlite3_bind_text(stmt, 1, W.c_str(), -1, SQLITE_STATIC); if (rc != SQLITE_OK) { sqlite3_finalize(stmt);sqlite3_close(db);return "FAILED - Binding"; }
        rc = sqlite3_bind_text(stmt, 2, P.c_str(), -1, SQLITE_STATIC); if (rc != SQLITE_OK) { sqlite3_finalize(stmt);sqlite3_close(db);return "FAILED - Binding"; }

        rc = sqlite3_step(stmt);
        // We start executing the sql code //
        if (rc != SQLITE_DONE){
            // If we are not done, that means an issue must have occured //
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            std::cout << sqlite3_errmsg(db);
            return "FAILED - Issue with sql code";
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return "SUCCESS";
    }
    return rtn;
}

// Helper function for deleting a certain password //
// PARAMATERS:
/*
TYPE: STRING || NAME: W || USAGE: The actual service we will delete
*/
std::string deletePass(const std::string& W){
    if (W.empty()) { return "FAILED - SERVICE EMPTY"; }
    std::string exists = search(W, FLAG_PASSWORD_FOUND_ENCRYPTED);
    // We grab the password

    if (exists != "NOSERVICE" || exists != "FAILED"){
        sqlite3* db;
        int rc = sqlite3_open(dbName.c_str(), &db); if (rc) { return "FAILED - Couldnt open the database"; }
        
        std::string sqlCode = "DELETE FROM data WHERE password = ?;";
        sqlite3_stmt* stmt;
        // If the the password exists, we initiate the database, the sql code and the stmt //

        rc = sqlite3_prepare(db, sqlCode.c_str(), -1, &stmt, 0); if (rc != SQLITE_OK) { sqlite3_finalize(stmt);sqlite3_close(db);return "FAILED - Preparation error"; };
        rc = sqlite3_bind_text(stmt, 1, exists.c_str(), -1, SQLITE_STATIC); if (rc != SQLITE_OK) { sqlite3_finalize(stmt);sqlite3_close(db);return "FAILED - Binding"; }
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE){
            // If its not done, that means there was an error with the sql code //
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            std::cout << "\n\n\n" << sqlite3_errmsg(db) << "\n\n\n";
            return "FAILED - Issue with sql code";
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return "SUCCESS";
    }
    return "FAILED";
}

// Helper function to show all the services //
void showAllServices(){
    sqlite3* db;
    int rc = sqlite3_open(dbName.c_str(), &db);
    std::vector<std::string> services;
    // Open the database and create an empty vector to store the services //

    std::string sqlCode = "SELECT service FROM data;";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare(db, sqlCode.c_str(), -1, &stmt, 0);
    // Prepare the database, create the stmt and the sql code //

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        // While we are looping through the data... //
        const char* W = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int L = sqlite3_column_bytes(stmt, 0);
        std::string service(W, L);
        services.push_back(service);
        // Add each service onto that vector //
    }
    if (rc != SQLITE_DONE) { std::cout << sqlite3_errmsg(db);return; }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    for (std::string item : services){
        std::cout << "SERVICE: " << item << "\n";
    }
    // Print all the services //
}

// Helper function to check whether X password is valid
/*
PASSWORD POLICY:
    - Must contain less than 18 characters and at least 8
    - Must contain at least 1 of each uppercase and lowercase letters
    - Must contain at least 1 symbols, making sure that each symbol cant exist more than four times
    - Must contain at least 1 numbers
    - Cant contain spaces
PARAMATERS:
TYPE: STRING || NAME: P || USAGE: The actual password we will check
*/
bool passCheck(std::string P){
    if ((int)P.size() > MAX_PASSWORD_LENGTH || (int)P.size() < MIN_PASSWORD_LENGTH) { std::cout << "Your password is either too small or too long\n";return false; } 
    
    int lowercaseCount = 0;
    int uppercaseCount = 0;
    int numbersCount = 0;
    std::unordered_set<char> symbols;
    // Loop through each character //
    for (char C : P){
        if (C == ' ') { std::cout << "\nYour password must not contain spaces\n";return false; }
        if (std::isupper(C)) { uppercaseCount++; } else if (std::islower(C)) { lowercaseCount++; } // Lower/Upper case checking //
        else if (std::isdigit(C)) { numbersCount++; } // Checking if the character is a digit //
        else {
            if (symbols.count(C) > 4) { std::cout << "Your password must not contain the same symbol four times\n";return false; }
            symbols.insert(C);
        }
    }
    if (lowercaseCount < 1){ std::cout << "Your password must have at least 1 lowercase letters\n";return false; }
    if (uppercaseCount < 1){ std::cout << "Your password must have at least 1 uppercase letters\n";return false; }
    if (numbersCount < 1){ std::cout << "Your password must have at least 1 numbers\n";return false; }
    if ((int)symbols.size() < 1) { std::cout << "Your password must contain at least 1 symbols\n";return false; } 

    return true;
}

// Helper function to generate an password //
std::string generatePassword(){
    std::string P = "";
    std::random_device rd;std::mt19937 gen(rd());

    std::string alphabet = "abcdefghijklmnopqrstuvwxyz";
    std::string numbers = "0123456789";
    std::string symbols = "!@#$%&*";

    int length = std::uniform_int_distribution<int>(MIN_PASSWORD_LENGTH, MAX_PASSWORD_LENGTH)(gen);
    // Initiating everything //

    for (int i=0;i<length;++i){ // We loop for random times between 8 and 18 (max/min password length)
        
        int category = gen() % 3; // 0 for letter | 1 for number | 2 for symbol
        
        if (category == 0) { // Add a letter
            char letter = alphabet[gen() % alphabet.size()];if (gen() % 2 == 1) { letter = std::toupper(letter); }
            P += letter;
        } else if (category == 1) { // Add a number
            char number = numbers[gen() % numbers.size()];
            P += number;
        } else { // Add a symbol
            char symbol = symbols[gen() % symbols.size()];
            P += symbol;
        }
    }
    if (!passCheck(P)){
        // If we fail to pass the password check, recall this function //
        P = generatePassword();
        system("cls");
    }
    return P;
}

// Main Function //
int main(){
    sqlite3* db;
    int rc = sqlite3_open(dbName.c_str(), &db);if (rc) { return -1; }
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS lost (key TEXT)", 0, 0, 0);
    if (!setupKey(key)) { std::cout << "COULD NOT INITIATE KEY";exit(-1); }
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS data (service TEXT, password TEXT);", 0, 0, 0);sqlite3_close(db);

    // Open the database and execute the standard sql code if its the first time //
    // As well as initiating the encryption key //

    while (true) {
        char cmd = 'N';
        std::cout << "\n\n\n--------------SIMPLE PASSWORD MANAGER--------------\n\n\n\n";
        std::cout << "| COMMANDS: W - Creates a new password | R - Reads X password | D - Deletes X password | L - Lists all the services\n| MORE?\n";
        std::cout << "ENTER YOUR COMMAND:  ";std::cin >> cmd;std::cin.ignore();
        // Print everything, grab input of command //

        if (std::tolower(cmd) == 'w'){
            // W = Create new password //
            system("cls");
            
            std::string W, P = "";
            char choice = 'y';
            std::cout << "(recommended) Would you like to generate a password automaticly? (y or n): ";std::cin >> choice;std::cin.ignore();
            if (choice == 'y'){
                P = generatePassword();
                std::cout << "Generated password is: " << P << "   .Would you like to keep it? (y or n): ";std::cin >> choice;std::cin.ignore();
                if (choice != 'y') { P = "";std::cout << "FINE, MAKE ONE ON YOUR OWN.\n"; }
            }
            std::cout << "Enter the service for the password: ";std::getline(std::cin, W);std::cout << "\nEnter the password now: ";if (P.empty()) { std::cin>>P; }std::transform(W.begin(), W.end(), W.begin(), ::tolower);
            
            if (!passCheck(P)) { continue; }

            unsigned char* password = (unsigned char*)P.c_str();

            int textLength = strlen((const char*)password);
            unsigned char cipher[64];

            int cipher_len = encrypt(password, textLength, key, cipher);       
            P.clear();
            for (int i=0; i<cipher_len; ++i){
                P.push_back(cipher[i]);
            }// We encrypt the password before adding it into the database for efficiency //
            if (P.empty()) { std::cout << "ERROR - ENCRYPTING PASSWORD";continue; }

            std::cout << createPass(W, P) << "\n";
        } else if (std::tolower(cmd) == 'r'){
            // R = Read X Password //            
            system("cls");
            std::string W = "";std::cout << "Enter the service for the password: ";std::getline(std::cin, W);std::transform(W.begin(), W.end(), W.begin(), ::tolower);
            std::cout << "PASSWORD: " << search(W, FLAG_PASSWORD_FOUND);
        } else if (std::tolower(cmd) == 'd') {
            // D = Delete X Password //
            system("cls");
            std::string W = "";
            std::cout << "Enter the service for the password: ";std::cin>>W;std::transform(W.begin(), W.end(), W.begin(), ::tolower);
            std::cout << deletePass(W) << "\n";
        } else if (std::tolower(cmd) == 'l') {
            // L = List all services //
            system("cls");
            showAllServices();
        } else {
            break;
            // If our command is not any of the specified, break //
        }
    }
    return 0;
}
