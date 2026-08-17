# ESP32 MJPEG Online Video Player 📺

Online audio (WAV) and video (MJPEG) playback using ESP32.

# Libraries used:
- TFT_eSPI: To set up the display (the display used in this project is ST7789 with a size of 240*240). Of course, by making some settings in the User_config.h file in the folder of this library, other displays can also be used.
- pwmWav: To set up the sound output, which can be configured to any desired pin and is able to play WAV audio files locally and online. You can use the link below to download this library.
- arduinoFFT: To calculate and generate the audio FFT to display the visualizer spectrum on the display
- QList: To store the names of the files in the server file address, which must have a routine to send the file names on the server.