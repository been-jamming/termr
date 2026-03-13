#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include "playback/player.h"

extern FILE *recording;
extern FILE *debug_file;
extern struct timespec last_time;
extern struct timespec current_time;
extern struct timespec sleep_time;
extern uint64_t last_nanoseconds;
extern uint64_t current_nanoseconds;

static uint64_t get_nanoseconds(struct timespec t){
	return 1000000000ULL*t.tv_sec + t.tv_nsec;
}

static void print_bash_char(char c){
	int y;
	int x;

	if(c == '\n'){
		getyx(stdscr, y, x);
		move(y, COLS - 1);
		printw("\n");
	} else {
		addch(c);
	}
}

int check_header(){
	struct termr_header header;

	if(fread(&header, sizeof(struct termr_header), 1, recording) < 1){
		return 1;
	}

	return strcmp(header.identifier, "termr");
}

struct termr_update next_action(){
	struct termr_update output;

	if(fread(&output, sizeof(struct termr_update), 1, recording) < 1){
		output.update_type = NONE;
	}

	return output;
}

void execute_action(struct termr_update update){
	switch(update.update_type){
		case NONE:
			break;
		case NEXT_FRAME:
			refresh();
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			last_nanoseconds = get_nanoseconds(last_time);
			current_nanoseconds = get_nanoseconds(current_time);
			if(current_nanoseconds - last_nanoseconds < 25000000ULL*update.frame_count){
				sleep_time = (struct timespec) {.tv_sec = (25000000ULL*update.frame_count - current_nanoseconds + last_nanoseconds)/1000000000ULL, .tv_nsec = (25000000ULL*update.frame_count - current_nanoseconds + last_nanoseconds)%1000000000ULL};
				nanosleep(&sleep_time, NULL);
				last_time.tv_sec = (last_nanoseconds + 25000000ULL*update.frame_count)/1000000000ULL;
				last_time.tv_nsec = (last_nanoseconds + 25000000ULL*update.frame_count)%1000000000ULL;
			} else {
				clock_gettime(CLOCK_MONOTONIC, &last_time);
			}
			break;
		case INPUT:
			break;
		case PRINT:
			print_bash_char(update.character);
			break;
		case CURSOR:
			move(update.cursor_y, update.cursor_x);
			break;
		case ATTR:
			attrset(A_NORMAL);
			attron(update.attr);
			break;
	}
}

