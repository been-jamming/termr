void fread_backwards(void *pointer, size_t size, int count, FILE *file);
int check_header(int *term_size_x, int *term_size_y);
unsigned char next_action();
unsigned char next_action_backwards();
void execute_action(unsigned char update);
void execute_action_backwards(unsigned char update_type);

