# MW
Simple middlewave AM-Modulation @ 1,3 MHz.<br>
Takes .wav/.mp3-files as parameter and broadcasts the stream

Uses LimeSDR Hardware / LimeSuite Library<br>
..and libav

compile<br>
g++ -std=c++11 -o mw mw.cpp -g -lLimeSuite -lavcodec -lavformat -lavutil -lswresample
