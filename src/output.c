#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include "playback/player.h"

extern FILE *output_file;
extern int global_attr;

static int frame_count = 0;

static void termr_output_frames();

void termr_write_header(){
	struct termr_header header;
	
	strcpy(header.identifier, "termr");
	header.fps = 40;
	header.frames = 0;
	header.term_size_x = 0;
	header.term_size_y = 0;

	fwrite(&header, sizeof(struct termr_header), 1, output_file);
}

void termr_input(char c){
	struct termr_update update;

	if(frame_count)
		termr_output_frames();
	update.update_type = INPUT;
	update.character = c;

	fwrite(&update, sizeof(struct termr_update), 1, output_file);
}

void termr_addch(char c, int do_print){
	chtype prev_character;
	struct termr_update update;

	if(frame_count)
		termr_output_frames();

	prev_character = inch();

	update.update_type = PRINT;
	update.character = c;
	update.prev_character = prev_character;

	fwrite(&update, sizeof(struct termr_update), 1, output_file);

	if(do_print){
		addch(c);
	}
}

void termr_set_attr(int new_attr){
	int prev_attr;
	struct termr_update update;

	if(frame_count)
		termr_output_frames();

	prev_attr = global_attr;

	update.update_type = ATTR;
	update.attr = new_attr;
	update.prev_attr = prev_attr;

	fwrite(&update, sizeof(struct termr_update), 1, output_file);

	global_attr = new_attr;
}

void termr_move(int y, int x){
	int prev_x;
	int prev_y;
	struct termr_update update;

	if(frame_count)
		termr_output_frames();

	getyx(stdscr, prev_y, prev_x);

	update.update_type = CURSOR;
	update.prev_cursor_x = prev_x;
	update.prev_cursor_y = prev_y;
	update.cursor_x = x;
	update.cursor_y = y;

	fwrite(&update, sizeof(struct termr_update), 1, output_file);

	move(y, x);
}

//This only increments a frame counter so that multiple frames where nothing happens can be merged.
void termr_next_frame(int inp_frame_count){
	frame_count += inp_frame_count;
}

static void termr_output_frames(){
	struct termr_update update;

	update.update_type = NEXT_FRAME;
	update.zoom_ins = 0;
	update.zoom_outs = 0;
	update.pos_x = 0;
	update.pos_y = 0;
	update.frame_count = frame_count;

	fwrite(&update, sizeof(struct termr_update), 1, output_file);

	frame_count = 0;
}

