/*
** Web terminal and firmware upload, in the style of the logger branch's
** Server.ino.
**
** The terminal routes straight into the WebSerial instance, so every "?"
** command that works on the USB console works here as well, including the
** ones added later. See webserial.cpp for how that is wired up.
*/

#include "page_term.h"

bool routesRegistered = false;
bool updateAuthOk = false;

//Basic auth against the maintenance password from /wifi.txt, user "bremote"
bool requireAuth()
{
  if(server.authenticate("bremote", wifi_otapass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

void handleTerminalPage()
{
  if(!requireAuth()) return;
  server.send_P(200, "text/html", PAGE_TERM);
}

//One command line from the browser, handed over as if it had been typed
void handleCmd()
{
  if(!requireAuth()) return;

  String body = server.arg("plain");

  if(!Dbg.wsPush(body.c_str(), body.length()))
  {
    server.send(413, "text/plain", "command too long");
    return;
  }

  server.send(200, "text/plain", "ok");
}

//New output bytes since the client's cursor, next cursor goes in the header.
//Without "since" the whole buffer is handed over, so a page opened later
//still shows the boot log.
void handleOut()
{
  if(!requireAuth()) return;

  uint32_t cursor = 0;
  if(server.hasArg("since"))
  {
    cursor = (uint32_t)strtoul(server.arg("since").c_str(), NULL, 10);
  }

  uint8_t buf[1024];
  size_t len = Dbg.wsPull(buf, sizeof(buf), cursor);

  server.sendHeader("X-Next", String(cursor));
  server.send_P(200, "text/plain", (PGM_P)buf, len);
}

void handleUpdatePage()
{
  if(!requireAuth()) return;

  String html;
  html.reserve(900);

  html += F(
    "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>BREmote Firmware</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:1200px;margin:0 auto;padding:20px;}</style>"
    "</head><body>"
    "<h3>Firmware update</h3>"
    "<p>Upload the .bin from the Arduino IDE (Sketch, Export Compiled Binary).</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin'> "
    "<input type='submit' value='Update'>"
    "</form>"
    "<p><a href='/'>Back to terminal</a></p>"
    "</body></html>"
  );

  server.send(200, "text/html", html);
}

//The upload handler runs before the completion handler, so the password has
//to be checked here as well. Otherwise the flash would be rewritten first and
//only then the request rejected.
void handleUpdateUpload()
{
  HTTPUpload& upload = server.upload();

  if(upload.status == UPLOAD_FILE_START)
  {
    updateAuthOk = server.authenticate("bremote", wifi_otapass.c_str());
    if(!updateAuthOk) return;

    Serial.print("Update: ");
    Serial.println(upload.filename);

    if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  }
  else if(upload.status == UPLOAD_FILE_WRITE)
  {
    if(!updateAuthOk) return;
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
  }
  else if(upload.status == UPLOAD_FILE_END)
  {
    if(!updateAuthOk) return;

    if(Update.end(true))
    {
      Serial.print("Update done, ");
      Serial.print(upload.totalSize);
      Serial.println(" byte");
    }
    else
    {
      Update.printError(Serial);
    }
  }
}

void handleUpdateDone()
{
  if(!updateAuthOk)
  {
    server.requestAuthentication();
    return;
  }

  server.sendHeader("Connection", "close");

  if(Update.hasError())
  {
    server.send(200, "text/plain", "Update failed");
    return;
  }

  server.send(200, "text/plain", "Update done, rebooting");
  delay(500);
  ESP.restart();
}

void registerRoutesOnce()
{
  if (routesRegistered) return;

  server.on("/", HTTP_GET, handleTerminalPage);
  server.on("/cmd", HTTP_POST, handleCmd);
  server.on("/out", HTTP_GET, handleOut);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);

  routesRegistered = true;
}
