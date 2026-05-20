#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include "player.h"
#include "../read.h"
#include "../state.h"
#include "virtkeys.h"
#include "playback_output.h"

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

extern int global_attr;
extern long frame;

static char status[256] = {0};

unsigned char paused = 0;
struct termr_playback_state playback_state =
	(struct termr_playback_state) {.zoom = 0, .x = 0, .y = 0, .speed = 1.0, .frame = 0, .cut = 0};

static int term_size_x;
static int term_size_y;

unsigned char recording_playback = 0;
unsigned char playing_playback_file = 0;
FILE *termrp_file = NULL;

static int open_recording(char *filename){
	recording = fopen(filename, "rb");

	return recording == NULL;
}

void display_status(){
	int last_x;
	int last_y;
	int x;
	int color;
	int last_attr;
	char *status_byte;

	global_foreground_color = COLOR_WHITE;
	global_background_color = COLOR_BLACK;
	color = get_global_color();
	attrset(A_STANDOUT | color);

	termr_getyx(&last_y, &last_x);

	status_byte = status;

	for(x = 0; x < COLS; x++){
		move(LINES - 1, x);
		if(*status_byte){
			addch(*status_byte);
			status_byte++;
		} else {
			addch(' ');
		}
	}

	move(last_y, last_x);

	refresh();
}

void apply_state_changes(struct termr_playback_state state, struct termr_playback_state prev_state){
	int zoom;

	termr_set_offset(state.x, state.y);

	for(zoom = state.zoom; zoom < prev_state.zoom; zoom++){
		zoom_out();
	}

	for(zoom = prev_state.zoom; zoom < state.zoom; zoom++){
		zoom_in();
	}
}

int main(int argc, char **argv){
	unsigned char next_update;
	int key_press;
	int do_refresh = 0;
	struct termr_playback_state read_state;

	init_virtkeys();
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

	if(open_recording("test")){
		endwin();
		fprintf(stderr, "Error: could not open file for reading\n");
		return 1;
	}

	if(argc <= 1){
		init_playback_states(playback_state);
	} else {
		termrp_file = fopen("test.termrp", "rb");
		read_playback_file(termrp_file);
		fclose(termrp_file);
	}

	if(check_header(&term_size_x, &term_size_y)){
		endwin();
		fprintf(stderr, "Error: invalid file format\n");
		return 1;
	}

	noecho();
	nodelay(stdscr, 1);
	keypad(stdscr, TRUE);
	setscrreg(0, 0);
	scrollok(stdscr, 0);
	create_color_pairs(5);
	init_term_state(term_size_x, term_size_y);
	termr_erase();
	global_foreground_color = COLOR_WHITE;
	global_background_color = COLOR_BLACK;
	bkgd(get_global_color());
	termr_erase();

	curs_set(1);
	clock_gettime(CLOCK_MONOTONIC, &last_time);

	debug_file = fopen("debug.txt", "w");
	clock_gettime(CLOCK_MONOTONIC, &last_time);
	paused = 0;

	do{
		do_refresh = 0;
		while((key_press = getch()) != ERR){
			switch(key_press){
				case ' ':
					if(!paused){
						paused = 1;
						strcpy(status, "Pause");
					} else {
						paused = 0;
						strcpy(status, "Unpause");
					}
					do_refresh = 1;
					break;
				case '>':
					if(playback_state.speed < 65536){
						playback_state.speed *= 2;
					}

					snprintf(status, 255, "Speed: %lf", playback_state.speed);
					break;
				case '<':
					if(playback_state.speed > 1.0/65536){
						playback_state.speed /= 2;
					}

					snprintf(status, 255, "Speed: %lf", playback_state.speed);
					break;
				case KEY_LEFT:
					if(playback_state.x > 0){
						playback_state.x--;
					}
					termr_set_offset(playback_state.x, playback_state.y);
					do_refresh = 1;
					snprintf(status, 255, "Move");
					break;
				case KEY_RIGHT:
					playback_state.x++;
					termr_set_offset(playback_state.x, playback_state.y);
					do_refresh = 1;
					snprintf(status, 255, "Move");
					break;
				case KEY_UP:
					if(playback_state.y > 0){
						playback_state.y--;
					}
					termr_set_offset(playback_state.x, playback_state.y);
					do_refresh = 1;
					snprintf(status, 255, "Move");
					break;
				case KEY_DOWN:
					playback_state.y++;
					termr_set_offset(playback_state.x, playback_state.y);
					do_refresh = 1;
					snprintf(status, 255, "Move");
					break;
				case '+':
					zoom_in();
					playback_state.zoom++;
					do_refresh = 1;
					snprintf(status, 255, "Zoom in %d", playback_state.zoom);
					break;
				case '-':
					zoom_out();
					playback_state.zoom--;
					do_refresh = 1;
					snprintf(status, 255, "Zoom out %d", playback_state.zoom);
					break;
				case 'p':
					playing_playback_file = !playing_playback_file;
					if(playing_playback_file){
						snprintf(status, 255, "Playing from playback file");
					} else {
						snprintf(status, 255, "Not playing from playback file");
					}
					break;
				case 'c':
					playback_state.cut = !playback_state.cut;
					if(playback_state.cut){
						snprintf(status, 255, "Cut");
					} else {
						snprintf(status, 255, "End cut");
					}
					break;
				case 'r':
					recording_playback = !recording_playback;
					if(recording_playback){
						snprintf(status, 255, "Recording");
					} else {
						snprintf(status, 255, "End recording");
					}
					break;
			}
		}

		playback_state.frame = frame;

		if(recording_playback){
			write_playback_state(playback_state);
		}

		if(playing_playback_file){
			read_state = read_playback_state();
			apply_state_changes(read_state, playback_state);
			playback_state = read_state;
		}

		if(paused){
			display_status();
		}

		next_update = next_action();
		execute_action(next_update);

		if(do_refresh){
			termr_refresh();
		}
	} while(next_update != NONE);

	termrp_file = fopen("test.termrp", "wb");
	write_playback_file(termrp_file);
	fclose(termrp_file);

	fclose(debug_file);
	endwin();
	fclose(recording);
	deinit_virtkeys();
}
