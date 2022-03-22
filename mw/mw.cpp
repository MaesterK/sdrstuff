/**
    @file   mw.cpp
    @author Karsten Schlachter
    @brief  simple am-modulation test broadcasting  mp3/wav-files @1,3mhz using libav based on basicTx.cpp from Lime Microsystems
 */
#include <iostream>
#include <chrono>
#include <math.h>
#include "lime/LimeSuite.h"
#include <unistd.h>
#include <string.h>
extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}


using namespace std;


lms_device_t* device = NULL;

int error()
{
    if (device != NULL)
  	LMS_Close(device);
    exit(-1);
}

	
//libav-code copied from their examples
int decode_audio_file(const char* path, const int sample_rate, double** data, int* size) {
 
    // initialize all muxers, demuxers and protocols for libavformat
    // (does nothing if called twice during the course of one program execution)
    av_register_all();
 
    // get format from audio file
    AVFormatContext* format = avformat_alloc_context();
    if (avformat_open_input(&format, path, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file '%s'\n", path);
        return -1;
    }
    if (avformat_find_stream_info(format, NULL) < 0) {
        fprintf(stderr, "Could not retrieve stream info from file '%s'\n", path);
        return -1;
    }
 
    // Find the index of the first audio stream
    int stream_index =- 1;
    for (int i=0; i<format->nb_streams; i++) {
        if (format->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
            break;
        }
    }
    if (stream_index == -1) {
        fprintf(stderr, "Could not retrieve audio stream from file '%s'\n", path);
        return -1;
    }
    AVStream* stream = format->streams[stream_index];
 
    // find & open codec
    AVCodecContext* codec = stream->codec;
    if (avcodec_open2(codec, avcodec_find_decoder(codec->codec_id), NULL) < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u in file '%s'\n", stream_index, path);
        return -1;
    }
 
    // prepare resampler
    struct SwrContext* swr = swr_alloc();
    av_opt_set_int(swr, "in_channel_count",  codec->channels, 0);
    av_opt_set_int(swr, "out_channel_count", 1, 0);
    av_opt_set_int(swr, "in_channel_layout",  codec->channel_layout, 0);
    av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(swr, "in_sample_rate", codec->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt",  codec->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_DBL,  0);
    swr_init(swr);
    if (!swr_is_initialized(swr)) {
        fprintf(stderr, "Resampler has not been properly initialized\n");
        return -1;
    }
 
    // prepare to read data
    AVPacket packet;
    av_init_packet(&packet);
    AVFrame* frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Error allocating the frame\n");
		return -1;
	}
 
    // iterate through frames
    *data = NULL;
    *size = 0;
    while (av_read_frame(format, &packet) >= 0) {
        // decode one frame
        int gotFrame;
        if (avcodec_decode_audio4(codec, frame, &gotFrame, &packet) < 0) {
            break;
        }
        if (!gotFrame) {
            continue;
        }
        // resample frames
        double* buffer;
        av_samples_alloc((uint8_t**) &buffer, NULL, 1, frame->nb_samples, AV_SAMPLE_FMT_DBL, 0);
        int frame_count = swr_convert(swr, (uint8_t**) &buffer, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples);
        // append resampled frames to data
        *data = (double*) realloc(*data, (*size + frame->nb_samples) * sizeof(double));
        memcpy(*data + *size, buffer, frame_count * sizeof(double));
        *size += frame_count;
    }
 
    // clean up
    av_frame_free(&frame);
    swr_free(&swr);
    avcodec_close(codec);
    avformat_free_context(format);
 
    // success
    return 0;
 
}






int main(int argc, char** argv)
{
    const double frequency = 50e6;  //center frequency to x MHz
    const double sample_rate = 4e6;    //sample rate to x MHz
 const double frequency_nco[16] = {49e6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  //center frequency to x MHz
   const double nco_phase = 0; 
    
    
    // decode data
    int audio_sample_rate = 44100;
    double* audio_data;
    int audio_size;

	char* audiofile="./sine1000.wav";
	if(argc > 1)
	{	
		audiofile=argv[1];
	}
  //  if (decode_audio_file("./inkspots.wav", audio_sample_rate, &audio_data, &audio_size) != 0)
	if (decode_audio_file(audiofile, audio_sample_rate, &audio_data, &audio_size) != 0) 
	{
        return -1;
    }
 
    // sum data
    double sum = 0.0;
    double min=0,max=0;
    for (int i=0; i<audio_size; ++i) {
        sum += audio_data[i];
		if(min>audio_data[i]) min = audio_data[i];
		if(max<audio_data[i]) max = audio_data[i];
    }
	printf("sum is %f min %f max %f\n", sum,min,max);
	cout << "audio size: "<<audio_size<<endl;
	audio_size;// /=10;

	
	//========================================================
	//initialiserungskram aus basicTX.cpp (LimeSDK Beispiel)

	//Find devices
    int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
        error();

    cout << "Devices found: " << n << endl; //print number of devices
    if (n < 1)
        return -1;

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
        error();

    //Initialize device with default configuration
    //Do not use if you want to keep existing configuration
    //Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI
    if (LMS_Init(device)!=0)
        error();

    //Enable TX channel,Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, true)!=0)
        error();

    //Set sample rate
    if (LMS_SetSampleRate(device, sample_rate, 0)!=0)
        error();
    cout << "Sample rate: " << sample_rate/1e6 << " MHz" << endl;

    //Set center frequency
    if (LMS_SetLOFrequency(device,LMS_CH_TX, 0, frequency)!=0)
        error();
    cout << "Center frequency: " << frequency/1e6 << " MHz" << endl;


    if (LMS_SetNCOFrequency(device,LMS_CH_TX, 0, frequency_nco,nco_phase)!=0)
        error();
    cout << "Center frequency: " << frequency/1e6 << " MHz" << endl;

	LMS_SetNCOIndex(device,LMS_CH_TX,0,0,true);

    
    /*
    if (LMS_SetLOFrequency(device,LMS_CH_TX, 0, frequency)!=0)
        error();
    cout << "Center frequency: " << frequency/1e6 << " MHz" << endl;
*/

    //select TX1_1 antenna
	//tx2-antennen sind die fuer oberen frequenzbereich
    if (LMS_SetAntenna(device, LMS_CH_TX, 0, LMS_PATH_TX1)!=0)
        error();

    //set TX gain
    if (LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 0.95) != 0)
        error();

    //calibrate Tx, continue on failure
    LMS_Calibrate(device, LMS_CH_TX, 0, sample_rate, 0);
    
    //Streaming Setup


	 int audio_samples_per_symbol=sample_rate/audio_sample_rate;

	//ein zusatzsample um mit hig bit nach daten zu enden
	
const float transfer_time_unit_s = 0.001;	
const int transfer_size = 2*audio_sample_rate*transfer_time_unit_s*audio_samples_per_symbol;//platz fuer 1s uebertragung
const int buffer_size =   1*2*audio_sample_rate*transfer_time_unit_s*audio_samples_per_symbol;//3*transfer_size;//(audio_size+1) * audio_samples_per_symbol; //1024*8;


    lms_stream_t tx_stream;                 //stream structure
    tx_stream.channel = 0;                  //channel number
    tx_stream.fifoSize =2*transfer_size;//platz fuer 3*10s // 2 * pkgbitcount * sample_rate / 1e6;//256*1024;          //fifo size in samples
    tx_stream.throughputVsLatency = 0;    //0 min latency, 1 max throughput
    tx_stream.dataFmt = lms_stream_t::LMS_FMT_F32; //floating point samples
    tx_stream.isTx = true;                  //TX channel


    LMS_SetupStream(device, &tx_stream);

    std::cout << "buffer size: " << buffer_size<<"\n";
	

    float *tx_buffer;
    tx_buffer=(float*)malloc(buffer_size*sizeof(float));
	 if(tx_buffer==0)
	 {
		 cout<<"kein speicher :-("<<endl;
	 }
 
    
    LMS_StartStream(&tx_stream);         //Start streaming
    //Streaming
    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = t1;


	int send_cnt =buffer_size/2;// transfer_size*2;

	cout << "sendcount: "<<send_cnt<<endl;
	
	
		 lms_stream_meta_t 	meta={0};
		meta.flushPartialPacket=true;
	
unsigned int lastaudiosampleindex=0;
   while (true)
    {
//continue;

int zerocount=0;
int nonzerocount =0;

	//zeit buffer nachzufuellen?
	lms_stream_status_t streamstate;
	LMS_GetStreamStatus(&tx_stream,&streamstate);
	//hat fuellstand unteres drittel erreicht?
	if(streamstate.fifoFilledCount < (1*transfer_size)  )
	{
		
		cout << "fillcount " << streamstate.fifoFilledCount << endl;
		
			//sampledaten fuer ble paket limesdr senden
		int ret = LMS_SendStream(&tx_stream,tx_buffer, send_cnt, &meta, 1000);	
		
		//naechste 20s uebertragen
		int sampleindex = 0;
		int i=lastaudiosampleindex+1;
		for (; i < (lastaudiosampleindex + 1*audio_sample_rate*transfer_time_unit_s); i++) {
			
			float audiosample=(audio_data[(i%audio_size)]-min)/(max-min)*0.9+0.1;
			for (int rsampleindex=0; rsampleindex < audio_samples_per_symbol; rsampleindex++)
			{
							  
				tx_buffer[2*sampleindex+1] = audiosample; //sin(t);
				
				sampleindex++;			
			}
			
			//cout<<"sampleindex: "<<sampleindex;
			
		}
		lastaudiosampleindex = i % audio_size;
		
		
	//	cout << "lastaudiosampleindex " <<  lastaudiosampleindex<< endl;
		
	//	LMS_GetStreamStatus(&tx_stream,&streamstate);
	//	cout << "fillcount nach gen " << streamstate.fifoFilledCount << endl;
		
	//cout << zerocount << " vs "<<nonzerocount << endl;


		
		//int ret=send_cnt;			
		//erfolg pruefen
		if (ret != send_cnt)
		{
			cout << "error: samples sent: " << ret << "/" << send_cnt << endl;
		}
	}	else {
		//cout << "fillcount" << streamstate.fifoFilledCount << endl;
	}
/*	
	if (chrono::high_resolution_clock::now() - t2 > chrono::seconds(1))
	{
		t2 = chrono::high_resolution_clock::now();
		lms_stream_status_t status;
		LMS_GetStreamStatus(&tx_stream, &status);  //Get stream status
		//cout << "TX data rate: " << status.linkRate / 1e6 << " MB/s\n"; //link data rate
		cout << "*"<<flush;//lebenszeichen von sich geben
	}*/

		//anschliessend fuer ein advertising interval warten
		//adv-interval = 20ms-10s + 0-10ms rnd gegen kollision        
	//	usleep(100000);	

    }
	
	//========================================================
	//cleanup aus basicTx.cpp
    //Stop streaming
    LMS_StopStream(&tx_stream);
    LMS_DestroyStream(device, &tx_stream);

    //Disable TX channel
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, false)!=0)
        error();

    //Close device
    if (LMS_Close(device)==0)
        cout << "Closed" << endl;
    return 0;
}
