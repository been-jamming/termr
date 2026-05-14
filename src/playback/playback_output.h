enum playback_action_type{
	NONE,
	FAST_FORWARD,
	SLOW_FORWARD,
	PAN,
	CUT
};

struct playback_action{
	enum playback_action_type type;
	long frame_start;
	union{
		long frame_end;
		int amount;
		struct{
			int offset_x;
			int offset_y;
		};
	};
};

struct playback_header{
	char identifier[7];
	long num_actions;
};

