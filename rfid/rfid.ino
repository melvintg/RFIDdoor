/*
 * This code is based on an example of a MFRC522 library example;
 * Added some tone functions and other modifications.
 * for further details and other examples see: https://github.com/miguelbalboa/rfid
 * 
 * 
 * --------------------------------------------------------------------------------------------------------------------
 * Example sketch/program showing An Arduino Door Access Control featuring RFID, EEPROM, Relay
 * --------------------------------------------------------------------------------------------------------------------
 *
 * This example showing a complete Door Access Control System

 Simple Work Flow (not limited to) :
                                     +---------+
+----------------------------------->READ TAGS+^------------------------------------------+
|                              +--------------------+                                     |
|                              |                    |                                     |
|                              |                    |                                     |
|                         +----v-----+        +-----v----+                                |
|                         |MASTER TAG|        |OTHER TAGS|                                |
|                         +--+-------+        ++-------------+                            |
|                            |                 |             |                            |
|                            |                 |             |                            |
|                      +-----v---+        +----v----+   +----v------+                     |
|         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
|         |            +-+-------+   |    +-----------+ +------------------+              |
|         |              |           |                |                    |              |
|    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
|    |MASTER TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
|    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
|                       |               |                 |               |               |
|       +----+     +----v------+     +--v---+             |               +--------------->
+-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
        +----+     |  EEPROM   |     |EEPROM|             |                               |
                   +-----------+     +------+             +-------------------------------+

 *
 * Use a Master Card which is act as Programmer then you can able to choose card holders who will granted access or not
 *
 * **Easy User Interface**
 *
 * Just one RFID tag needed whether Delete or Add Tags. You can choose to use Leds for output or Serial LCD module to inform users.
 *
 * **Stores Information on EEPROM**
 *
 * Information stored on non volatile Arduino's EEPROM memory to preserve Users' tag and Master Card. No Information lost
 * if power lost. EEPROM has unlimited Read cycle but roughly 100,000 limited Write cycle.
 *
 * **Security**
 * To keep it simple we are going to use Tag's Unique IDs. It's simple and not hacker proof.
 *
 * @license Released into the public domain.
 *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 */

#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

#include <Servo.h>

#define PIN_LOCK 3     // Pin for lock servo
#define PIN_WIPE 2     // Button pin for WipeMode
#define PIN_BUZZER 5   // Pin for tones notifications

Servo myservo;  

boolean match = false;          // initialize card match to false
boolean programMode = false;
boolean openDoor = true;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

// Create MFRC522 instance.
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //Arduino Pin Configuration
  pinMode(PIN_WIPE, INPUT_PULLUP);   // Enable pin's pull up resistor
  pinMode(PIN_BUZZER, OUTPUT);
  
  // GND pin
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  myservo.attach(PIN_LOCK); 
  myservo.write(5);
  delay(1000);
  myservo.detach();

  //Protocol Configuration
  Serial.begin(9600);  // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware

  //If you set Antenna Gain to Max it will increase reading distance
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println(F("Access Control Example v0.1"));   // For debugging purposes
  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details


  // *************************************************** Wipe Function *******************************************************
  //Wipe Code - If the Button (wipeB) Pressed while setup run (powered on) it wipes EEPROM
  // Procedure: 
  // - Wipe pin to Low.
  // - Tone of 1 sec. to inform.
  // - Wait 10 sec.
  // - If Wipe pin still to Low
  // -----> Delete All EEPROM
  // - End tone of one or two tone to notify result.

  // when button pressed pin should get low, button connected to ground
  if (digitalRead(PIN_WIPE) == LOW) { 
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    // Buzzer stays on to inform user we are going to wipe
    tone(PIN_BUZZER, 1000); 
    //TODO: tone(PIN_BUZZER, 1000, 10000);
    // Give user enough time to cancel operation
    delay(10000);                         
    noTone(PIN_BUZZER);
    // If button is still pressed, wipe EEPROM
    if (digitalRead(PIN_WIPE) == LOW) {
      Serial.println(F("Starting Wiping EEPROM"));
      boolean toneActive = false;
      //Loop end of EEPROM address
      for (uint8_t x = 0; x < EEPROM.length(); x = x + 1) {
        // Buzzer stays on to inform user we are going to wipe
        if (toneActive) {
          noTone(PIN_BUZZER);
          toneActive = false;
        } else {
          tone(PIN_BUZZER, 1000);
          toneActive = false;
        }

        //If EEPROM address 0
        if (EEPROM.read(x) == 0) {              
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          // if not write 0 to clear, it takes 3.3mS
          EEPROM.write(x, 0);       
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      oneTone();

    } else {
      // If button is not still pressed, wipe EEPROM operation cancelled.
      Serial.println(F("Wiping Cancelled"));
      twoTone();
    }
  }


  // *************************************************** Check Master UID *******************************************************
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'

  // Procedure: 
  // - If we don't have magic number, i.e. we don't have registered any Master UID.
  // - Play cycleTone. 
  // - If we get some UID we end program with an ackTone.
  
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      cycleTone();
      successRead = getID();            
    }
    while (!successRead);
                      
    for ( uint8_t j = 0; j < 4; j++ ) { 
      // Write scanned PICC's UID to EEPROM, start from address 3
      EEPROM.write( 2 + j, readCard[j] );  
    }
    // Write to EEPROM we defined Master Card. Add flac of Magic Number.
    EEPROM.write(1, 143);                  
    Serial.println(F("Master Card Defined"));
    ackProgTone();
  }

  // *************************************************** Store Master UID from EEPROM to RAM memory *******************************************************
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {       
    masterCard[i] = EEPROM.read(2 + i);
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything Ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {

  // *************************************************** Read RFID UID Card *******************************************************
  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0

    // *************************************************** Wipe Only Master UID Function *******************************************************
    if (digitalRead(PIN_WIPE) == LOW) { // Check if button is pressed
      // Give some feedback
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 5 seconds"));
      // Buzzer stays on to inform user we are going to wipe
      tone(PIN_BUZZER, 1000); 
      // Wait 5 seconds to see user still wants to wipe
      delay(5000);  
      noTone(PIN_BUZZER);
      if (digitalRead(PIN_WIPE) == LOW) {
        // Reset Magic Number.
        EEPROM.write(1, 0);                  
        Serial.println(F("Restart device to re-program Master Card"));
        ackProgTone();
        while (1);
      }
    }
  }
  //the program will not go further while you are not getting a successful read
  while (!successRead);   

  // If the execution is in program Mode (i.e. previously, master card was detected and enter to program mode, so this loop is for new card)
  if (programMode) {
    // Check first if master card scanned again to exit program mode
    if (isMaster(readCard) ) { 
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Master Program Mode"));
      Serial.println(F("-----------------------------"));
      nakProgTone();
    }
    else {
       // If scanned card is known delete it
      if (findID(readCard) ) {
        Serial.println(F("I know this PICC, removing..."));
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        if(deleteID(readCard)) {
          nakProgTone();
        } else {
          twoTone();
        }
        
      // If scanned card is not known add it
      } else {                    
        Serial.println(F("I do not know this PICC, adding..."));
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        if (writeID(readCard)) {
          ackProgTone();
        } else {
          twoTone();
        }
      }
    }
    programMode = false;

  // Normal Loop Operation
  } else {
    // If scanned card's ID matches Master Card's ID - enter program mode
    if (isMaster(readCard)) {
      // Open Door because is Master Card and after, enter to program mode.
      granted();

      Serial.println(F("Hello Master - Entered Program Mode"));
      // Read the first Byte of EEPROM that stores the number of ID's in EEPROM
      uint8_t count = EEPROM.read(0);   
      Serial.print(F("I have "));
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan Master again after 2 second delay"));
      delay(2000);

      // If we detect again a Card in the RFID field
      if (getID()) {
        // And this card is Master Card. Enter Program Mode.
        if (isMaster(readCard) ) {
          programMode = true;
          Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
          Serial.println(F("Scan Master Card again to Exit Program Mode"));
          Serial.println(F("-----------------------------"));
          cycleTone();

        // Normal Card is detected. Exit.
        } else {
          programMode = false;
          Serial.println(F("Normal Card Scanned"));
          Serial.println(F("Exiting Master Program Mode"));
          Serial.println(F("-----------------------------"));
          nakProgTone();
        }

      // No card detected in this 2 seconds. Exit Program Mode.
      } else {
          Serial.println(F("No Card Scanned"));
          Serial.println(F("Exiting Master Program Mode"));
          Serial.println(F("-----------------------------"));
      }

      // Return because we detected some card after this 2 second delay.
      return;
    }

    if (findID(readCard)) {
      Serial.println(F("Welcome, You shall pass"));
      granted();
    }
    else {
      Serial.println(F("You shall not pass"));
      denied();
    }
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted () {
  oneTone();
  if (!myservo.attached()) {
    myservo.attach(PIN_LOCK); 
  }
  
  if(openDoor) {
    myservo.write(90);              
    delay(1500);
    openDoor = false;
  } else {
    myservo.write(5);
    delay(1500);
    openDoor = true;
  }
  if (myservo.attached()) {
    myservo.detach();
  }
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  twoTone();
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    while (true); // do not go further
  }
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
boolean writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    Serial.println(F("Succesfully added ID record to EEPROM"));
    return true;
  }
  else {
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
    return false;
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
boolean deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
    return false;
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    Serial.println(F("Succesfully removed ID record from EEPROM"));
    return true;
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}


////////////////////// Tone configurations  ///////////////////////////////////
void oneTone() {
  tone(PIN_BUZZER, 1000);
  delay(700);
  noTone(PIN_BUZZER);
}

void twoTone() {
  tone(PIN_BUZZER, 1000);
  delay(100);
  noTone(PIN_BUZZER);
  delay(50);
  tone(PIN_BUZZER, 1000);
  delay(700);
  noTone(PIN_BUZZER);
}

void cycleTone() {
  for(int i=0; i<5; i++) {
    tone(PIN_BUZZER, 1000);
    delay(100);
    noTone(PIN_BUZZER);
  }
}

void ackProgTone() {
  cycleTone();
  oneTone();
}

void nakProgTone() {
  cycleTone();
  twoTone();
}
