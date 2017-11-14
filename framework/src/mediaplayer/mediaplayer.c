/****************************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <tinyara/config.h>
#include <tinyara/audio/audio.h>
#include <tinyara/audio/pcm.h>
#include <tinyalsa/tinyalsa.h>
#include <tinyara/fs/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <mediaplayer/mediaplayer.h>
#include "mplayer_debug.h"

/***
 * Structure
 */
struct media_s {
	void * info;	
	media_op_t op;
	media_state_t state;
	media_type_t type;
	media_play_type_t play_type;
	struct pcm_config *config;
};

struct media_file_s {
	int fd;
};

struct media_stream_s {
	void (*callback)(void* );
};

typedef struct media_file_s media_file_t;
typedef struct media_stream_s media_stream_t;
/***
 * Static variables
 */
static pthread_t g_pth_playing;
static pthread_t g_pth_recording;
static media_t *g_media_play;
static media_t *g_media_record;

static pthread_mutex_t g_mutex_playing;
static pthread_mutex_t g_mutex_recording;

static bool g_playing;
static bool g_recording;

/***
 * Pre-definitions
 */
media_result_t audio_playing(void *args);
media_result_t audio_recording(void *args);

media_result_t media_init(struct media_cb_s *cbs)
{
	
	g_playing = false;
	g_recording = false;
	g_media_play = false;
	g_media_record = false;
	

	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_NORMAL);
	
	pthread_mutex_init(&g_mutex_playing, &mutex_attr);
	pthread_mutex_init(&g_mutex_recording, &mutex_attr);
	
	// TODO: Error handling
	return MEDIA_OK;
}

void media_shutdown(void)
{
	// Shutdown playback thread
	pthread_mutex_lock(&g_mutex_playing);
	if (g_playing) {
		g_playing = false;
		if (g_media_play != NULL) {
			g_media_play->state = MEDIA_STATE_STOPPING;
		}
	}
	pthread_mutex_unlock(&g_mutex_playing);
	pthread_join(g_pth_playing, NULL);
	pthread_mutex_destroy(&g_mutex_playing);
	g_media_play = NULL;
	
	// Shutdown Record thread
	pthread_mutex_lock(&g_mutex_recording);
	if (g_recording) {
		g_recording = false;
		if (g_media_record != NULL) {
			g_media_record->state = MEDIA_STATE_STOPPING;
		}
	}
	pthread_mutex_unlock(&g_mutex_recording);
	pthread_join(g_pth_playing, NULL);
	pthread_mutex_destroy(&g_mutex_recording);
	g_media_record = NULL;

}

media_result_t media_play(media_t *m, bool loop)
{
	// pthread create stuff
	pthread_mutex_lock(&g_mutex_playing);
	if (m->state == MEDIA_STATE_CREATED && !g_playing) {
		pthread_attr_t attr;
		struct sched_param sparam;

		if (pthread_attr_init(&attr) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}

		sparam.sched_priority = 150;
		if (pthread_attr_setschedparam(&attr, &sparam) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
	
		if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}

		if (pthread_attr_setstacksize(&attr, 8192) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
		g_media_play = m;
		if (pthread_create(&g_pth_playing, &attr, (pthread_startroutine_t)audio_playing, (void *)m) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
		m->state = MEDIA_STATE_PLAYING;
		g_playing = true;
		pthread_mutex_unlock(&g_mutex_playing);
		return MEDIA_OK;
	}	
	pthread_mutex_unlock(&g_mutex_playing);
	return MEDIA_ERROR;
}

media_result_t media_record(media_t *m)
{
	pthread_mutex_lock(&g_mutex_recording);
	if (m->state == MEDIA_STATE_CREATED && !g_recording) {
		pthread_attr_t attr;
		struct sched_param sparam;

		if (pthread_attr_init(&attr) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}

		sparam.sched_priority = 150;
		if (pthread_attr_setschedparam(&attr, &sparam) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
	
		if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}

		if (pthread_attr_setstacksize(&attr, 8192) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
		g_media_record = m;
		if (pthread_create(&g_pth_recording, &attr, (pthread_startroutine_t)audio_recording, (void *)m) != 0) {
			return MEDIA_ERROR_THREAD_INIT;
		}
		m->state = MEDIA_STATE_RECORDING;
		g_recording = true;
		pthread_mutex_unlock(&g_mutex_recording);
		return MEDIA_OK;
	}	
	pthread_mutex_unlock(&g_mutex_recording);
	return MEDIA_ERROR;

}

media_result_t media_stop(media_t *m)
{
	if (m->op == MEDIA_OP_PLAYBACK) {
		if (m->state != MEDIA_STATE_CREATED && m->state != MEDIA_STATE_STOPPED && m->state !=MEDIA_STATE_CLOSING) {
			pthread_mutex_lock(&g_mutex_playing);
			m->state = MEDIA_STATE_STOPPING;
			g_media_play = NULL;
			pthread_mutex_unlock(&g_mutex_playing);
			pthread_join(g_pth_playing, NULL);
			return MEDIA_OK;
		}
		return MEDIA_ERROR;
		
	} else {
		if (m->state != MEDIA_STATE_CREATED && m->state != MEDIA_STATE_STOPPED && m->state !=MEDIA_STATE_CLOSING) {
			pthread_mutex_lock(&g_mutex_recording);
			m->state = MEDIA_STATE_STOPPING;
			g_media_record = NULL;
			pthread_mutex_unlock(&g_mutex_recording);
			pthread_join(g_pth_recording, NULL);
			return MEDIA_OK;
		}
		return MEDIA_ERROR;
	}
}

media_result_t media_pause(media_t *m)
{
	media_result_t res = MEDIA_ERROR;
	if (m->op == MEDIA_OP_PLAYBACK) {
		pthread_mutex_lock(&g_mutex_playing);
	} else {
		pthread_mutex_lock(&g_mutex_recording);

	}
	if (m->state == MEDIA_STATE_PLAYING) {
		m->state = MEDIA_STATE_PAUSING;
		res = MEDIA_OK;
	}
	if (m->op == MEDIA_OP_PLAYBACK) {
		pthread_mutex_unlock(&g_mutex_playing);
	} else {
		pthread_mutex_unlock(&g_mutex_recording);

	}
	return res;
}

media_result_t media_resume(media_t *m)
{
	media_result_t res = MEDIA_ERROR;
	if (m->op == MEDIA_OP_PLAYBACK) {
		pthread_mutex_lock(&g_mutex_playing);
	} else {
		pthread_mutex_lock(&g_mutex_recording);

	}
	if (m->state == MEDIA_STATE_PAUSED) {
		m->state = MEDIA_STATE_PLAYING;
		res = MEDIA_OK;
	}
	if (m->op == MEDIA_OP_PLAYBACK) {
		pthread_mutex_unlock(&g_mutex_playing);
	} else {
		pthread_mutex_unlock(&g_mutex_recording);

	}
	return res;

}

media_state_t media_get_state(media_t *m)
{
	return m->state;
}

static media_result_t read_wav_header(media_t *m)
{
	//struct pcm_config config;
	struct wav_header_s header;
	int ret;

	ret = read(((media_file_t *)m->info)->fd, &header, sizeof(header));
	if (ret != sizeof(header)) {
		return MEDIA_ERROR_UNKNOWN_FILE;
	}

	// Todo: Add header information to media_t

	return MEDIA_OK;
}

media_type_t get_media_type(char *path, media_t * m)
{
	char *dot = strrchr(path, '.');
	if (dot != NULL) {
		if (strcmp(dot, ".wav") == 0) {
			return MEDIA_TYPE_WAV;
		}
	}
	return MEDIA_TYPE_PCM;
}

media_t *media_open(void *info, media_op_t op, media_play_type_t play_type, struct pcm_config *config)
{
	media_t *m = (media_t *)malloc(sizeof(media_t));
	if (m == NULL) {
		return NULL;
	}
	m->op = op;
	m->state = MEDIA_STATE_CREATED;
	m->play_type = play_type;
	m->config = config;
	if (play_type == MEDIA_PLAY_TYPE_FILE) {
		m->info = (void *)malloc(sizeof(media_file_t));
		if (op == MEDIA_OP_PLAYBACK) {
			m->type = get_media_type((char *)info, m);
			if (m->type == MEDIA_TYPE_WAV || m->type == MEDIA_TYPE_PCM) {
				((media_file_t *)m->info)->fd = open((char *)info, O_RDONLY);
				if (((media_file_t *)m->info)->fd < 0) {
					goto error_out;
				}

				if (m->type == MEDIA_TYPE_WAV) {
					read_wav_header(m);
				}
				m->state = MEDIA_STATE_PLAYING;
				return m;
			} else {
				goto error_out;
			}
		} else if (op == MEDIA_OP_RECORD) {
				m->type = MEDIA_TYPE_WAV;
				((media_file_t *)m->info)->fd = (int)info; 
				if (((media_file_t *)m->info)->fd < 0) {
					goto error_out;
				}
				m->state = MEDIA_STATE_RECORDING;
				return m;
		}
	} else if (play_type == MEDIA_PLAY_TYPE_STREAM) {
		m->info = (void *)malloc(sizeof(media_stream_t));
		((media_stream_t *)m->info)->callback = info;
		// TODO : Complete streaming support
		goto error_out;
	}

error_out:
	free(m->info);
	free(m);
	return NULL;
}

media_result_t media_close(media_t *m)
{
	int ret;
	if (m == NULL) {
		return MEDIA_OK;
	}
	if (m->state == MEDIA_STATE_STOPPED) {
		if (m->op == MEDIA_OP_PLAYBACK && m->play_type == MEDIA_PLAY_TYPE_FILE) {
			if (((media_file_t *)m->info)->fd >= 0) {
				ret = close(((media_file_t *)m->info)->fd);
				if (ret < 0) {
					return MEDIA_ERROR;
				}
			}
		}
		free(m);
		return MEDIA_OK;
	}
	return MEDIA_ERROR;
}


int on_media_state_playing(struct pcm *pcmout, media_t *m, char *buffer, unsigned int buffer_size)
{
	int readed;
	int remain;
	int ret;
	readed = read(((media_file_t *)m->info)->fd, buffer, buffer_size);
	if (readed) {
		remain = pcm_bytes_to_frames(pcmout, readed);
		while (remain > 0) {
			ret = pcm_writei(pcmout, buffer + readed - pcm_frames_to_bytes(pcmout, remain), remain);
			if (ret > 0) {
				remain -= ret;
			} else {
				return ret;
			}
		}
	}

	return readed;
}

int on_media_state_recording(struct pcm *pcmin, media_t *m, char *buffer, unsigned int buffer_size)
{
	int readed;
	int remain;
	int ret;

	readed = pcm_readi(pcmin, buffer, pcm_bytes_to_frames(pcmin, buffer_size));
	if (readed) {
		remain = pcm_frames_to_bytes(pcmin, readed);
		while (remain > 0) {
			ret = write(((media_file_t *)m->info)->fd, buffer + pcm_frames_to_bytes(pcmin, readed) - remain, remain);
			if (ret > 0) {
				remain -= ret;
			} else {
				return ret;
			}
		}
	}
	return readed;
}

/***
 * Thread routine
 */
media_result_t audio_playing(void *args)
{
	char *buffer;
	unsigned int buffer_size;
	struct pcm *pcmout;
	int ret;
	media_t * node = (media_t* )args; 	
	buffer = NULL;
	buffer_size = 0;
	pcmout = NULL;

	pcmout = pcm_open(0, 0, PCM_OUT, NULL);
	buffer_size = pcm_frames_to_bytes(pcmout, pcm_get_buffer_size(pcmout));
	buffer = (char *)malloc(buffer_size);
	if (buffer == NULL) {
		pthread_mutex_lock(&g_mutex_playing);
		node->state = MEDIA_STATE_STOPPED;
		g_playing = false;
		pcm_close(pcmout);
		pthread_mutex_unlock(&g_mutex_playing);
		return MEDIA_ERROR_PLAYBACK;
	}

	MPLAYER_VERBOSE("audio_playing thread started. Buffer size: %d\n", buffer_size);

	while (g_playing) {
		pthread_mutex_lock(&g_mutex_playing);
		switch (node->state) {
			case MEDIA_STATE_PLAYING:
				ret = on_media_state_playing(pcmout, node, buffer, buffer_size);
				if (ret == 0) {
					node->state = MEDIA_STATE_STOPPING;
				} else if (ret < 0) {
					MPLAYER_ERROR("Playing error!\n");
					node->state = MEDIA_STATE_STOPPING;
				}
				break;
			case MEDIA_STATE_PAUSING:
				node->state = MEDIA_STATE_PAUSED;
				break;

			case MEDIA_STATE_PAUSED:
				break;

			case MEDIA_STATE_STOPPING:
				node->state = MEDIA_STATE_STOPPED;
				g_playing = 0;	
				break;
			default:
				break;
		}
		pthread_mutex_unlock(&g_mutex_playing);

		usleep(1);
	}
	pcm_close(pcmout);
	free(buffer);
	MPLAYER_VERBOSE("audio_playing thread terminated.\n");
	return MEDIA_OK;
}

media_result_t audio_recording(void *args)
{
	char *buffer;
	unsigned int buffer_size;
	struct pcm *pcmin;
	buffer_size = 0;
	media_t * node = (media_t *)args;
	
	pcmin = pcm_open(0, 0, PCM_IN, NULL);
	buffer_size = pcm_frames_to_bytes(pcmin, pcm_get_buffer_size(pcmin));
	buffer = (char *)malloc(buffer_size);
	if (buffer == NULL) {
		pthread_mutex_lock(&g_mutex_recording);
		pcm_close(pcmin);
		pcmin = NULL;
		node->state = MEDIA_STATE_STOPPED;
		g_recording = false;
		pthread_mutex_unlock(&g_mutex_recording);
		return MEDIA_ERROR_RECORD;
	}

	MPLAYER_VERBOSE("audio_recording thread started. Buffer size: %d\n", buffer_size);

	while (g_recording) {

		pthread_mutex_lock(&g_mutex_recording);
		switch (node->state) {
			case MEDIA_STATE_RECORDING:
				if (on_media_state_recording(pcmin, (media_t *)node, buffer, buffer_size) < 0) {
					// Todo: Error handling
					MPLAYER_ERROR("Recording error!\n");
					node->state = MEDIA_STATE_STOPPING;
				}
				break;

			case MEDIA_STATE_PAUSING:
				node->state = MEDIA_STATE_PAUSED;
				break;

			case MEDIA_STATE_PAUSED:
				break;

			case MEDIA_STATE_STOPPING:
				node->state = MEDIA_STATE_STOPPED;
				g_recording = false;
				continue;
			default:
				break;
		}
		pthread_mutex_unlock(&g_mutex_recording);
		usleep(1);
	}
	pcm_close(pcmin);
	free(buffer);
	MPLAYER_VERBOSE("audio_recording thread terminated.\n");
	return MEDIA_OK;
}
