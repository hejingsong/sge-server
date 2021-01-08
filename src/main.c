#include "core/sge.h"
#include "core/log.h"
#include "os/server.h"

#include "python-src/env.h"

int main(int argc, char const *argv[]) {
	if (argc < 2) {
		ERROR("%s config_file", argv[0]);
		return SGE_ERR;
	}

	sge_config config = {
		.workdir = NULL,
		.logfile = NULL,
		.entry_file = NULL,
		.entry_func = NULL,
		.socket = NULL,
		.user = NULL,
		.cb = NULL,
		.daemon = 0
	};

	if (init_env() == SGE_ERR) {
		return -1;
	}

	if (load_config(argv[1], &config) == SGE_ERR) {
		return -1;
	}

	if (start_server(&config) == SGE_ERR) {
		return -1;
	}

	destroy_server();
	destroy_env();

	return 0;
}
