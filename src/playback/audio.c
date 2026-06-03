#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NUM_TRACKS 3

static char *audio_files[NUM_TRACKS] = {"media/click1.mp3", "media/click2.mp3", "media/click3.mp3"};
static float track_gains[NUM_TRACKS] = {0.8, 0.8, 0.8};

static MIX_Mixer *mixer;
static MIX_Audio *click_audios[NUM_TRACKS];
static MIX_Track *click_tracks[NUM_TRACKS];

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


	for(i = 0; i < NUM_TRACKS; i++){
		click_audios[i] = MIX_LoadAudio(mixer, audio_files[i], true);
		if(!click_audios[i]){
			fprintf(stderr, "Error: Failed to load click audio\n");
			exit(1);
		}
		click_tracks[i] = MIX_CreateTrack(mixer);
		if(!click_tracks[i]){
			fprintf(stderr, "Error: Failed to create SDL mixer track\n");
			exit(1);
		}
		if(!MIX_SetTrackGain(click_tracks[i], track_gains[i])){
			fprintf(stderr, "Error: Failed to set track gain\n");
			exit(1);
		}
		if(!MIX_SetTrackAudio(click_tracks[i], click_audios[i])){
			fprintf(stderr, "Error: Failed to set track audio\n");
			exit(1);
		}
	}
}

void play_click(void){
	int i, j;
	int place;
	int random_order[NUM_TRACKS];

	for(i = 0; i < NUM_TRACKS; i++){
		random_order[i] = -1;
	}

	for(i = 0; i < NUM_TRACKS; i++){
		place = rand()%(NUM_TRACKS - i);
		for(j = 0; j < NUM_TRACKS; j++){
			if(place > 0 && random_order[j] != -1){
				place--;
			} else if(place == 0 && random_order[j] == -1){
				random_order[j] = i;
			}
		}
	}

	for(i = 0; i < NUM_TRACKS; i++){
		if(!MIX_TrackPlaying(click_tracks[random_order[i]])){
			if(!MIX_SetTrackGain(click_tracks[random_order[i]], track_gains[random_order[i]] + (rand()%100)/500.0)){
				fprintf(stderr, "Error: Failed to set track gain\n");
				exit(1);
			}
			if(!MIX_PlayTrack(click_tracks[random_order[i]], 0)){
				fprintf(stderr, "Error: Failed to play track\n");
			}
			break;
		}
	}
}

void deinit_audio(void){
	int i;

	for(i = 0; i < NUM_TRACKS; i++){
		MIX_DestroyTrack(click_tracks[i]);
		MIX_DestroyAudio(click_audios[i]);
	}

	MIX_DestroyMixer(mixer);
	MIX_Quit();
	SDL_Quit();
}

