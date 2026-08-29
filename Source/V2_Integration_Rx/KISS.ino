/*
** KISS telemetry from a BLHeli_32 or AM32 ESC (data_src 3).
**
** Wiring and mux channel are the same as for a VESC: the ESC telemetry wire
** goes to JP5, setUartMux(0). Only the protocol differs, so nothing about the
** UART setup changes.
**
** Unlike the VESC there is no request and response: with "Auto Telemetry"
** enabled the ESC just keeps sending, and the frame carries no start byte.
** Frames are found by the pause between them, the CRC then confirms the frame.
** Sliding a window over the stream and trusting the CRC alone would not work,
** a CRC8 matches roughly every 256 misaligned positions.
*/

void getKissLoop()
{
  setUartMux(0);
  vTaskDelay(pdMS_TO_TICKS(10));

  uint8_t frame[KISS_FRAME_LEN];
  uint8_t idx = 0;
  bool got = false;

  unsigned long started = millis();
  unsigned long last_byte = micros();

  while(millis() - started < 100)
  {
    if(Serial1.available())
    {
      unsigned long now = micros();

      //The ten bytes of one frame follow each other within about 90us at
      //115200 baud, so a longer pause is the start of the next frame
      if(now - last_byte > 1000) idx = 0;
      last_byte = now;

      uint8_t c = Serial1.read();
      if(idx < KISS_FRAME_LEN) frame[idx] = c;
      idx++;

      if(idx == KISS_FRAME_LEN && kissDecode(frame, &kiss))
      {
        got = true;
        idx = 0;
      }
    }
    else
    {
      //Sleeping is only safe while we are clearly between frames, otherwise
      //the byte timing above would no longer be trustworthy
      if(micros() - last_byte > 3000) vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  if(got)
  {
    last_uart_packet = millis();

    fbatVolt = (float)kiss.volt / 100.0;
    telemetry.foil_bat = getUbatPercent(fbatVolt);
    telemetry.foil_temp = kiss.temp;
  }

  get_vesc_timer = millis();

  // Check for ESC connection break, same timeout as the VESC path
  if(millis() - last_uart_packet > 20000)
  {
    telemetry.foil_bat = 0xFF;
    telemetry.foil_temp = 0xFF;
  }
}
