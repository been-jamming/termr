#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "player.h"
#include "playback_output.h"

static struct termr_playback_state *states = NULL;
static long num_states = 0;
static long max_states = 0;
static long current_state = 0;

extern long frame;

void init_playback_states(struct termr_playback_state state){
	states = malloc(sizeof(struct termr_playback_state));
	num_states = 1;
	max_states = 1;
	current_state = 0;

	*states = state;
}

static void seek_frame(long frame){
	while(current_state < num_states - 1 && states[current_state + 1].frame <= frame){
		current_state++;
	}

	while(current_state > 0 && states[current_state].frame > frame){
		current_state--;
	}
}

static void insert_state(struct termr_playback_state state, long index){
	struct termr_playback_state *next_states;
	long next_max_states;

	if(num_states >= max_states){
		next_max_states = max_states + max_states/2 + 1;
		next_states = realloc(states, sizeof(struct termr_playback_state)*next_max_states);
		if(!next_states){
			fprintf(stderr, "Error: out of memory\n");
			exit(1);
		}
		states = next_states;
		max_states = next_max_states;
	}

	if(index < num_states - 1){
		memmove(states + index + 1, states + index, sizeof(struct termr_playback_state)*(num_states - index));
		states[index] = state;
	} else {
		states[index] = state;
	}

	num_states++;
}

void write_playback_state(struct termr_playback_state state){
	seek_frame(state.frame);

	if(state.frame == states[current_state].frame){
		states[current_state] = state;
	} else if(states[current_state].zoom  != state.zoom  || 
		  states[current_state].x     != state.x     ||
		  states[current_state].y     != state.y     ||
		  states[current_state].speed != state.speed ||
		  states[current_state].cut   != state.cut){
		insert_state(state, current_state + 1);
	}

	seek_frame(frame);
}

struct termr_playback_state read_playback_state(){
	seek_frame(frame);

	return states[current_state];
}

void write_playback_file(FILE *file){
	struct player_header header;

	strcpy(header.identifier, "termrp");
	header.num_states = num_states;

	if(fwrite(&header, sizeof(struct player_header), 1, file) < 1){
		fprintf(stderr, "Error: failed to write playback file\n");
		exit(1);
	}
	if(fwrite(states, sizeof(struct termr_playback_state), num_states, file) < num_states){
		fprintf(stderr, "Error: failed to write playback file\n");
		exit(1);
	}
}

void read_playback_file(FILE *file){
	struct player_header header;

	if(states){
		free(states);
	}

	if(fread(&header, sizeof(struct player_header), 1, file) < 1){
		fprintf(stderr, "Error: failed to read playback file\n");
		exit(1);
	}

	if(header.identifier[6] || strcmp(header.identifier, "termrp")){
		fprintf(stderr, "Error: invalid playback file format\n");
		exit(1);
	}

	num_states = header.num_states;
	max_states = num_states;
	current_state = 0;

	states = malloc(sizeof(struct termr_playback_state)*max_states);

	if(fread(states, sizeof(struct termr_playback_state), num_states, file) < num_states){
		fprintf(stderr, "Error: failed to read playback file\n");
		exit(1);
	}
}

