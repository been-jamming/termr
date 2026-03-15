#include <stdlib.h>
#include <ncurses.h>
#include "state.h"

int global_attr = 0;
static struct term_state state;

void init_term_state(int width, int height){
	int x;

	state.width = width;
	state.height = height;
	state.cursor_x = 0;
	state.cursor_y = 0;
	state.characters = malloc(sizeof(chtype *)*width);
	for(x = 0; x < width; x++){
		state.characters[x] = malloc(sizeof(chtype)*height);
	}
}

void termr_scroll(){
	int x;
	int y;

	for(x = 0; x < state.width; x++){
		for(y = 1; y < state.height; y++){
			state.characters[x][y - 1] = state.characters[x][y];
		}
	}

	for(x = 0; x < state.width; x++){
		state.characters[x][state.height - 1] = ' ' | global_attr;
	}
}

void termr_advance_cursor(){
	if(state.cursor_x < state.width - 1){
		state.cursor_x++;
	} else if(state.cursor_y < state.height - 1){
		state.cursor_x = 0;
		state.cursor_y++;
	} else {
		state.cursor_x = 0;
		termr_scroll();
	}
}



void termr_addch(char c){
	state.characters[state.cursor_x][state.cursor_y] = c | global_attr;
	if(c >= 32 && c <= 126)
		termr_advance_cursor();
}

void termr_putch(char c){
	state.characters[state.cursor_x][state.cursor_y] = c | global_attr;
}

void termr_newline(){
	if(state.cursor_y < state.height - 1){
		state.cursor_x = 0;
		state.cursor_y++;
	} else {
		state.cursor_x = 0;
		termr_scroll();
	}
}

void termr_move(int y, int x){
	state.cursor_y = y;
	state.cursor_x = x;

	if(state.cursor_y < 0){
		state.cursor_y = 0;
	}

	if(state.cursor_y >= state.height){
		state.cursor_y = state.height - 1;
	}

	if(state.cursor_x < 0){
		state.cursor_x = 0;
	}

	if(state.cursor_x >= state.width){
		state.cursor_x = state.width - 1;
	}
}

void termr_getyx(int *y, int *x){
	*y = state.cursor_y;
	*x = state.cursor_x;
}

void termr_erase(){
	int x;
	int y;

	for(x = 0; x < state.width; x++){
		for(y = 0; y < state.height; y++){
			state.characters[x][y] = ' ' | global_attr;
		}
	}
}

void termr_clrtoeol(){
	int x;

	for(x = state.cursor_x; x < state.width; x++){
		state.characters[x][state.cursor_y] = ' ' | global_attr;
	}
}

void termr_refresh(){
	int x;
	int y;

	attrset(A_NORMAL);

	for(y = 0; y < state.height; y++){
		for(x = 0; x < state.width; x++){
			move(y, x);
			addch(state.characters[x][y]);
		}
	}
	move(state.cursor_y, state.cursor_x);
	refresh();
}

