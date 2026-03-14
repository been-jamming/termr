#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include "playback/player.h"
#include "state.h"

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
	if(frame_count)
		termr_output_frames();
	fwrite(&c, sizeof(char), 1, output_file);
	append_update_type(PRINT);
	if(do_print){
		termr_addch(c);
	}
}

void termr_write_set_attr(int new_attr){
	if(frame_count)
		termr_output_frames();
	fwrite(&new_attr, sizeof(int), 1, output_file);
	append_update_type(ATTR);
	global_attr = new_attr;
}

void termr_write_move(short y, short x){
	if(frame_count)
		termr_output_frames();
	fwrite(&x, sizeof(short), 1, output_file);
	fwrite(&y, sizeof(short), 1, output_file);
	append_update_type(CURSOR);
	termr_move(y, x);
}

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
	header.term_size_x = 0;
	header.term_size_y = 0;
	header.updates_offset = updates_offset;
	header.num_updates = num_updates;

	fwrite(&header, sizeof(struct termr_header), 1, output_file);
}

