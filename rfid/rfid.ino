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

/*
  Instead of a Relay you may want to use a servo. Servos can lock and unlock door locks too
  Relay will be used by default
*/

#include <Servo.h>

/*
  For visualizing whats going on hardware we need some leds and to control door lock a relay and a wipe button
  (or some other hardware) Used common anode led,digitalWriting HIGH turns OFF led Mind that if you are going
  to use common cathode led or just seperate leds, simply comment out #define COMMON_ANODE,
*/

#define PIN_LOCK 3     // Set Relay Pin
#define PIN_WIPE 2     // Button pin for WipeMode
#define PIN_BUZZER 5

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
  pinMode(PIN_BUZZER,OUTPUT);
  pinMode(4,OUTPUT);
  digitalWrite(4,LOW);
  //Be careful how relay circuit behave on while resetting or power-cycling your Arduino


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

  //Wipe Code - If the Button (wipeB) Pressed while setup run (powered on) it wipes EEPROM
  if (digitalRead(PIN_WIPE) == LOW) {  // when button pressed pin should get low, button connected to ground
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    tone(PIN_BUZZER, 1000); // Buzzer stays on to inform user we are going to wipe
    //TODO: tone(PIN_BUZZER, 1000, 10000);
    delay(10000);                         // Give user enough time to cancel operation
    noTone(PIN_BUZZER);
    if (digitalRead(PIN_WIPE) == LOW) {    // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      boolean toneActive = false;
      for (uint8_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        // Buzzer stays on to inform user we are going to wipe
        if (toneActive) {
          noTone(PIN_BUZZER);
          toneActive = false;
        } else {
          tone(PIN_BUZZER, 1000);
          toneActive = false;
        }

        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      oneTone();
    }
    else {
      Serial.println(F("Wiping Cancelled")); // Show some feedback that the wipe button did not pressed for 15 seconds
      twoTone();
    }
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      cycleTone();
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    Serial.println(F("Master Card Defined"));
    ackProgTone();
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything Ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0

    if (digitalRead(PIN_WIPE) == LOW) { // Check if button is pressed
      // Give some feedback
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      tone(PIN_BUZZER, 1000); // Buzzer stays on to inform user we are going to wipe
      delay(5000);  // Wait 10 seconds to see user still wants to wipe
      noTone(PIN_BUZZER);
      if (digitalRead(PIN_WIPE) == LOW) {
        EEPROM.write(1, 0);                  // Reset Magic Number.
        Serial.println(F("Restart device to re-program Master Card"));
        ackProgTone();
        while (1);
      }
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read

  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Master Program Mode"));
      Serial.println(F("-----------------------------"));
      nakProgTone();
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        if(deleteID(readCard)) {
          nakProgTone();
        } else {
          twoTone();
        }
      } else {                    // If scanned card is not known add it
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
  } else {
    if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      granted();

      Serial.println(F("Hello Master - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan Master again after 2 second delay"));
      delay(2000);

      if (getID()) {
        if ( isMaster(readCard) ) {
          programMode = true;
          Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
          Serial.println(F("Scan Master Card again to Exit Program Mode"));
          Serial.println(F("-----------------------------"));
          cycleTone();

        } else {
          programMode = false;
          Serial.println(F("Normal Card Scanned"));
          Serial.println(F("Exiting Master Program Mode"));
          Serial.println(F("-----------------------------"));
          nakProgTone();
        }

      } else {
          Serial.println(F("No Card Scanned"));
          Serial.println(F("Exiting Master Program Mode"));
          Serial.println(F("-----------------------------"));
      }
      return;
    }

    if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
      Serial.println(F("Welcome, You shall pass"));
      granted();         // Open the door lock for 300 ms
    }
    else {      // If not, show that the ID was not valid
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
    myservo.write(90);              // tell servo to go to position in variable 'pos'
    delay(1500);                       // waits 15ms for the servo to reach the position
    openDoor = false;
  } else {
    myservo.write(5);              // tell servo to go to position in variable 'pos'
    delay(1500);                       // waits 15ms for the servo to reach the position
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
