#include "SD.h"
#include "SPI.h"
#include "WiFi.h"
#include "WebServer.h"

// SD Card pins 
#define CS_PIN 5    
#define MOSI_PIN 23
#define MISO_PIN 19  
#define SCK_PIN 18

// WiFi credentials - CHANGE THESE TO YOUR NETWORK
const char* ssid = "dois";
const char* password = "";

// Web server on port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== ESP32 SD Card Web Server ===");
  Serial.println("Initializing SD card...");
  
  // Ensure CS pin is output and high
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  delay(100);
  
  // Initialize SPI with explicit parameters
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  SPI.setFrequency(1000000); // Start with 1MHz (slower speed)
  
  // Try multiple initialization attempts
  // This is most useful when using bad wiring like connecting directly to the card
  bool sdReady = false;
  for(int attempts = 0; attempts < 5; attempts++) {
    Serial.printf("\nAttempt %d: ", attempts + 1);
    
    if (SD.begin(CS_PIN, SPI, 1000000)) { // 1MHz frequency
      sdReady = true;
      Serial.println("SUCCESS!");
      break;
    }
    
    Serial.println("FAILED!");
    delay(1000);
  }
  
  if (!sdReady) {
    Serial.println("SD Card mount failed after 5 attempts!");
    Serial.println("Troubleshooting steps:");
    Serial.println("1. Check all wiring connections");
    Serial.println("2. Try a different SD card (prefer 32GB or less)");
    Serial.println("3. Format SD card as FAT32");
    Serial.println("4. Check power supply (SD needs stable 3.3V)");
    Serial.println("5. Try different jumper wires");
    return 1;
  }
  
  // Print SD card info
  printSDInfo();
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("SUCCESS!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUploadResponse, handleFileUpload);
  server.on("/files", HTTP_GET, handleListFiles);
  server.on("/download", HTTP_GET, handleDownload);
  
  // Start server
  server.begin();
  Serial.println("Web server started!");
  Serial.println("Open your browser and go to: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  delay(10);
}

//got pissed trying to make multi line string to work, asked ai to generate this lol
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head><title>ESP32 SD Card File Manager</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 40px; }";
  html += ".container { max-width: 600px; margin: 0 auto; }";
  html += ".upload-area { border: 2px dashed #ccc; padding: 20px; text-align: center; margin: 20px 0; border-radius: 10px; }";
  html += ".file-list { margin: 20px 0; }";
  html += ".file-item { padding: 10px; border: 1px solid #ddd; margin: 5px 0; display: flex; justify-content: space-between; align-items: center; }";
  html += "button { padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += "button:hover { background: #0056b3; }";
  html += ".small-btn { padding: 5px 10px; font-size: 12px; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 SD Card File Manager</h1>";
  html += "<div class='upload-area'>";
  html += "<h3>Upload Files to SD Card</h3>";
  html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
  html += "<input type='file' name='file' multiple>";
  html += "<br><br><button type='submit'>Upload Files</button>";
  html += "</form></div>";
  html += "<div class='file-list'>";
  html += "<h3>Files on SD Card</h3>";
  html += "<button onclick='location.reload()'>Refresh File List</button>";
  html += "<div id='files'></div>";
  html += "</div></div>";
  html += "<script>";
  html += "fetch('/files').then(response => response.text()).then(data => {";
  html += "document.getElementById('files').innerHTML = data;";
  html += "});";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    Serial.printf("Upload started: %s\n", filename.c_str());
    
    // Remove file if it exists
    if (SD.exists(filename)) {
      SD.remove(filename);
    }
    
    // Open file for writing
    uploadFile = SD.open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for writing");
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write data to SD card
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      Serial.printf("Writing %d bytes...\n", upload.currentSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("Upload completed: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
    }
  }
}

void handleUploadResponse() {
  String html = "<!DOCTYPE html>";
  html += "<html><head><title>Upload Complete</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 40px; text-align: center; }";
  html += ".success { color: green; font-size: 18px; margin: 20px; }";
  html += "button { padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += "</style></head><body>";
  html += "<h1>Upload Complete!</h1>";
  html += "<div class='success'>File successfully uploaded to SD card</div>";
  html += "<button onclick=\"window.location.href='/'\">Back to File Manager</button>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleListFiles() {
  String fileList = "";
  File root = SD.open("/");
  
  if (!root) {
    fileList = "<p>Failed to open root directory</p>";
  } else {
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;
      
      if (!entry.isDirectory()) {
        String filename = entry.name();
        int fileSize = entry.size();
        
        fileList += "<div class='file-item'>";
        fileList += "<span>" + filename + " (" + String(fileSize) + " bytes)</span>";
        fileList += "<button class='small-btn' onclick=\"window.open('/download?file=" + filename + "', '_blank')\">Download</button>";
        fileList += "</div>";
      }
      entry.close();
    }
    root.close();
    
    if (fileList == "") {
      fileList = "<p>No files found on SD card</p>";
    }
  }
  
  server.send(200, "text/html", fileList);
}

void handleDownload() {
  String filename = server.arg("file");
  if (filename == "") {
    server.send(400, "text/plain", "No file specified");
    return;
  }
  
  String filepath = "/" + filename;
  if (!SD.exists(filepath)) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  
  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  
  server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
  server.streamFile(file, "application/octet-stream");
  file.close();
}

void printSDInfo() {
  Serial.println("\n--- SD Card Information ---");
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}
