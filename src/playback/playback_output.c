#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "player.h"
#include "playback_output.h"

static struct termr_playback_state playback_state =
	(struct termr_playback_state) {.size_x = 0, .size_y = 0, .x = 0, .y = 0, .speed = 1.0, .frame = 0, .cut = 0};
static struct termr_playback_state *states = NULL;
static long num_states = 0;
static long max_states = 0;
static long current_state = 0;

extern long frame;
//long frame;

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

	if(index < num_states){
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
	} else if(states[current_state].size_x != state.size_x || 
		  states[current_state].size_y != state.size_y ||
		  states[current_state].x      != state.x      ||
		  states[current_state].y      != state.y      ||
		  states[current_state].speed  != state.speed  ||
		  states[current_state].cut    != state.cut){
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

static void delete_playback_state(long index){
	if(index < num_states - 1){
		memmove(states + index, states + index + 1, sizeof(struct termr_playback_state)*(num_states - index - 1));
	}

	num_states--;
}

/*

long get_bad_playback_state(void){
	int k;

	for(k = 0; k < num_states; k++){
		if(states[k].frame == 0){
			return k;
		}
	}

	return -1;
}

static void print_playback_state(struct termr_playback_state playback_state){
	printf("size x: %d\n", playback_state.size_x);
	printf("size y: %d\n", playback_state.size_y);
	printf("x: %d\n", playback_state.x);
	printf("y: %d\n", playback_state.y);
	printf("speed: %lf\n", playback_state.speed);
	printf("frame: %ld\n", playback_state.frame);
	printf("cut: %d\n", (int) (playback_state.cut));
	printf("------------\n");
}

static int sort_compare(const void *ptr0, const void *ptr1){
	const struct termr_playback_state *playback_state0, *playback_state1;

	playback_state0 = ptr0;
	playback_state1 = ptr1;

	return playback_state0->frame - playback_state1->frame;
}

int main(int argc, char **argv){
	char *file_name;
	FILE *file;
	long bad_index;

	file_name = argv[1];

	file = fopen(file_name, "rb");

	init_playback_states(playback_state);
	read_playback_file(file);
	fclose(file);

	qsort(states, num_states, sizeof(playback_state), sort_compare);
	
	while((bad_index = get_bad_playback_state()) >= 0){
		delete_playback_state(bad_index);
	}

	file_name = argv[2];
	file = fopen(file_name, "wb");
	write_playback_file(file);
	fclose(file);

	return 0;
}

*/
