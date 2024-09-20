#include <SPI.h>
#include <RF24.h>

RF24 radio(5, 4); // Assuming you connect NRF24L01 CE pin to GPIO 5 and CSN/SS pin to GPIO 18

byte addresses[][6] = {"1Node", "2Node"};

void setup() {
  Serial.begin(9600);

  // Initialize SPI bus
  SPI.begin();

  // Initialize NRF24L01 radio
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);

  // Set the address for writing and reading
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);

  // Start listening for incoming data
  radio.startListening();

  Serial.println("Setup complete");
}

void loop() {
  // Check for incoming data
  if (radio.available()) {
    // Read the incoming message
    char receivedMessage[32] = "ffffffffffff";
    radio.read(&receivedMessage, sizeof(receivedMessage));

    // Print the received message
    Serial.print("Received message: ");
    Serial.println(receivedMessage);
  }else{
    Serial.println("not receivedMessage");
  }

  // Optionally, you can add code here to send data
}