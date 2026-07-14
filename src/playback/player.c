#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>
#include <unistd.h>
#include "player.h"
#include "../read.h"
#include "../state.h"
#include "virtkeys.h"
#include "playback_output.h"
#include "audio.h"

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
unsigned char backwards = 0;
struct termr_playback_state playback_state =
	(struct termr_playback_state) {.size_x = 0, .size_y = 0, .x = 0, .y = 0, .speed = 1.0, .frame = 0, .cut = 0};

static int term_size_x;
static int term_size_y;

unsigned char recording_playback = 0;
unsigned char playing_playback_file = 0;
unsigned char skipping = 0;
unsigned char skipping_paused = 0;
FILE *termrp_file = NULL;

extern long current_update;
extern unsigned char waiting;

char *playback_file_name = NULL;
char *recording_file_name = NULL;
char *output_file_name = NULL;

static long bookmarks[256];
unsigned char bookmark_seeking = 0;
unsigned char bookmark_paused = 0;
unsigned char bookmark_backwards = 0;
long bookmark_frame;
unsigned char do_quit = 0;

double playback_speed = 1.0;

extern long num_frames;

static uint64_t get_nanoseconds(struct timespec t){
	return 1000000000ULL*t.tv_sec + t.tv_nsec;
}

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
	struct timespec ts;
	struct timespec rem;
	int prev_COLS;
	int count = 0;

	termr_set_offset(state.x, state.y);

	while(COLS != state.size_x){
		while(COLS < state.size_x){
			zoom_out();

			//Sleep for some time between each zoom
			//so that each input by the virtual key press may be distinguished
			ts.tv_sec = 0;
			//10ms
			ts.tv_nsec = 10000000;

			while(nanosleep(&ts, &rem) == -1){
				ts = rem;
			}
			termr_refresh();
			count++;
			if(count >= 100)
				break;
		}

		while(COLS > state.size_x){
			zoom_in();

			//Sleep for some time between each zoom
			//so that each input by the virtual key press may be distinguished
			ts.tv_sec = 0;
			//10ms
			ts.tv_nsec = 10000000;

			while(nanosleep(&ts, &rem) == -1){
				ts = rem;
			}
			termr_refresh();
			count++;
			if(count >= 100)
				break;
		}

		if(count >= 100)
			break;
	}
}

static void parse_arguments(int argc, char **argv){
	int opt;

	while((opt = getopt(argc, argv, "o:p:h")) != -1){
		switch(opt){
			case 'o':
				if(!optarg || !optarg[0]){
					fprintf(stderr, "Error: expected output file name after argument 'o'\n");
					exit(1);
				}
				output_file_name = optarg;
				break;
			case 'p':
				if(!optarg || !optarg[0]){
					fprintf(stderr, "Error: expected playback file name after argument 'p'\n");
					exit(1);
				}
				playback_file_name = optarg;
				break;
			case 'h':
				printf("Usage: termr_player [-o output_file] [-p playback_file] recording_file\n");
				exit(0);
				break;
		}
	}

	if(optind < argc){
		recording_file_name = argv[optind];
	} else {
		fprintf(stderr, "Error: expected recording file name\n");
		exit(1);
	}
}

int main(int argc, char **argv){
	unsigned char next_update;
	int key_press;
	int do_refresh = 0;
	struct termr_playback_state read_state;
	int prev_COLS;
	int prev_LINES;
	int do_frame = 0;
	int k;
	unsigned char dummy;

	parse_arguments(argc, argv);

	init_virtkeys();
	init_audio();
	initscr();
	prev_COLS = COLS;
	prev_LINES = LINES;
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
	
	for(k = 0; k < 256; k++){
		bookmarks[k] = -1;
	}

	if(open_recording(recording_file_name)){
		endwin();
		fprintf(stderr, "Error: could not open recording file for reading\n");
		return 1;
	}

	playback_state.size_x = COLS;
	playback_state.size_y = LINES;

	if(playback_file_name){
		termrp_file = fopen(playback_file_name, "rb");
		if(termrp_file){
			read_playback_file(termrp_file);
			fclose(termrp_file);
		} else {
			endwin();
			fprintf(stderr, "Error: failed to read playback file\n");
			exit(1);
		}
	} else {
		init_playback_states(playback_state);
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
					if(!playing_playback_file){
						if(playback_state.speed < 65536){
							playback_state.speed *= 2;
						}

						snprintf(status, 255, "Speed: %lf", playback_state.speed);
					} else {
						if(playback_speed < 65536){
							playback_speed *= 2;
						}

						snprintf(status, 255, "Playback speed: %lf", playback_speed);
					}
					break;
				case '<':
					if(!playing_playback_file){
						if(playback_state.speed > 1.0/65536){
							playback_state.speed /= 2;
						}

						snprintf(status, 255, "Speed: %lf", playback_state.speed);
					} else {
						if(playback_speed > 1.0/65536){
							playback_speed /= 2;
						}

						snprintf(status, 255, "Playback speed: %lf", playback_speed);
					}
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
				case '(':
					zoom_in();
					break;
				case ')':
					zoom_out();
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
				case KEY_RESIZE:
					playback_state.size_x = COLS;
					playback_state.size_y = LINES;
					if(COLS < prev_COLS){
						snprintf(status, 255, "Zoom in (%d, %d)", playback_state.size_x, playback_state.size_y);
					} else {
						snprintf(status, 255, "Zoom out (%d, %d)", playback_state.size_x, playback_state.size_y);
					}
					prev_COLS = COLS;
					prev_LINES = LINES;
					do_refresh = 1;
					break;
				case 'b':
					backwards = !backwards;
					if(backwards){
						snprintf(status, 255, "Backwards playback");
						current_update--;
						if(waiting){
							fread_backwards(&dummy, sizeof(unsigned char), 1, recording);
						}
					} else {
						snprintf(status, 255, "Forwards playback");
						current_update++;
						if(waiting){
							fread(&dummy, sizeof(unsigned char), 1, recording);
						}
					}
					break;
				case 'f':
					do_frame = 1;
					paused = 0;
					do_refresh = 1;
					snprintf(status, 255, "Single frame");
					break;
				case 's':
					skipping = 1;
					skipping_paused = paused;
					paused = 0;
					do_refresh = 1;
					snprintf(status, 255, "Skipping");
					break;
				case 'q':
					snprintf(status, 255, "Enter key for bookmark");
					termr_refresh();
					display_status();
					nodelay(stdscr, 0);
					key_press = getch()%256;
					nodelay(stdscr, 1);
					if(key_press >= ' ' && key_press < '~'){
						snprintf(status, 255, "Bookmark set for '%c'", key_press);
					} else {
						snprintf(status, 255, "Bookmark set for %02X", key_press);
					}
					do_refresh = 1;
					bookmarks[key_press] = frame;
					break;
				case '\t':
					if(playing_playback_file)
						break;
					bookmark_paused = paused;
					bookmark_backwards = backwards;
					snprintf(status, 255, "Enter bookmark to seek");
					termr_refresh();
					display_status();
					nodelay(stdscr, 0);
					key_press = getch()%256;
					nodelay(stdscr, 1);
					if(bookmarks[key_press] >= 0 && bookmarks[key_press] > frame){
						backwards = 0;
						bookmark_seeking = 1;
						paused = 0;
						bookmark_frame = bookmarks[key_press];
					} else if(bookmarks[key_press] >= 0 && bookmarks[key_press] < frame){
						backwards = 1;
						bookmark_seeking = 1;
						paused = 0;
						bookmark_frame = bookmarks[key_press];
					}

					if(backwards && !bookmark_backwards){
						current_update--;
						if(waiting){
							fread_backwards(&dummy, sizeof(unsigned char), 1, recording);
						}
					} else if(!backwards && bookmark_backwards){
						current_update++;
						if(waiting){
							fread(&dummy, sizeof(unsigned char), 1, recording);
						}
					}
					do_refresh = 1;
					break;
				case 'Q':
					do_quit = 1;
					break;
			}
		}

		playback_state.frame = frame;


		if(backwards && frame == 1){
			paused = 1;
			snprintf(status, 255, "Start");
		} else if(!backwards && frame == num_frames - 1){
			paused = 1;
			snprintf(status, 255, "End");
		}

		if(paused){
			if(do_refresh){
				termr_refresh();
				do_refresh = 0;
			}
			display_status();
			//Sleep for a frame
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
		} else {
			if(recording_playback && !bookmark_seeking){
				write_playback_state(playback_state);
			}

			if(!playing_playback_file){
				playback_speed = 1.0;
			}

			if(playing_playback_file && !skipping && !bookmark_seeking){
				read_state = read_playback_state();
				if(!read_state.cut){
					apply_state_changes(read_state, playback_state);
				}
				playback_state = read_state;
			}

			if(!backwards && frame < num_frames){
				next_update = next_action();
				execute_action(next_update);
			}
			if(backwards && frame > 0){
				execute_action_backwards(next_update);
				next_update = next_action_backwards();
			}

			if(skipping || (!skipping && skipping_paused))
				do_refresh = 1;

			if(do_refresh && (!playing_playback_file || !playback_state.cut)){
				termr_refresh();
			}

			if(do_frame){
				do_frame = 0;
				paused = 1;
			}

			if(!skipping && skipping_paused){
				paused = 1;
				skipping_paused = 0;
			}

			if(bookmark_seeking && frame == bookmark_frame){
				bookmark_seeking = 0;

				if(bookmark_backwards && !backwards){
					current_update--;
					if(waiting){
						fread_backwards(&dummy, sizeof(unsigned char), 1, recording);
					}
				} else if(!bookmark_backwards && backwards){
					current_update++;
					if(waiting){
						fread(&dummy, sizeof(unsigned char), 1, recording);
					}
				}

				backwards = bookmark_backwards;
				paused = bookmark_paused;
			}
		}

		if(do_quit)
			break;
	} while(1);

	if(output_file_name){
		termrp_file = fopen(output_file_name, "wb");
		if(termrp_file){
			write_playback_file(termrp_file);
			fclose(termrp_file);
		} else {
			endwin();
			fprintf(stderr, "Error: failed to write playback file\n");
			exit(1);
		}
	}

	fclose(debug_file);
	endwin();
	fclose(recording);
	deinit_audio();
	deinit_virtkeys();
}

