String makeApSsid() {
  uint64_t chipId = ESP.getEfuseMac();
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "BREmote_Rx_%012llX", chipId);
  return String(ssid);
}

String urlEncode(const String& s) {
  String out;
  char hex[4];
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') out += char(c);
    else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      out += hex;
    }
  }
  return out;
}

String htmlEscape(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

void handleDownload() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing file name");
    return;
  }

  String filename = server.arg("name");
  if (!filename.startsWith("/")) filename = "/" + filename;

  File file = SPIFFS.open(filename, "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  String shortName = filename;
  int slashPos = shortName.lastIndexOf('/');
  if (slashPos >= 0) shortName = shortName.substring(slashPos + 1);

  server.sendHeader("Content-Disposition", "attachment; filename=\"" + shortName + "\"");
  server.streamFile(file, "application/octet-stream");
  file.close();
}

void handleLogFilesPage() {
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    server.send(500, "text/plain", "Failed to open SPIFFS root");
    return;
  }

  String html;
  html.reserve(6000);

  html += F(
    "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Available Log Files</title>"
    "<style>"
    "body{font-family:monospace;background:#111;color:#ddd;padding:20px;}"
    "pre{font-family:monospace;font-size:14px;line-height:1.45;}"
    "a{color:#6cf;text-decoration:none;}"
    "a:hover{text-decoration:underline;}"
    "</style></head><body><pre>"
  );

  html += "\n=== Available Log Files ===\n";
  html += "Filename\t\tSize (KB)\tDate\n";
  html += "--------------------------------------------\n";

  File file = root.openNextFile();
  int fileCount = 0;

  while (file) {
    String filename = String(file.name());
    if (filename.endsWith(".log")) {
      size_t fileSize = file.size();

      int dotPos = filename.lastIndexOf('.');
      int slashPos = filename.lastIndexOf('/') + 1;
      String timestampStr = filename.substring(slashPos, dotPos);
      uint32_t timestamp = timestampStr.toInt();

      time_t rawtime = (time_t)timestamp;
      struct tm* timeinfo = gmtime(&rawtime);
      char dateStr[20];

      if (timeinfo != NULL) {
        snprintf(dateStr, sizeof(dateStr), "%02d-%02d-%04d %02d:%02d:%02d",
                 timeinfo->tm_mday,
                 timeinfo->tm_mon + 1,
                 timeinfo->tm_year + 1900,
                 timeinfo->tm_hour,
                 timeinfo->tm_min,
                 timeinfo->tm_sec);
      } else {
        strcpy(dateStr, "Invalid date");
      }

      html += "<a href='/download?name=" + urlEncode(filename) + "'>";
      html += htmlEscape(filename);
      html += "</a>\t";
      html += String(fileSize / 1024.0, 2);
      html += "\t";
      html += dateStr;
      html += "\n";

      fileCount++;
    }
    file = root.openNextFile();
  }

  html += "\nTotal log files: " + String(fileCount) + "\n";
  html += "Free space: " + String((SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1024) + " KB\n";
  html += "</pre></body></html>";

  server.send(200, "text/html", html);
}

bool routesRegistered = false;

void registerRoutesOnce() {
  if (routesRegistered) return;

  server.on("/logs", HTTP_GET, handleLogFilesPage);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Location", "/logs");
    server.send(302, "text/plain", "");
  });

  routesRegistered = true;
}

void enableWebUi() {
  if (webUiEnabled) return;

  String ssid = makeApSsid();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), nullptr);   // open AP; use password if you want
  // optional fixed AP IP:
  // WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  registerRoutesOnce();

  server.begin();
  webUiEnabled = true;

  Serial.println("\nWeb UI enabled");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void disableWebUi() {
  if (!webUiEnabled) return;

  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  webUiEnabled = false;
  Serial.println("\nWeb UI disabled");
}