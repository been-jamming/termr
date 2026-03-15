struct termr_header{
	char identifier[6];
	float fps;
	long frames;
	int term_size_x;
	int term_size_y;
	long updates_offset;
	long num_updates;
};

enum termr_update_type{
	NONE = 0,
	NEXT_FRAME = 1,
	INPUT = 2,
	PRINT = 3,
	CURSOR = 4,
	ATTR = 5
};

enum termr_playback_state{
	PLAY,
	PAUSE
};

