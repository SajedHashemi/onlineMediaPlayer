#include "OnlinePlayer.h"

uint32_t OnlinePlayer::responseIdel(WiFiClient *src, bool codeCheck){
  unsigned long timeout = millis();
  while(src->available() == 0) {
    if(millis() - timeout > 3000) {
#ifdef DEBUGING
      Serial.println("getDataFile >>> Client Timeout !");
#endif
      src->stop();
      return 0;
    }
  }

  bool code200 = !codeCheck;
  uint32_t len = 0;
  while(src->available()){
    String line = src->readStringUntil('\r');
    line.toLowerCase();
    line.trim();
    //Serial.println(line);
    delay(5);
    if(line.indexOf(" 200 ok")) code200 = true;
    if(code200 && line.indexOf("content-length:")>=0) len = line.substring(line.indexOf(":")+1).toInt();
    if(code200 && len>0 && line.length()==0){ src->read(); break; }
  }

  return len;
}

void OnlinePlayer::writeText(String txt, bool clr, String anchor, uint16_t fgColor, uint16_t bgColor, int x, int y){
  int tW = tft.textWidth(txt);
  int tH = tft.fontHeight();
  if(anchor == "top_left"){
    x = 0;
    y = 0;
  }else if(anchor == "top_center"){
    x = (tft.width() - tW) / 2;
    y = 0;
  }else if(anchor == "top_right"){
    x = tft.width() - tW;
    y = 0;
  }else if(anchor == "middle_left"){
    x = 0;
    y = (tft.height() - tH) / 2;
  }else if(anchor == "center"){
    x = (tft.width() - tW) / 2;
    y = (tft.height() - tH) / 2;
  }else if(anchor == "middle_right"){
    x = tft.width() - tW;
    y = (tft.height() - tH) / 2;
  }else if(anchor == "bottom_left"){
    x = 0;
    y = tft.height() - tH;
  }else if(anchor == "bottom_center"){
    x = (tft.width() - tW) / 2;
    y = tft.height() - tH;
  }else if(anchor == "bottom_right"){
    x = tft.width() - tW;
    y = tft.height() - tH;
  }else{
    if(x==-1) x = (tft.width() - tW) / 2;
    if(y==-1) y = (tft.height() - tH) / 2;
  }
  if(clr){
    tft.fillRect(0, y, tft.width(), lastTextHeight, bgColor);
  }
  tft.setTextColor(fgColor, bgColor, true);
  tft.drawString(txt, x, y);
  lastTextHeight = tH;
}

void OnlinePlayer::begin(byte tft_rotation, byte audio_pin, char audio_vol){
  tft.init();
  //tft.initDMA();
  tft.setRotation(tft_rotation);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
      
  if(psramInit() && psramFound()){
    mjpeg_buf = (uint8_t *)ps_malloc(MJPEG_BUFFER_SIZE);
  }else{
    mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
  }

  audio_cfg.lSPKPin = audio_pin;
  audio_cfg.vol = audio_vol;
  audio.begin(audio_cfg);
  audio.enEcho(true);
  audio.setCallback(WAVWrite);
  audio.play(access_wav, access_wav_len);
}

bool OnlinePlayer::setURL(String val){
  _url = val;
  return audio.urlSeperator(&_url, &_server, &_port);
}

bool OnlinePlayer::initVideo(WiFiClient *src, String videoName){
  videoOK = false;
  if(!src->connected()){
    if(WiFi.status() != WL_CONNECTED || !src->connect(_server.c_str(), _port)){
#ifdef DEBUGING
      Serial.println("initVideo >>> connection failed");
#endif
      isConnected = false;
      writeText("connection failed!");
      return false;
    }
  }
  isConnected = true;
  
  _videoName = videoName;
  
  String url = String("/" + _url + _videoName);
  String req = (String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + _server + "\r\n" +
               "Connection: close\r\n\r\n");
  src->print(req);
  uint32_t vlen = responseIdel(src, true);
#ifdef DEBUGING  
  Serial.printf("Video Content-Length: %d\r\n", vlen);
#endif
  if(vlen==0) return false;
  return true; 
}

bool OnlinePlayer::initAudio(WiFiClient *src, String audioName){
  if(!src->connected()){
    if(WiFi.status() != WL_CONNECTED || !src->connect(_server.c_str(), _port)){
#ifdef DEBUGING
      Serial.println("initAudio >>> connection failed");
#endif
      isConnected = false;
      writeText("connection failed!");
      return false;
    }
  }

  isConnected = true;
  _audioName = audioName;
  
  String url = String("/" + _url + _audioName);
  String req = (String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + _server + "\r\n" +
               "Connection: close\r\n\r\n");
  src->print(req);
  return true;
}

String OnlinePlayer::getFilesList(String dir){
  if(!aclient.connected()){
    if(WiFi.status() != WL_CONNECTED || !aclient.connect(_server.c_str(), _port)){
#ifdef DEBUGING
      Serial.println("getAudioList >>> connection failed");
#endif
      isConnected = false;
      writeText("connection failed!");
      return "";
    }
  }

  isConnected = true;
  
  String url = String("/" + _url + dir + "_list");
  String req = (String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + _server + "\r\n" +
               "Connection: close\r\n\r\n");
  aclient.print(req);
  if(responseIdel(&aclient, true)>0){
    String fList ="";
    while(aclient.available()) fList += (char)aclient.read();
    return fList;
  }
  return "";
}

void OnlinePlayer::setFPS(int fps){
  _fps = fps;
  frameDelay = 1000000 / _fps;
}

bool OnlinePlayer::playVideo(String videoName, bool withAudio){
  if(_videoName=="" || _videoName!=videoName)
    if(!setVideoName(videoName))
      return false;

  videoOK = initVideo(&vclient, _videoName);
  if(!videoOK) return false;

  if(withAudio && isConnected){
    _audioName = _videoName;
    _audioName.replace(".mjpeg", ".wav");
    initAudio(&aclient, _audioName);
  }
  
  audioOK = audio.setData(aclient);
  if(audioOK){
    audioLength = audio.getLengthTime(&audioHr, &audioMi, &audioSc);
    audio.setDelay(0);
    audio.start();
  }
  else audio.stop();

  tft.fillScreen(TFT_BLACK);
  writeText(_videoName, false, "bottom_center");
  tft.drawFastHLine(10, tft.height()-20, tft.width()-20, TFT_WHITE);
  
  bytesInBuf = vclient.read(mjpeg_buf, MJPEG_BUFFER_SIZE);
  frameCounter = 0;
  lastFrameMillis = millis();
  isPlaying = true;
  while(isPlaying && bytesInBuf>0 && videoOK){
    isPlaying = loop();
  }

  audio.stop();
  
  aclient.stop();
  vclient.stop();
  
  tft.fillScreen(TFT_BLACK);
  
  isPlaying = false;
  videoOK = false;
  audioOK = false;
  _videoName = "";
  return true;
}

bool OnlinePlayer::playAudio(String audioName){
  if(_audioName=="" || _audioName!=audioName)
    if(!setAudioName(audioName))
      return false; 
  
  if(!initAudio(&aclient, _audioName)) return false;
  
  videoOK = false;
  audioOK = false;

  Serial.println("\r\nWAV Player");
  audioOK = audio.setData(aclient);
  audio.setDelay(0);
  audioLength = (audioOK)?audio.getLengthTime(&audioHr, &audioMi, &audioSc):0;
  
  if(audioOK && audio.isSetParameters) audio.start();
  
  tft.fillScreen(TFT_BLACK);
  writeText(_audioName, false, "bottom_center");
  tft.drawFastHLine(10, tft.height()-20, tft.width()-20, TFT_WHITE);

  if(FFTEnable && gModeSelect==-1) gModeSelect = random(0, 11);
  frameCounter = 0;
  lastFrameMillis = millis();
  isPlaying = true;
  while(isPlaying && audioOK){
    isPlaying = loop();
  }
  Serial.println("Audio Finished!\r\n");
  audio.stop();
  
  aclient.stop();
  
  tft.fillScreen(TFT_BLACK);
  
  isPlaying = false;
  audioOK = false;
  gModeSelect = -1;
  return true;
}

bool OnlinePlayer::setVideoName(String val){
  if(!val.endsWith(".mjpeg")) return false;
  else _videoName = val;
  return true;
}

bool OnlinePlayer::setAudioName(String val){
  if(!val.endsWith(".wav")) return false;
  else _audioName = val;
  return true;
}

bool OnlinePlayer::loop(){
  int frameStartIdx = -1;
  int frameEndIdx = -1;
  unsigned long dly = 0;
  static unsigned long cun = 0;

  if(videoOK){
    for (int i = 0; i < bytesInBuf - 1; i++) {
      if (mjpeg_buf[i] == 0xFF) {
        if (mjpeg_buf[i + 1] == 0xD8) frameStartIdx = i;
        else if (mjpeg_buf[i + 1] == 0xD9) {
          frameEndIdx = i + 1;
          if (frameStartIdx != -1) break;
        }
      }
    }
    if(frameStartIdx != -1 && frameEndIdx != -1){
      int frameSize = frameEndIdx - frameStartIdx + 1;
      
      lastFrameTime = micros();
      if(jpeg.openRAM(mjpeg_buf + frameStartIdx, frameSize, JPEGDraw)){
        int x = (tft.width() - jpeg.getWidth()) / 2;
        int y = (tft.height() - jpeg.getHeight()) / 2;
        jpeg.decode(x, y, 0); 
        jpeg.close();
      }
#ifdef DEBUGING
      dly = micros() - lastFrameTime;
      Serial.printf("tft delay: %d\r\n", dly);
#endif
      uint32_t remaining = bytesInBuf - (frameEndIdx + 1);
      if(remaining > 0) memmove(mjpeg_buf, mjpeg_buf + frameEndIdx + 1, remaining);
        
      int toRead = MJPEG_BUFFER_SIZE - remaining;
      if(toRead > 0){
        int actuallyRead = vclient.read(mjpeg_buf + remaining, toRead);
        bytesInBuf = remaining + actuallyRead;
        if (actuallyRead == 0 && remaining == 0) videoOK = false;
      }else{
        bytesInBuf = remaining;
      }

      dly = micros() - lastFrameTime;
#ifdef DEBUGING
      Serial.printf("tft delay: %d\r\n", dly);
      //Serial.printf("Remaining: %d, byteInBuf: %d, Start: %d, End: %d, frameSize: %d\r\n", remaining, bytesInBuf, frameStartIdx, frameEndIdx, frameSize);
#endif
    }else{
      if (bytesInBuf < MJPEG_BUFFER_SIZE) {
        int actuallyRead = vclient.read(mjpeg_buf + bytesInBuf, MJPEG_BUFFER_SIZE - bytesInBuf);
        if (actuallyRead == 0) videoOK = false;
        bytesInBuf += actuallyRead;
      }else{
        memmove(mjpeg_buf, mjpeg_buf + 1, MJPEG_BUFFER_SIZE - 1);
        bytesInBuf--;
      }
    }
  }

  if(audioOK){
    //dly = 0;
    uint32_t dlydiv = dly/2;
    lastFrameTime = micros();
    while(dly < (frameDelay - dlydiv)){
      if(audio.run()==0){
        audioOK = false;
        break;
      }
      dly = micros() - lastFrameTime;
      //Serial.printf("%s, %s, %d, %d, %d, %d\r\n", (audioOK)?"true":"false", (videoOK)?"true":"false", frameCounter, _fps, audioLength, ((millis()-lastFrameMicros)/1000));
    }
    frameCounter++;
    if((audioOK || videoOK) && audioLength>0){
      byte hr = 0, mi = 0;
      int sc = ((millis()-lastFrameMillis)/1000);
      int cx = ((float)(tft.width()-30)/audioLength) * sc + 10;
      cx = (cx>(tft.width()-10))?(tft.width()-10):cx;
      
      mi = sc / 60;
      sc = sc % 60;
      hr = mi / 60;
      mi = mi % 60;
      
      String ln = ((hr>0)?(String(hr)+":"):"") + String(String(mi)+":"+String(sc));
      writeText(ln, true, "", TFT_WHITE, TFT_BLACK, 10, tft.height()-tft.fontHeight()-25);
      ln = String(((hr>0)?(String(audioHr)+":"):"") + String(audioMi)+":"+String(audioSc));
      writeText(ln, false, "", TFT_WHITE, TFT_BLACK, tft.width()-tft.textWidth(ln)-10, tft.height()-tft.fontHeight()-25);
      tft.fillCircle(cx, tft.height()-20, 2, TFT_WHITE);
      //Serial.printf("%d, cx: %d\r\n", (frameCounter/_fps), cx);
    }
  }else{
    dly = frameDelay - dly;
    delayMicroseconds((dly<0)?frameDelay:dly);
  }

  if(Serial.available()) return false;
  if(!videoOK && !audioOK) return false;
  return true;
}

void OnlinePlayer::FFTResult(uint8_t *src, int width, uint8_t gMode){
  //uint32_t lllsss = micros();
  width = max(width, ((FFT_SAMPLES/2) * 3));
  for(int i = 0; i < FFT_SAMPLES; i++){
    vReal[i] = src[i];
    vImag[i] = 0;
  }
  
  FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal,  vImag, FFT_SAMPLES);
  
  for(int i = 0, j = 0; i < FFT_SAMPLES / 2; i++){    
    int amplitude = ((int)vReal[i] | (int)vReal[i+1])/30;
    amplitude = min(amplitude, min(width*2, 120));
    rectangleGR(j++, (width/(FFT_SAMPLES / 2))-2, amplitude+1, TFT_BLACK, gMode);
  }
  //Serial.println(micros()-lllsss);
}

void OnlinePlayer::rectangleGR(int x0, int w, int h, uint16_t bColor, uint8_t gMode) {
  const int decal = 16;
  int width = ((w+2)*(FFT_SAMPLES / 2));
  int height = min(width*2, 120) + 8;
  int x = ((tft.width() - width) / 2) + (x0 * (w + 2));
  int ry = ((tft.height() - height) / 2);
  int dh = (height - decal) / 8;
  switch(gMode){
    case 0:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      int y = ry + height - h;
      tft.fillRect(x, y, w, h, (bColor==TFT_BLACK)?TFT_WHITE:bColor);
    }break;
    case 1:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      int y = ry + height - h;
      tft.drawRect(x, y, w, h, (bColor==TFT_BLACK)?TFT_WHITE:bColor);
    }break;
    case 2:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      for (int i = 1; i < 8; i++) {
        int y = 0;
        if (h > i * dh) {
          y = ry + height - decal - i * dh;
          tft.fillRect(x, y, w, dh, colors[i - 1]);
        }else{
          y = ry + height - decal - h;
          tft.fillRect(x, y, w, h - (i - 1) * dh, colors[i - 1]);
        }
      }
    }break;
    case 3:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      barColor = random(0x0000, 0xFFFF);
      int y = ry + height - h;
      tft.fillRect(x, y, w, h, barColor);
    }break;
    case 4:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      barColor = random(0x0000, 0xFFFF);
      int y = ry + height - h;
      tft.drawRect(x, y, w, h, barColor);
    }break;
    case 5:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      if(x0 == 0) barColor = random(0x0000, 0xFFFF);
      int y = ry + height - h;
      tft.fillRect(x, y, w, h, barColor);
    }break;
    case 6:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      if(x0 == 0) barColor = random(0x0000, 0xFFFF);
      int y = ry + height - h;
      tft.drawRect(x, y, w, h, barColor);
    }break;
    case 7:{
      tft.fillRect(x, ry, w, height, TFT_BLACK);
      if(bColor == TFT_BLACK) barColor = colors[h / dh];
      else barColor = bColor;
      int y = ry + height - h - ((w-2)/2);
      tft.fillCircle(x+(w/2), y, (w-2)/2, barColor);
    }break;
    case 8:{
      tft.fillRect(x, ry, w, height, TFT_BLACK);
      if(bColor == TFT_BLACK) barColor = colors[h / dh];
      else barColor = bColor;
      int y = ry + height - h;
      tft.drawFastHLine(x, y, w, barColor);
    }break;
    case 9:{
      dh /= 2;
      tft.fillRect(x, ry, w, height, TFT_BLACK);
      for (int i = 1; i < 8; i++) {
        if(bColor == TFT_BLACK) barColor = colors[i - 1];
        else barColor = bColor;
        if ((h/2) > i * dh) {
          int y = ry + (height/2) - (decal/2) - i * dh;
          tft.fillRect(x, y, w, dh, barColor);
          y = ry + (height/2) - (decal/2) + i * dh;
          tft.fillRect(x, y, w, dh, barColor);
        }else{
          int y = ry + (height/2) - (decal/2) - (h/2);
          tft.fillRect(x, y, w, (h/2) - (i - 1) * dh, barColor);
          y = ry + (height/2) - (decal/2) + (h/2);
          tft.fillRect(x, y, w, (h/2) - (i - 1) * dh, barColor);
        }
      }
    }break;
    case 10:{
      tft.fillRect(x, ry, w+2, height, TFT_BLACK);
      if(bColor == TFT_BLACK) barColor = colors[h / dh];
      else barColor = bColor;
      int y = ry + height - h;
      int lastBarX = ((tft.width() - ((w+2)*(FFT_SAMPLES / 2))) / 2) + ((x0-1) * (w + 2));
      if(x0 > 0) tft.drawLine(lastBarX, lastBarH, x, y, barColor);
      lastBarH = y;
    }break;
    case 11:{
      tft.fillRect(x, ry, w, height-h, TFT_BLACK);
      for (int i = 1; i < 8; i++) {
        if(bColor == TFT_BLACK) barColor = colors[i-1];
        else barColor = bColor;
        int y = 0;
        if (h > i * dh) {
          y = ry + height - decal - i * dh;
          tft.fillRect(x, y, w, dh-2, barColor);
        }else{
          y = ry + height - decal - h;
          tft.fillRect(x, y, w, (h - (i - 1) * dh)-2, barColor);
        }
      }
    }break;
  }
}
