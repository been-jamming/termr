void termr_write_header();
void termr_input(char c);
void termr_addch(char c, int do_print);
void termr_set_attr(int new_attr);
void termr_set_colors(int foreground_color, int background_color);
void termr_move(int y, int x);
void termr_next_frame(int zoom_ins, int zoom_outs, int pos_x, int pos_y, int frame_count);

