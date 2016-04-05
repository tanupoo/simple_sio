#ifdef __linux__
#include <endian.h>
#else
#include <i386/endian.h>
#endif

#if BYTE_ORDER == LITTLE_ENDIAN

/* dst: (will be initialized), bp: src pointer, len: byte length */
#define BCOPYLE(dst, bp, len) do { \
	int _len = len; \
	dst = 0; \
	while (_len--) { \
		dst |= (*(bp+_len)<<(_len*8)) & (0xff<<(_len*8)); \
	} \
	bp += len; } while (0)

#define BCOPYBE(dst, bp, len) do { \
	int _len = len; \
	dst = 0; \
	while (_len--) { \
		dst |= (*(bp+_len)<<((len-_len-1)*8)) & (0xff<<((len-_len-1)*8)); \
	} \
	bp += len; } while (0)

/* copy data alighed little endian into the dst in a little endian system */
/* dst: (will be initialized), bp: src pointer */
#define COPY08LE(dst, bp)	BCOPYLE(dst, bp, 1)
#define COPY16LE(dst, bp)	BCOPYLE(dst, bp, 2)
#define COPY24LE(dst, bp)	BCOPYLE(dst, bp, 3)
#define COPY32LE(dst, bp)	BCOPYLE(dst, bp, 4)

/* copy data alighed big endian into the dst in a little endian system */
#define COPY08BE(dst, bp)	BCOPYBE(dst, bp, 1)
#define COPY16BE(dst, bp)	BCOPYBE(dst, bp, 2)
#define COPY24BE(dst, bp)	BCOPYBE(dst, bp, 3)
#define COPY32BE(dst, bp)	BCOPYBE(dst, bp, 4)

#else

#error "big endian system is not supported."

#endif

/*
 *  |------------------ buflen ----------------|
 *  |-- offset --|------- datalen -------|
 *  v
 * buf
 *        * buflen < offset + datalen
 *
 *  e.g.  buflen = 10
 *    0 1 2 3 4 5 6 7 8 9
 *        |
 *        +- if offset = 2, max datalen = 7
 */
struct sio_ctx {
	int fd;
	int (*cb)(struct sio_ctx *);
	void *cb_ctx;	/* the holder to be passed into cb */
/* pointer to the next of the end of the valid data. */
#define SIOCTX_BUF_TAIL(ctx)	(ctx->buf + ctx->offset + ctx->datalen)
/* remain length in the buffer */
#define SIOCTX_BUF_RESTLEN(ctx)	(ctx->buflen - ctx->offset - ctx->datalen - 1)
	char *buf;	/* pointer to the top of the buffer */
	int buflen;	/* buffer length */
	int offset;	/* offset to the top of the valid data */
	int datalen;	/* valid data length */

	int f_flush;	/* flush the buffer everytime */

	char *device;
	void *orig_tios; /* original termios */
	int blocking;
	int speed;
	int debug;
};

struct sio_ctx *sio_init(char *, int, int, int, int,
    int (*)(struct sio_ctx *), void *, int);
void sio_buf_reset(struct sio_ctx *);
void sio_buf_forward(struct sio_ctx *, int);
void sio_buf_rewind(struct sio_ctx *);
void sio_dump(void *, int);
int sio_readx(void *);
void sio_loop(struct sio_ctx *);
