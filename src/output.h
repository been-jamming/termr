void termr_write_header();
void termr_write_input(char c);
void termr_write_addch(char c, int do_print);
void termr_write_set_attr(int new_attr);
void termr_write_move(int y, int x);
void termr_write_clrtoeol();
void termr_write_next_frame(int frame_count);
void termr_finish_write();

