#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include <math.h>
#include "playback/player.h"
#include "playback/audio.h"
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
long current_update = 0;

extern unsigned char paused;
extern unsigned char playing_playback_file;
extern unsigned char skipping;
extern struct termr_playback_state playback_state;

long frame = 0;
static long frame_start = 0;
static long duration = 0;
unsigned char waiting = 0;

extern unsigned char bookmark_seeking;
extern unsigned char bookmark_paused;
extern unsigned char bookmark_backwards;
extern long bookmark_frame;
extern unsigned char backwards;

long num_frames;

static uint64_t get_nanoseconds(struct timespec t){
	return 1000000000ULL*t.tv_sec + t.tv_nsec;
}

static void print_bash_char(char c){
	int y;
	int x;

	termr_addch(c);
}

void fread_backwards(void *pointer, size_t size, int count, FILE *file){
	fseek(file, -count*size, SEEK_CUR);
	fread(pointer, size, count, file);
	fseek(file, -count*size, SEEK_CUR);
}

int check_header(int *term_size_x, int *term_size_y){
	struct termr_header header;
	long offset;

	if(fread(&header, sizeof(struct termr_header), 1, recording) < 1){
		return 1;
	}

	if(header.identifier[5] || strcmp(header.identifier, "termr")){
		return 1;
	}

	num_frames = header.frames;

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

	return 0;
}

unsigned char next_action(){
	unsigned char output;

	if(current_update < num_updates && !waiting){
		output = updates[current_update];
	} else if(waiting){
		output = updates[current_update - 1];
	} else {
		output = NONE;
	}
	//if(output != NEXT_FRAME && current_update < num_updates && !paused)
	if(!waiting && output != NONE)
		current_update++;

	return output;
}

unsigned char next_action_backwards(){
	unsigned char output;

	if(current_update > 0 && !waiting){
		output = updates[current_update - 1];
	} else if(waiting){
		output = updates[current_update];
	} else {
		output = NONE;
	}
	//if(output != NEXT_FRAME && current_update > 0 && !paused){
	if(!waiting && output != NONE)
		current_update--;
	//}

	return output;
}

void execute_action(unsigned char update_type){
	int frame_count = 0;
	unsigned char frame_count_char;
	signed char char_diff;
	signed char prev_char;
	chtype prev_chtype;
	chtype all_diff;
	chtype next_chtype;
	char character;
	int prev_x;
	int prev_y;
	short cursor_x_diff;
	short cursor_y_diff;
	short cursor_x;
	short cursor_y;
	int attr_diff;

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

				if((!playback_state.cut || !playing_playback_file) && !skipping)
					termr_refresh();
				waiting = 1;
			}
			break;
		case INPUT:
			fread(&character, sizeof(char), 1, recording);
			play_click();
			break;
		case PRINT:
			fread(&char_diff, sizeof(signed char), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			prev_chtype = termr_mvinch(prev_y, prev_x);
			prev_char = prev_chtype&0x7F;
			character = prev_char + char_diff;
			global_attr = prev_chtype&~0x7F;
			print_bash_char(character);
			skipping = 0;
			break;
		case PRINT_ATTR:
			fread(&all_diff, sizeof(chtype), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			prev_chtype = termr_mvinch(prev_y, prev_x);
			next_chtype = prev_chtype + all_diff;
			character = next_chtype&0x7F;
			global_attr = next_chtype&~0x7F;
			print_bash_char(character);
			skipping = 0;
			break;
		case CURSOR:
			fread(&cursor_x_diff, sizeof(short), 1, recording);
			fread(&cursor_y_diff, sizeof(short), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			cursor_x = prev_x + cursor_x_diff;
			cursor_y = prev_y + cursor_y_diff;
			termr_move(cursor_y, cursor_x);
			break;
	}

	if(waiting){
		if((!playback_state.cut || !playing_playback_file) && !skipping && !bookmark_seeking){
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			last_nanoseconds = get_nanoseconds(last_time);
			current_nanoseconds = get_nanoseconds(current_time);
			if(current_nanoseconds - last_nanoseconds < 25000000ULL/playback_state.speed){
				sleep_time = (struct timespec) {.tv_sec = (25000000ULL/playback_state.speed - current_nanoseconds + last_nanoseconds)/1000000000ULL, .tv_nsec = (long long unsigned int) (25000000ULL/playback_state.speed - current_nanoseconds + last_nanoseconds)%1000000000ULL};
				nanosleep(&sleep_time, NULL);
				last_time.tv_sec = (last_nanoseconds + 25000000ULL/playback_state.speed)/1000000000ULL;
				last_time.tv_nsec = (long long unsigned int) (last_nanoseconds + 25000000ULL/playback_state.speed)%1000000000ULL;
			} else {
				clock_gettime(CLOCK_MONOTONIC, &last_time);
			}
		}

		frame++;
		if(frame - frame_start >= duration){
			waiting = 0;
		}
	}
}

void execute_action_backwards(unsigned char update_type){
	int frame_count = 0;
	unsigned char frame_count_char;
	signed char char_diff;
	signed char prev_char;
	chtype prev_chtype;
	chtype all_diff;
	chtype next_chtype;
	char character;
	int prev_x;
	int prev_y;
	short cursor_x_diff;
	short cursor_y_diff;
	short cursor_x;
	short cursor_y;
	int attr_diff;
	int width;
	int height;

	switch(update_type){
		case NONE:
			break;
		case NEXT_FRAME:
			if(!waiting){
				fread_backwards(&frame_count_char, sizeof(unsigned char), 1, recording);
				duration = frame_count_char;
				if(duration == 0)
					duration = 256;
				frame_start = frame - duration;

				if((!playback_state.cut || !playing_playback_file) && !skipping)
					termr_refresh();
				waiting = 1;
			}
			break;
		case INPUT:
			fread_backwards(&character, sizeof(char), 1, recording);
			break;
		case PRINT:
			fread_backwards(&char_diff, sizeof(signed char), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			termr_size(&width, &height);
			if(prev_x == 0){
				prev_y--;
				prev_x = width - 1;
			} else {
				prev_x--;
			}
			termr_move(prev_y, prev_x);
			prev_chtype = termr_mvinch(prev_y, prev_x);
			prev_char = prev_chtype&0x7F;
			character = prev_char - char_diff;
			global_attr = prev_chtype&~0x7F;
			termr_putch(character);
			skipping = 0;
			break;
		case PRINT_ATTR:
			fread_backwards(&all_diff, sizeof(chtype), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			termr_size(&width, &height);
			if(prev_x == 0){
				prev_y--;
				prev_x = width - 1;
			} else {
				prev_x--;
			}
			termr_move(prev_y, prev_x);
			prev_chtype = termr_mvinch(prev_y, prev_x);
			next_chtype = prev_chtype - all_diff;
			character = next_chtype&0x7F;
			global_attr = next_chtype&~0x7F;
			termr_putch(character);
			skipping = 0;
			break;
		case CURSOR:
			fread_backwards(&cursor_y_diff, sizeof(short), 1, recording);
			fread_backwards(&cursor_x_diff, sizeof(short), 1, recording);
			termr_getyx(&prev_y, &prev_x);
			cursor_x = prev_x - cursor_x_diff;
			cursor_y = prev_y - cursor_y_diff;
			termr_move(cursor_y, cursor_x);
			break;
	}

	if(waiting){
		if((!playback_state.cut || !playing_playback_file) && !skipping && !bookmark_seeking){
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			last_nanoseconds = get_nanoseconds(last_time);
			current_nanoseconds = get_nanoseconds(current_time);
			if(current_nanoseconds - last_nanoseconds < 25000000ULL/playback_state.speed){
				sleep_time = (struct timespec) {.tv_sec = (25000000ULL/playback_state.speed - current_nanoseconds + last_nanoseconds)/1000000000ULL, .tv_nsec = (long long unsigned int) (25000000ULL/playback_state.speed - current_nanoseconds + last_nanoseconds)%1000000000ULL};
				nanosleep(&sleep_time, NULL);
				last_time.tv_sec = (last_nanoseconds + 25000000ULL/playback_state.speed)/1000000000ULL;
				last_time.tv_nsec = (long long unsigned int) (last_nanoseconds + 25000000ULL/playback_state.speed)%1000000000ULL;
			} else {
				clock_gettime(CLOCK_MONOTONIC, &last_time);
			}
		}

		frame--;
		if(frame <= frame_start){
			waiting = 0;
		}
	}
}
