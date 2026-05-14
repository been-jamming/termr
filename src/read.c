#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include <math.h>
#include "playback/player.h"
#include "state.h"

extern FILE *recording;
extern FILE *debug_file;
extern struct timespec last_time;
extern struct timespec current_time;
extern struct timespec sleep_time;
extern uint64_t last_nanoseconds;
extern uint64_t current_nanoseconds;
extern int global_attr;

static long updates_offset;
static long num_updates;
static unsigned char *updates;
static long current_update = 0;

extern enum termr_playback_state playback_state;

extern double playback_speed;

long frame = 0;
static long frame_start = 0;
static long duration = 0;
unsigned char waiting = 0;

static uint64_t get_nanoseconds(struct timespec t){
	return 1000000000ULL*t.tv_sec + t.tv_nsec;
}

static void print_bash_char(char c){
	int y;
	int x;

	if(c == '\n'){
		termr_newline();
	} else {
		termr_addch(c);
	}
}

int check_header(int *term_size_x, int *term_size_y){
	struct termr_header header;
	long offset;

	if(fread(&header, sizeof(struct termr_header), 1, recording) < 1){
		return 1;
	}

	updates_offset = header.updates_offset;
	num_updates = header.num_updates;

	offset = ftell(recording);

	updates = malloc(sizeof(unsigned char)*num_updates);
	fseek(recording, updates_offset, SEEK_SET);
	if(fread(updates, sizeof(unsigned char), num_updates, recording) < num_updates){
		return 1;
	}

	fseek(recording, offset, SEEK_SET);

	*term_size_x = header.term_size_x;
	*term_size_y = header.term_size_y;

	return header.identifier[5] || strcmp(header.identifier, "termr");
}

unsigned char next_action(){
	unsigned char output;

	if(current_update < num_updates)
		output = updates[current_update];
	else
		output = NONE;
	if(output != NEXT_FRAME && current_update < num_updates && playback_state == PLAY)
		current_update++;

	return output;
}

void execute_action(unsigned char update_type){
	int frame_count = 0;
	unsigned char frame_count_char;
	char character = '\0';
	short cursor_x;
	short cursor_y;
	int attr;

	if(playback_state == PLAY){
		switch(update_type){
			case NONE:
				break;
			case NEXT_FRAME:
				if(!waiting){
					fread(&frame_count_char, sizeof(unsigned char), 1, recording);
					duration = frame_count_char;
					if(duration == 0)
						duration = 256;
					frame_start = frame;

					termr_refresh();
					waiting = 1;
				}
				break;
			case INPUT:
				break;
			case PRINT:
				fread(&character, sizeof(char), 1, recording);
				print_bash_char(character);
				break;
			case CURSOR:
				fread(&cursor_x, sizeof(short), 1, recording);
				fread(&cursor_y, sizeof(short), 1, recording);
				termr_move(cursor_y, cursor_x);
				break;
			case ATTR:
				fread(&attr, sizeof(int), 1, recording);
				global_attr = attr;
				break;
		}
	} else {
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		last_nanoseconds = get_nanoseconds(last_time);
		current_nanoseconds = get_nanoseconds(current_time);
		if(current_nanoseconds - last_nanoseconds < 25000000ULL){
			sleep_time = (struct timespec) {.tv_sec = (25000000ULL - current_nanoseconds + last_nanoseconds)/1000000000ULL, .tv_nsec = (25000000ULL - current_nanoseconds + last_nanoseconds)%1000000000ULL};
			nanosleep(&sleep_time, NULL);
			last_time.tv_sec = (last_nanoseconds + 25000000ULL)/1000000000ULL;
			last_time.tv_nsec = (last_nanoseconds + 25000000ULL)%1000000000ULL;
		} else {
			clock_gettime(CLOCK_MONOTONIC, &last_time);
		}
	}

	if(playback_state == PLAY && waiting){
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		last_nanoseconds = get_nanoseconds(last_time);
		current_nanoseconds = get_nanoseconds(current_time);
		if(current_nanoseconds - last_nanoseconds < 25000000ULL/playback_speed){
			sleep_time = (struct timespec) {.tv_sec = (25000000ULL/playback_speed - current_nanoseconds + last_nanoseconds)/1000000000ULL, .tv_nsec = (long long unsigned int) (25000000ULL/playback_speed - current_nanoseconds + last_nanoseconds)%1000000000ULL};
			nanosleep(&sleep_time, NULL);
			last_time.tv_sec = (last_nanoseconds + 25000000ULL/playback_speed)/1000000000ULL;
			last_time.tv_nsec = (long long unsigned int) (last_nanoseconds + 25000000ULL/playback_speed)%1000000000ULL;
		} else {
			clock_gettime(CLOCK_MONOTONIC, &last_time);
		}

		frame++;
		if(frame - frame_start >= duration){
			waiting = 0;
			current_update++;
		}
	}
}

