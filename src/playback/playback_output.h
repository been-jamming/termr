struct player_header{
	char identifier[7];
	long num_states;
};

void init_playback_states(struct termr_playback_state state);
void write_playback_state(struct termr_playback_state state);
struct termr_playback_state read_playback_state();
void write_playback_file(FILE *file);
void read_playback_file(FILE *file);

