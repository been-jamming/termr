#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include "player.h"
#include "../read.h"
#include "../state.h"

FILE *recording;
static struct termr_header header;
FILE *output_file = NULL;
FILE *debug_file;

extern int global_foreground_color;
extern int global_background_color;

void create_color_pairs(int pairs_start);
int get_global_color();

int red_background;
int yellow_background;
int green_background;

struct timespec last_time;
struct timespec current_time;
struct timespec sleep_time;
uint64_t last_nanoseconds;
uint64_t current_nanoseconds;

static int open_recording(char *filename){
	recording = fopen(filename, "rb");

	return recording == NULL;
}

int main(int argc, char **argv){
	unsigned char next_update;

	initscr();
	if(!has_colors()){
		endwin();
		fprintf(stderr, "Error: the terminal does not support colors\n");
		return 1;
	}

	cbreak();
	start_color();
	if(COLORS >= 256){
		init_pair(1, COLOR_WHITE, COLOR_BLACK);
		init_pair(2, COLOR_WHITE, 52);
		init_pair(3, COLOR_WHITE, 58);
		init_pair(4, COLOR_WHITE, 22);
		red_background = 52;
		yellow_background = 58;
		green_background = 22;
	} else {
		init_pair(1, COLOR_WHITE, COLOR_BLACK);
		init_pair(2, COLOR_WHITE, COLOR_RED);
		init_pair(3, COLOR_WHITE, COLOR_YELLOW);
		init_pair(4, COLOR_WHITE, COLOR_GREEN);
		red_background = COLOR_RED;
		yellow_background = COLOR_YELLOW;
		green_background = COLOR_GREEN;
	}
	noecho();
	nodelay(stdscr, 1);
	setscrreg(0, 0);
	scrollok(stdscr, 0);
	create_color_pairs(5);
	init_term_state(COLS, LINES);
	termr_erase();
	global_foreground_color = COLOR_WHITE;
	global_background_color = COLOR_BLACK;
	bkgd(get_global_color());
	termr_erase();

	curs_set(1);
	clock_gettime(CLOCK_MONOTONIC, &last_time);

	if(open_recording("test")){
		endwin();
		fprintf(stderr, "Error: could not open file for reading\n");
		return 1;
	}

	if(check_header()){
		endwin();
		fprintf(stderr, "Error: invalid file format\n");
		return 1;
	}

	debug_file = fopen("debug.txt", "w");
	clock_gettime(CLOCK_MONOTONIC, &last_time);

	do{
		next_update = next_action();
		//fprintf(debug_file, "%d\n", next_update);
		execute_action(next_update);
	} while(next_update != NONE);

	fclose(debug_file);
	endwin();
	fclose(recording);
}
