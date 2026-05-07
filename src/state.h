struct term_state{
	int width;
	int height;
	int cursor_x;
	int cursor_y;
	int offset_x;
	int offset_y;
	chtype **characters;
};

void init_term_state(int width, int height);
void termr_scroll();
void termr_advance_cursor();
void termr_addch(char c);
void termr_putch(char c);
void termr_newline();
void termr_move(int y, int x);
void termr_getyx(int *y, int *x);
void termr_erase();
void termr_clrtoeol();
void termr_refresh();
void termr_set_offset(int offset_x, int offset_y);

