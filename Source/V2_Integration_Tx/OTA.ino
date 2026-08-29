/*
** WiFi maintenance mode.
**
** The Tx joins the home network while it sits on the charger and in USB mode
** (throttle plus left toggle at power up). Both mean it is on a cable and not
** in use, so nothing is ever running while riding. The USB mode loop in
** System.ino never returns, which is also why Serial stays alive there.
**
** Everything the USB console can do is reachable from the web terminal, and
** the firmware goes in through /update. See Server.ino and webserial.cpp.
*/

String makeHostname()
{
  uint64_t chipId = ESP.getEfuseMac();
  char host[32];
  snprintf(host, sizeof(host), "bremote-tx-%012llX", chipId);
  return String(host);
}

void startWifiOta()
{
  if(wifi_active) return;
  if(!readWifiFromSPIFFS()) return;

  String host = makeHostname();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(host.c_str());
  //Non blocking, the USB mode loop keeps animating while this comes up
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  //Lets the blocking ?print loops keep serving the web, see webserial.cpp
  Dbg.setPump(handleOta);

  wifi_active = true;

  Serial.print("Wifi maintenance mode, host: ");
  Serial.println(host);
}

void stopWifiOta()
{
  if(!wifi_active) return;

  Dbg.setPump(NULL);

  if(ota_started)
  {
    server.stop();
    ota_started = false;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifi_active = false;

  Serial.println("Wifi off");
}

void handleOta()
{
  if(!wifi_active) return;

  if(!ota_started)
  {
    //The server needs an address, so this waits for the connection
    if(WiFi.status() != WL_CONNECTED) return;

    registerRoutesOnce();
    server.begin();
    ota_started = true;

    Serial.print("Wifi connected, IP: ");
    Serial.println(WiFi.localIP());
  }

  server.handleClient();
}
