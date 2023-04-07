#include <string.h>
#include <stdio.h>

#include "arsd.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define cleanup_if(statement) {\
	if(statement) {\
		fprintf(stderr, "cleanup_if assertion failed:%s\n", #statement);\
		goto cleanup;\
	}\
}

static arsd_config_t* config;

int32_t init_decoder(arsd_config_t* config_in){
	config = config_in;

	av_log_set_level(AV_LOG_ERROR);

	int32_t clip_length_samplerate_product;
	//Set trivial variables
	
	//For this case, I think we should warn and keep moving
	clip_length_samplerate_product = config->samplerate_hz * config->clip_len_ms;
	if((clip_length_samplerate_product % 1000) != 0){
		fprintf(stderr, "WARNING: requested clip length does not evenly divide into samples. Continuing.\n");
	}
	config->clip_len_samples = clip_length_samplerate_product / 1000;

	return 0;
}

int32_t BLOCKING_draw_clip(char* filename, float* output_buffer){
	int32_t rc = -1;

	AVFormatContext* format_context = NULL;

	int32_t chosen_stream = 0;
	
	const AVCodec* decoder;
	AVCodecContext* decoder_context;

	AVPacket* packet;
	AVFrame* frame;

	AVRational timebase_s;
	int32_t tb_per_sample;

	int64_t file_len_tb;
	int64_t file_len_samples;
	int64_t seek_point_samples;
	int64_t seek_point_tb;
	int64_t output_samples;

	int64_t pts_samples;
	int64_t read_start_samples;
	int64_t read_end_samples;

	// fprintf(stderr, "Opening %s\n", filename);

	cleanup_if(avformat_open_input(&format_context, filename, NULL, NULL) != 0);
	cleanup_if(avformat_find_stream_info(format_context, NULL) != 0);
	cleanup_if(
		(chosen_stream = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0)) < 0
	);

	// Prepare to decode
	cleanup_if((decoder_context = avcodec_alloc_context3(decoder)) == NULL);
	cleanup_if(avcodec_open2(decoder_context, decoder, NULL) != 0);
	cleanup_if((packet = av_packet_alloc()) == NULL);
	cleanup_if((frame = av_frame_alloc()) == NULL);

	//Finding the seek point
	timebase_s = format_context->streams[chosen_stream]->time_base;
	tb_per_sample = timebase_s.den / (timebase_s.num * config->samplerate_hz);

	file_len_tb = format_context->streams[chosen_stream]->duration;
	cleanup_if((file_len_tb % tb_per_sample) != 0);
	file_len_samples = file_len_tb / tb_per_sample;

	seek_point_samples =
		((((long)rand()) << 32) | ((long)rand()))
		% (long)(file_len_samples - config->clip_len_samples);

	// Due to MP3 being weird, we need to start decoding ~2000 samples before we start to read
	seek_point_tb = (seek_point_samples - config->run_in_samples) * tb_per_sample;
	seek_point_tb = seek_point_tb < 0 ? 0 : seek_point_tb;
	
	cleanup_if(avformat_seek_file(format_context, chosen_stream, 0, seek_point_tb, seek_point_tb, 0) < 0);

	output_samples = 0;

	// The fun bit
	while ((output_samples) < config->clip_len_samples) {
		if(av_read_frame(format_context, packet) != 0){
			break;
		}
			
		if (packet->stream_index != chosen_stream){
			// This is not the correct stream, skip it
			av_packet_unref(packet);
			continue;
		}

		avcodec_send_packet(decoder_context, packet);

		while(avcodec_receive_frame(decoder_context, frame) == 0 && output_samples < config->clip_len_samples){
			
			// fprintf(stderr, "samplerate:%i\n", frame->sample_rate);
			cleanup_if(frame->sample_rate != config->samplerate_hz);
			cleanup_if(frame->format != AV_SAMPLE_FMT_FLTP); // Assert that we are dealing in 4-byte floats

			//pts_samples is the presentation timestamp in terms of samples
			cleanup_if(frame->pts % tb_per_sample != 0);
			pts_samples = frame->pts / tb_per_sample;
			
			//read_start_samples and read_end_samples are relative to the returned frame
			read_start_samples = seek_point_samples - pts_samples;
			read_start_samples = read_start_samples > frame->nb_samples ? frame->nb_samples : read_start_samples;
			read_start_samples = read_start_samples > 0 ? read_start_samples : 0;

			// fprintf(stderr, "read_start_samples %li ", read_start_samples);

			read_end_samples = config->clip_len_samples - output_samples;
			read_end_samples = read_end_samples > frame->nb_samples ? frame->nb_samples : read_end_samples;
			// fprintf(stderr, "read_end_samples %li ", read_end_samples);

			// fprintf(stderr, "count %li\n", (read_end_samples - read_start_samples));

			cleanup_if(frame->channels != 1);
			
			memcpy(
				output_buffer + output_samples,
				((float*)frame->data[0]) + read_start_samples, 
				(read_end_samples - read_start_samples) * sizeof(float)
			);

			output_samples += read_end_samples - read_start_samples;

			// fprintf(stderr, "output_samples %li\n", (*output_samples));
		}
		av_packet_unref(packet);
	}

	rc = 0;
	cleanup:

	if(packet)av_packet_free(&packet);
	if(frame)av_frame_free(&frame);

	if(format_context)avformat_close_input(&format_context);
	if(decoder_context)avcodec_free_context(&decoder_context);

	return rc;
}