#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <JPEGDEC.h>
#include <pwmWav.h>
#include <arduinoFFT.h>

#include "data.h"

//#define DEBUGING
#define SPK_PIN        16
#define FFT_SAMPLES    16  //Must be a power of 2
#define FFT_SAMPLE_FRQ 100

static const uint16_t colors[8] = {TFT_DARKGREEN, TFT_GREEN, TFT_GREENYELLOW, TFT_YELLOW,TFT_GOLD, TFT_ORANGE, TFT_RED, TFT_BROWN};

static TFT_eSPI tft = TFT_eSPI();
static JPEGDEC jpeg;
static pwmWav audio;
static bool audioOK = false;
static bool videoOK = false;

static uint32_t lastFrameMillis = 0;

static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static uint16_t barColor = 0;
static int lastBarH = 0;
static ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SAMPLES, FFT_SAMPLE_FRQ);  // FFT object
static int8_t gModeSelect = -1;

#define MJPEG_BUFFER_SIZE (1024 * 20)

enum{
  TFT_PORTRATE,
  TFT_LANDSCAPE
};

class OnlinePlayer{
  public:
    int _port = 0;
    String _server = "";
    String _url = "";
    bool isPlaying = false;
    bool isConnected = true;
    bool FFTEnable = false;
    
    OnlinePlayer(bool DMA = false){}

    void begin(byte tft_rotation = 0, byte audio_pin = SPK_PIN, char audio_vol = -12);
    bool setURL(String);
    bool setVideoName(String);
    bool setAudioName(String);
    bool playVideo(String videoName, bool withAudio = true);
    bool playAudio(String audioName);
    bool initVideo(WiFiClient *, String);
    bool initAudio(WiFiClient *, String);
    String getFilesList(String);
    void setFPS(int);
    bool loop(void);

    void writeText(String txt, bool clr=true, String anchor="center", uint16_t fgColor=TFT_WHITE, uint16_t bgColor=TFT_BLACK, int x = -1, int y = -1);
    void setVolume(int8_t val){ audio.setVolume(val); }
    int8_t getVolume(){ return audio.getVolume(); }
    
  private:
    int _fps = 10;
    unsigned long frameDelay = 1000000 / _fps;
    uint32_t frameCounter = 0;
    
    uint8_t *mjpeg_buf;
    
    outconfig_t audio_cfg;

    WiFiClient aclient, vclient;

    unsigned long lastFrameTime = 0;
    int bytesInBuf = 0;
    int lastTextHeight = 0;
    String _videoName = "";
    String _audioName = "";
    uint32_t audioLength = 0;
    uint8_t audioHr, audioMi, audioSc;

    static int JPEGDraw(JPEGDRAW *pDraw){
      tft.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
      if(audioOK) audio.run();
      return 1;
    }

    static void WAVWrite(uint8_t *pwm_buffer, int len) {
      audio.run(pwm_buffer, len);
      if(audioOK && !videoOK && (millis()-lastFrameMillis)%3==0){
        uint8_t dst[FFT_SAMPLES];
        memcpy(dst, pwm_buffer, FFT_SAMPLES);
        FFTResult(dst, 40, gModeSelect);
      }
    }

    static void FFTResult(uint8_t *src, int width = (tft.width()/6), uint8_t gMode = 10);
    static void rectangleGR (int x0, int w, int h, uint16_t bColor, uint8_t gMode);
    uint32_t responseIdel(WiFiClient *src, bool codeCheck = true);
};
