#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "simple_sio.h"

int n_buflen = 512;
int n_speed = 115200;

void
usage()
{
	printf(
"Usage: this [-s speed] [-B buflen] [-bdh] (dev)\n"
"\t-s: speed (default: %d)\n"
"\t-B: buflen (default: %d)\n"
"\t-b: blocking\n"
"\t-d: verbose\n"
"\t-h: show this help\n"
"\tdon't use both -d and -e options in same time.\n"
	, n_speed, n_buflen);
	exit(-1);
}

/**
 * parse the message.
 */
int
test_parse_func(struct sio_ctx *ctx)
{
	if (ctx->debug > 0) {
		printf("=== received total length=%d\n", ctx->datalen);
		sio_dump(ctx->buf, ctx->datalen);
	}

	sio_buf_forward(ctx, ctx->datalen);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	int f_blocking = 0;
	int f_debug = 0;
	int f_flush = 0;
	struct sio_ctx *ctx;

	while ((ch = getopt(argc, argv, "s:bB:dh")) != -1) {
		switch (ch) {
		case 's':
			n_speed = atoi(optarg);
			break;
		case 'b':
			f_blocking = 1;
			break;
		case 'B':
			n_buflen = atoi(optarg);
			break;
		case 'F':
			f_flush++;
			break;
		case 'd':
			f_debug++;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	ctx = sio_init(argv[0], n_speed, f_blocking, n_buflen, f_flush,
	    test_parse_func, NULL, f_debug);
	sio_loop(ctx);

	exit(0);
}
