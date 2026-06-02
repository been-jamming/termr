#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NUM_TRACKS 8

MIX_Mixer *mixer;
MIX_Audio *click_audio;
MIX_Track *click_tracks[NUM_TRACKS];
int last_track = 0;

void init_audio(void){
	int i;

	SDL_Init(SDL_INIT_AUDIO);
	if(!MIX_Init()){
		fprintf(stderr, "Error: Failed to initialize SDL mixer\n");
		exit(1);
	}

	mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if(!mixer){
		fprintf(stderr, "Error: Failed to create SDL mixer device\n");
		exit(1);
	}

	click_audio = MIX_LoadAudio(mixer, "media/click.mp3", true);
	if(!click_audio){
		fprintf(stderr, "Error: Failed to load click audio\n");
		exit(1);
	}

	for(i = 0; i < NUM_TRACKS; i++){
		click_tracks[i] = MIX_CreateTrack(mixer);
		if(!click_tracks[i]){
			fprintf(stderr, "Error: Failed to create SDL mixer track\n");
			exit(1);
		}
		if(!MIX_SetTrackGain(click_tracks[i], 0.25 + (rand()%1024)/1024.0*0.75)){
			fprintf(stderr, "Error: Failed to set track gain\n");
		}
		if(!MIX_SetTrackAudio(click_tracks[i], click_audio)){
			fprintf(stderr, "Error: Failed to set track audio\n");
			exit(1);
		}
	}
}

void play_click(void){
	int i;

	for(i = (last_track + 1)%NUM_TRACKS; i != last_track; i = (i + 1)%NUM_TRACKS){
		if(!MIX_TrackPlaying(click_tracks[i])){
			if(!MIX_PlayTrack(click_tracks[4], 0)){
				fprintf(stderr, "Error: Failed to play track\n");
			}
			last_track = i;
			break;
		}
	}
}

void deinit_audio(void){
	int i;

	for(i = 0; i < NUM_TRACKS; i++){
		MIX_DestroyTrack(click_tracks[i]);
	}

	MIX_DestroyAudio(click_audio);
	MIX_DestroyMixer(mixer);
	MIX_Quit();
	SDL_Quit();
}

