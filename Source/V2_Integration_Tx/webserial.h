#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <Arduino.h>

/*
** WebSerial mirrors everything the sketch prints to Serial into a ring buffer
** that the web terminal polls, and feeds lines coming from the browser back in
** as if they had been typed on the USB console.
**
** The sketch does not call this class directly: BREmote_V2_Tx.h ends with
** "#define Serial Dbg", so all existing Serial.print/println/printf/available/
** readStringUntil calls end up here without a single call site being touched.
*/

#define WS_OUT_SIZE 4096
//Long enough for the base64 config strings of ?setConf and ?setBC
#define WS_IN_SIZE  512

class WebSerial : public Stream
{
  public:
    //Print/Stream interface used by the sketch
    size_t write(uint8_t c);
    size_t write(const uint8_t *buf, size_t len);
    int available();
    int read();
    int peek();
    void flush();

    void begin(unsigned long baud);
    void end();

    //Called by the web server: hand over one command line from the browser.
    //A newline is appended if missing, otherwise readStringUntil('\n') would
    //block until its one second timeout expires. Returns false if the line did
    //not fit, so the browser hears about it instead of sending half a config.
    bool wsPush(const char *data, size_t len);

    //Called by the web server: copy out everything printed since "cursor".
    //Returns the number of bytes written to dst and advances cursor.
    size_t wsPull(uint8_t *dst, size_t max, uint32_t &cursor);

    //Total number of bytes ever printed, the cursor a fresh client starts at
    uint32_t wsTotal();

    //available() calls this so the blocking ?print loops keep serving the web.
    //Set to handleOta() in startWifiOta(), see OTA.ino.
    void setPump(void (*fn)(void)) { pump = fn; }

  private:
    uint8_t out_buf[WS_OUT_SIZE];
    uint32_t out_total = 0;

    uint8_t in_buf[WS_IN_SIZE];
    uint16_t in_head = 0;
    uint16_t in_tail = 0;

    bool usb_on = false;
    bool in_pump = false;

    void (*pump)(void) = NULL;

    portMUX_TYPE ws_mux = portMUX_INITIALIZER_UNLOCKED;
};

extern WebSerial Dbg;

#endif
