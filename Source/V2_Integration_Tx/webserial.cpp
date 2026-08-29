#include "webserial.h"

WebSerial Dbg;

/*
** Output: goes to USB and into the ring buffer at the same time.
** The real Serial write is kept outside the critical section because the USB
** CDC can block, and print() is also called from the FreeRTOS tasks.
*/
size_t WebSerial::write(uint8_t c)
{
  if(usb_on) Serial.write(c);

  portENTER_CRITICAL(&ws_mux);
  out_buf[out_total % WS_OUT_SIZE] = c;
  out_total++;
  portEXIT_CRITICAL(&ws_mux);

  return 1;
}

size_t WebSerial::write(const uint8_t *buf, size_t len)
{
  if(usb_on) Serial.write(buf, len);

  portENTER_CRITICAL(&ws_mux);
  for(size_t i = 0; i < len; i++)
  {
    out_buf[out_total % WS_OUT_SIZE] = buf[i];
    out_total++;
  }
  portEXIT_CRITICAL(&ws_mux);

  return len;
}

/*
** Input: the browser buffer is served first, then USB.
**
** The pump call is what makes the blocking ?print loops work over the web:
** they sit in "while(1){ if(Serial.available()) ... }" and never touch the
** web server themselves, so it gets served from here. The guard stops the
** pump from re-entering available() through one of its own handlers.
*/
int WebSerial::available()
{
  if(pump && !in_pump)
  {
    in_pump = true;
    pump();
    in_pump = false;
  }

  int web = (int)((uint16_t)(in_head - in_tail) % WS_IN_SIZE);
  if(web) return web;

  if(usb_on) return Serial.available();
  return 0;
}

int WebSerial::read()
{
  if(in_head != in_tail)
  {
    uint8_t c = in_buf[in_tail];
    in_tail = (in_tail + 1) % WS_IN_SIZE;
    return c;
  }

  if(usb_on) return Serial.read();
  return -1;
}

int WebSerial::peek()
{
  if(in_head != in_tail) return in_buf[in_tail];

  if(usb_on) return Serial.peek();
  return -1;
}

void WebSerial::flush()
{
  if(usb_on) Serial.flush();
}

void WebSerial::begin(unsigned long baud)
{
  Serial.begin(baud);
  usb_on = true;
}

void WebSerial::end()
{
  if(usb_on) Serial.end();
  usb_on = false;
}

bool WebSerial::wsPush(const char *data, size_t len)
{
  for(size_t i = 0; i < len; i++)
  {
    uint16_t next = (in_head + 1) % WS_IN_SIZE;
    if(next == in_tail) return false;   //full, the caller reports it
    in_buf[in_head] = (uint8_t)data[i];
    in_head = next;
  }

  //readStringUntil('\n') would wait for its timeout without this
  if(len == 0 || data[len-1] != '\n')
  {
    uint16_t next = (in_head + 1) % WS_IN_SIZE;
    if(next == in_tail) return false;
    in_buf[in_head] = '\n';
    in_head = next;
  }

  return true;
}

size_t WebSerial::wsPull(uint8_t *dst, size_t max, uint32_t &cursor)
{
  portENTER_CRITICAL(&ws_mux);
  uint32_t total = out_total;
  portEXIT_CRITICAL(&ws_mux);

  //A slow client can fall behind by more than the buffer holds, then the
  //oldest bytes are already overwritten and it continues from what is left
  if(total - cursor > WS_OUT_SIZE) cursor = total - WS_OUT_SIZE;

  size_t cnt = 0;
  while(cursor < total && cnt < max)
  {
    dst[cnt++] = out_buf[cursor % WS_OUT_SIZE];
    cursor++;
  }
  return cnt;
}

uint32_t WebSerial::wsTotal()
{
  portENTER_CRITICAL(&ws_mux);
  uint32_t total = out_total;
  portEXIT_CRITICAL(&ws_mux);
  return total;
}
