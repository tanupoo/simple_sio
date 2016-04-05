#include <sys/types.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <err.h>

#include "simple_sio.h"

static speed_t
get_brate(int speed)
{
	switch (speed) {
	case 0: return B0;
	case 50: return B50;
	case 75: return B75;
	case 110: return B110;
	case 134: return B134;
	case 150: return B150;
	case 200: return B200;
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 115200: return B115200;
	case 230400: return B230400;
#ifndef _POSIX_SOURCE
	case 7200: return B7200;
	case 14400: return B14400;
	case 28800: return B28800;
	case 57600: return B57600;
	case 76800: return B76800;
#endif
	default: return speed;
	}
}

void
sio_buf_reset(struct sio_ctx *ctx)
{
	ctx->offset = 0;
	ctx->datalen = 0;
}

void
sio_buf_forward(struct sio_ctx *ctx, int len)
{
	if (ctx->offset + len > ctx->buflen) {
		sio_buf_reset(ctx);
		return;
	}

	ctx->offset += len;
	ctx->datalen -= len;
}

void
sio_buf_rewind(struct sio_ctx *ctx)
{
	memcpy(ctx->buf, ctx->buf + ctx->offset, ctx->datalen);
	ctx->offset = 0;
}

void
sio_revert(struct sio_ctx *ctx)
{
	if (tcsetattr(ctx->fd, TCSANOW, (struct termios *)ctx->orig_tios) == -1)
		err(1, "ERROR: %s: tcsetattr()", __FUNCTION__);
}

/*
 * @param f_blocking if non-zero, ICANON is set.
 */
struct sio_ctx *
sio_init(char *device, int speed, int f_blocking, int buflen, int f_flush,
    int (*cb)(struct sio_ctx *), void *cb_ctx, int debug)
{
	int fd;
	int mode;
	struct termios tty, *orig_tios;
	speed_t brate;
	struct sio_ctx *ctx;

	mode = O_RDWR;
	mode |= O_NOCTTY;
	if (!f_blocking)
		mode |= O_NONBLOCK;

	fd = open(device, mode);
	if (fd == -1)
		err(1, "ERROR: %s: open(%s)", __FUNCTION__, device);

	memset(&tty, 0, sizeof(tty));
	if (tcgetattr (fd, &tty) == -1)
		err(1, "ERROR: %s: tcgetattr()", __FUNCTION__);

	/* save the termios of the device */
	if ((orig_tios = calloc(1, sizeof(struct termios))) == NULL)
		err(1, "ERROR: %s: calloc(termios)", __FUNCTION__);
	memcpy(orig_tios, &tty, sizeof(tty));

	cfmakeraw(&tty);

	if (f_blocking)
		tty.c_lflag |= ICANON;
	else
		tty.c_lflag &= ~ICANON;

	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	brate = get_brate(speed);
	cfsetospeed(&tty, brate);
	cfsetispeed(&tty, brate);

	if (tcsetattr(fd, TCSANOW, &tty) == -1)
		err(1, "ERROR: %s: tcsetattr()", __FUNCTION__);

	/* create context */
	if ((ctx = calloc(1, sizeof(struct sio_ctx))) == NULL)
		err(1, "ERROR: %s: calloc(sio_ctx)", __FUNCTION__);

	if ((ctx->buf = calloc(buflen, sizeof(char))) == NULL)
		err(1, "ERROR: %s: calloc(char)", __FUNCTION__);

	ctx->fd = fd;
	ctx->cb = cb;
	ctx->cb_ctx = cb_ctx;
	ctx->buflen = buflen;
	ctx->offset = 0;
	ctx->device = strdup(device);
	ctx->orig_tios = orig_tios;
	ctx->f_flush = f_flush;
	ctx->blocking = f_blocking;
	ctx->speed = speed;
	ctx->debug = debug;

	return ctx;
}

void
sio_dump(void *data0, int datalen)
{
	char *data = (char *)data0;
	int i;

	for (i = 0; i < datalen; i++) {
		if (i)
			printf(" ");
		printf("%02x", data[i]&0xff);
	}
	printf("\n");
}

int
sio_readx(void *ctx_base)
{
	int recvlen;
	struct sio_ctx *ctx = (struct sio_ctx *)ctx_base;

	if (SIOCTX_BUF_RESTLEN(ctx) == 0)
		sio_buf_rewind(ctx);

	recvlen = read(ctx->fd, SIOCTX_BUF_TAIL(ctx), SIOCTX_BUF_RESTLEN(ctx));
	if (recvlen == 0)
		return 0;
	if (recvlen < 0) {
		switch(errno) {
		case EAGAIN:
			if (ctx->debug > 1)
				printf("EAGAIN\n");
			return 0;
		}
		err(1, "ERROR: %s: read()", __FUNCTION__);
	}
	/* sanity check */
	if (recvlen > SIOCTX_BUF_RESTLEN(ctx))
		errx(1, "ERROR: %s: recvlen is too big. panic", __FUNCTION__);

	if (ctx->debug > 1) {
		printf("== ");
		if (ctx->debug > 2) {
			time_t t = time(NULL);
			struct tm *tm = localtime(&t);
			char buf[32];
			char *fmt = "%Y-%m-%dT%H:%M:%S%z";
			strftime(buf, sizeof(buf), fmt, tm);
			printf("%s\n", buf);
		}
		printf("received length=%d\n", recvlen);
		sio_dump(SIOCTX_BUF_TAIL(ctx), recvlen);
	}

	ctx->datalen += recvlen;

	if (ctx->cb)
		ctx->cb(ctx);

	if (ctx->f_flush)
		sio_buf_reset(ctx);

	return 0;
}

void
sio_loop(struct sio_ctx *ctx)
{
	int n_fds = 0;
	fd_set rfds;
	int ret;

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(ctx->fd, &rfds);
		n_fds = ctx->fd;

		ret = select(n_fds + 1, &rfds, NULL, NULL, NULL);
		if (ret == 0)
			continue;
		if (ret == -1)
			errx(1, "ERROR: %s: select() = -1", __FUNCTION__);
		if (ret < -1)
			errx(1, "ERROR: %s: select() < -1", __FUNCTION__);

		if (FD_ISSET(ctx->fd, &rfds)) {
			sio_readx(ctx);
		}
	}
}
