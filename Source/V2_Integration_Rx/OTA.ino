/*
** WiFi maintenance mode.
**
** The Rx tries to join the home network at boot and shuts WiFi down again as
** soon as the remote sends its first packet, so nothing is running while
** riding. To reach it, power the Rx up with the remote switched off.
**
** Everything the USB console can do is reachable from the web terminal, and
** the firmware goes in through /update. See Server.ino and webserial.cpp.
*/

String makeHostname()
{
  uint64_t chipId = ESP.getEfuseMac();
  char host[32];
  snprintf(host, sizeof(host), "bremote-rx-%012llX", chipId);
  return String(host);
}

void startWifiOta()
{
  if(!readWifiFromSPIFFS()) return;

  String host = makeHostname();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(host.c_str());
  //Non blocking, setup() carries on while the connection comes up. Without a
  //network in range nothing is delayed, handleOta() just never gets further.
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

  //Remote is talking to us: WiFi goes off and stays off until the next reboot
  if(last_packet != 0)
  {
    stopWifiOta();
    return;
  }

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
