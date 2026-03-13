struct termr_header{
	char identifier[6];
	float fps;
	long frames;
	int term_size_x;
	int term_size_y;
};

enum termr_update_type{
	NONE,
	NEXT_FRAME,
	INPUT,
	PRINT,
	CURSOR,
	ATTR
};

struct termr_update{
	enum termr_update_type update_type;
	union{
		struct{
			int zoom_ins;
			int zoom_outs;
			int pos_x;
			int pos_y;
			int frame_count;
		};
		struct{
			chtype character;
			chtype prev_character;
		};
		struct{
			int cursor_x;
			int cursor_y;
			int prev_cursor_x;
			int prev_cursor_y;
		};
		struct{
			int attr;
			int prev_attr;
		};
	};
};

struct termr_term_state{
	int term_size_x;
	int term_size_y;
	int cursor_x;
	int cursor_y;
	int foreground;
	int background;
	char **characters;
	int **foregrounds;
	int **backgrounds;
};

