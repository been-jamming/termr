#include <stdlib.h>
#include <string.h>
#include "playback_output.h"

struct playback_action *actions = NULL;
static long num_actions = 0;
static long max_actions = 0;
static long current_action = 0;

extern long frame;

void insert_playback_action(struct playback_action action, long action_id){
	long new_max_actions;
	struct playback_action *new_actions;

	if(num_actions >= max_actions){
		new_max_actions = max_actions + max_actions/2 + 1;
		new_actions = realloc(actions, sizeof(struct playback_action)*new_max_actions);

		if(!new_actions){
			fprintf(stderr, "Error: out of memory\n");
			exit(1);
		}

		max_actions = new_max_actions;
		actions = new_actions;
	}

	if(action_id < num_actions){
		memmove(actions + action_id + 1, actions + action_id, sizeof(struct playback_action)*(num_actions - action_id));
	}

	actions[action_id] = action;

	num_actions++;
}

void write_playback_file(FILE *file){
	struct playback_header header;

	strcpy(header.identifier, "termrp");
	header.num_actions = num_actions;

	if(num_actions < fwrite(actions, sizeof(struct playback_action), num_actions, file)){
		fprintf(stderr, "Error: error while writing playback file\n");
	}
}

void read_playback_file(FILE *file){
	struct playback_header header;

	if(fread(&header, sizeof(struct playback_action), 1, file) < 1){
		fprintf(stderr, "Error: error while reading playback file\n");
		exit(1);
	}

	if(header.identifier[6] || strcmp(header.identifier, "termrp")){
		fprintf(stderr, "Error: invalid file format\n");
		exit(1);
	}

	if(actions){
		free(actions);
	}
	num_actions = header.num_actions;
	max_actions = num_actions;
	current_action = 0;
	actions = malloc(sizeof(struct playback_action)*num_actions);

	if(fread(actions, sizeof(struct playback_action), num_actions, file) < num_actions){
		fprintf(stderr, "Error: error while reading playback file\n");
		exit(1);
	}
}

