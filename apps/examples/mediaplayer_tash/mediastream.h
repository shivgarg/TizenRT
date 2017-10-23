#include <stdio.h>

typedef enum {
	MEDIASTREAM_FILE,
	MEDIASTREAM_NET_HTTP,
	MEDIASTREAM_NET_HTTPS,
	MEDIASTREAM_NET_MQTT
} mediastream_type_e;

typedef enum {
	MEDIASTREAM_NONE,
	MEDIASTREAM_NORMAL,
	MEDIASTREAM_BUFFERING,
	MEDIASTREAM_RECONNECTING
} mediastream_state_e;

struct mediastream_s {
	
};

typedef struct mediastream_s mediastream_t;

/**
 * Public functions
 */
mediastream_t mediastream_open(const char *path, mediastream_type_e type);

int mediastream_read(unsigned char *buffer, size_t size);

int mediastream_timed_read(unsigned char *buffer, size_t size, int timeout_ms);

int mediastream_write(unsigned char *buffer, size_t size);

int mediastream_close(mediastream_t *stream);

/**
 * Internal functions
 */
int stream_read_from_file();

int stream_read_from_http();

int stream_read_from_https();

int stream_read_from_mqtt();

int stream_write_from_file();

int stream_write_from_http();

int stream_write_from_https();

int stream_write_from_mqtt();