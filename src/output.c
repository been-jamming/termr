#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include "playback/player.h"
#include "state.h"
#include "output.h"

extern FILE *output_file;
extern int global_attr;

static int frame_count = 0;
static int total_frame_count = 0;

static void termr_output_frames();

unsigned char *update_types = NULL;
static long num_updates = 0;
static long max_updates = 0;

static void append_update_type(unsigned char update_type){
	unsigned char *realloced;

	if(num_updates >= max_updates){
		max_updates = max_updates + max_updates/2 + 1;
		realloced = realloc(update_types, sizeof(unsigned char)*max_updates);
		if(!realloced){
			fprintf(stderr, "Error: out of memory");
			exit(1);
		}
		update_types = realloced;
	}

	//Now max_updates is guaranteed to be larger than num_updates unless long overflows
	update_types[num_updates] = update_type;
	num_updates++;
}

void termr_write_header(){
	struct termr_header header;
	
	strcpy(header.identifier, "termr");
	header.fps = 40;
	header.frames = 0;
	header.term_size_x = 0;
	header.term_size_y = 0;
	header.updates_offset = 0;
	header.num_updates = 0;

	fwrite(&header, sizeof(struct termr_header), 1, output_file);
}

void termr_write_input(char c){
	if(frame_count)
		termr_output_frames();
	fwrite(&c, sizeof(char), 1, output_file);
	append_update_type(INPUT);
}

void termr_write_addch(char c, int do_print){
	int prev_x;
	int prev_y;
	chtype prev_char;
	signed char diff;
	chtype attr_diff;
	chtype all_diff;

	//Only do anything if c is a visible character
	if(c >= ' ' && c <= '~'){
		termr_getyx(&prev_y, &prev_x);
		prev_char = termr_mvinch(prev_y, prev_x);
		diff = (signed char) c - (signed char) (prev_char&0x7F);
		attr_diff = global_attr - prev_char&~0x7F;
		all_diff = ((chtype) c | global_attr) - prev_char;

		if(frame_count)
			termr_output_frames();

		if(prev_y >= LINES - 1 && prev_x >= COLS - 1){
			//We need to manually scroll the terminal and make sure the operation is recorded.
			//This ensures that printing a character is reversible.
			termr_write_scroll();
			termr_write_move(LINES - 2, COLS - 1);
		}

		if(attr_diff == 0){
			//If the character has the same attribute, use the PRINT update type
			fwrite(&diff, sizeof(char), 1, output_file);
			append_update_type(PRINT);
		} else {
			//If the character has different attributes, use the PRINT_ATTR update type
			fwrite(&all_diff, sizeof(chtype), 1, output_file);
			append_update_type(PRINT_ATTR);
		}

		if(do_print){
			termr_addch(c);
		}
	}
}

void termr_write_move(short y, short x){
	int prev_x;
	int prev_y;
	short diff_x;
	short diff_y;

	termr_getyx(&prev_y, &prev_x);
	diff_x = x - prev_x;
	diff_y = y - prev_y;

	if(frame_count)
		termr_output_frames();
	fwrite(&diff_x, sizeof(short), 1, output_file);
	fwrite(&diff_y, sizeof(short), 1, output_file);
	append_update_type(CURSOR);
	termr_move(y, x);
}

void termr_write_scroll(){
	int prev_x, x;
	int prev_y, y;
	chtype c;
	int prev_global_attr;

	termr_getyx(&prev_y, &prev_x);
	prev_global_attr = global_attr;

	termr_write_move(0, 0);

	for(y = 1; y < LINES - 1; y++){
		for(x = 0; x < COLS; x++){
			c = termr_mvinch(y, x);
			global_attr = c&~0xFF;
			//termr_write_set_attr(c&~0xFF);
			termr_write_addch(c&0xFF, 1);
		}
	}

	for(x = 0; x < COLS - 1; x++){
		c = termr_mvinch(y, x);
		global_attr = c&~0xFF;
		//termr_write_set_attr(c&~0xFF);
		termr_write_addch(c&0xFF, 1);
	}

	global_attr = prev_global_attr;
	//termr_write_set_attr(prev_global_attr);
	for(x = 0; x < COLS; x++){
		termr_write_addch(' ', 1);
	}

	termr_write_move(prev_y, prev_x);
}

void termr_write_advance_cursor(){
	int prev_x;
	int prev_y;

	termr_getyx(&prev_y, &prev_x);

	if(prev_x < COLS - 1){
		termr_write_move(prev_y, prev_x + 1);
	} else if(prev_y < LINES - 1){
		termr_write_move(prev_y + 1, 0);
	} else {
		termr_write_move(prev_y, 0);
		termr_write_scroll();
	}
}

void termr_write_newline(){
	int prev_x;
	int prev_y;

	termr_getyx(&prev_y, &prev_x);

	if(prev_y < LINES - 1){
		termr_write_move(prev_y + 1, 0);
	} else {
		termr_write_move(prev_y, 0);
		termr_write_scroll();
	}
}

//I don't believe this functions is used.
//besides, the implementation is wrong.
//can't go to end of line if on the last line in the terminal
void termr_write_clrtoeol(){
	int cursor_x;
	int cursor_y;
	int x;

	termr_getyx(&cursor_y, &cursor_x);

	for(x = cursor_x; x < COLS; x++){
		termr_write_addch(' ', 1);
	}

	termr_write_move(cursor_y, cursor_x);
}

//This only increments a frame counter so that multiple frames where nothing happens can be merged.
void termr_write_next_frame(int inp_frame_count){
	total_frame_count += inp_frame_count;
	frame_count += inp_frame_count;

	while(frame_count >= 256){
		termr_output_frames();
	}
}

static void termr_output_frames(){
	unsigned char frames;

	frames = frame_count%256;
	fwrite(&frames, sizeof(unsigned char), 1, output_file);
	append_update_type(NEXT_FRAME);

	if(frames)
		frame_count -= frames;
	else
		frame_count -= 256;
}

void termr_finish_write(){
	long updates_offset;
	struct termr_header header;
	
	updates_offset = ftell(output_file);

	fwrite(update_types, sizeof(unsigned char), num_updates, output_file);

	rewind(output_file);

	strcpy(header.identifier, "termr");
	header.fps = 40;
	header.frames = total_frame_count;
	header.term_size_x = COLS;
	header.term_size_y = LINES;
	header.updates_offset = updates_offset;
	header.num_updates = num_updates;

	fwrite(&header, sizeof(struct termr_header), 1, output_file);
}

