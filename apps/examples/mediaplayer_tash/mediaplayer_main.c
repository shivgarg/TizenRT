/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <apps/shell/tash.h>
#include "mediaplayer.h"


#include <sys/socket.h>
#include <dm/dm_connectivity.h>
#include <dm/dm_error.h>

// for test
#include <tinyara/audio/audio.h>
#include <tinyara/audio/pcm.h>
#include <tinyalsa/tinyalsa.h>

#define BUF_MAX_SIZE 4096

int isWiFiConnected = 0;

void WiFi_Link_Up(void)
{
	isWiFiConnected = 1;
	printf("\n[WiFi] Connected!\n");
}

void WiFi_Link_Down(void)
{
	isWiFiConnected = 0;
	printf("\n[WiFi] Disconnected!\n");
}


static void usage(char *cmd)
{
	printf("Usage: %s [option] [filename1] [filename2]\n", cmd);
	printf("\t-p: Playing\n");
	printf("\t-r: Recording\n");

	printf("\t-h: Playing HTTP Streaming\n");
	printf("\n");
}

static void http_streaming(void)
{
	int fd;
	int ret;
	struct sockaddr_in dest;
	char buf[BUF_MAX_SIZE];

	struct pcm_config config;
	config.channels = 2;
	config.rate = 16000;
	config.period_size = CONFIG_AUDIO_BUFFER_NUMBYTES;
	config.period_count = CONFIG_AUDIO_NUM_BUFFERS;
	config.format = PCM_FORMAT_S16_LE;
	config.start_threshold = 0;
	config.stop_threshold = 0;
	config.silence_threshold = 0;	

	memset(&dest, 0, sizeof(struct sockaddr_in));
	dest.sin_family = PF_INET;
	dest.sin_addr.s_addr = inet_addr("128.214.222.15");
	dest.sin_port = htons(80);

	isWiFiConnected = 0;
	dm_cb_register_init();
	dm_conn_wifi_connect(WiFi_Link_Up, WiFi_Link_Down);

	while (1) {
		if (isWiFiConnected == 1) {
			break;
		}
		printf("[WiFi] Waiting Connect......\n");
		sleep(1);
	}
	dm_conn_dhcp_init();
	sleep(5);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("[WiFi] Open Socket Failed!\n");
	}

	if (connect(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr)) < 0) {
		printf("[TCPCLIENT] connect fail: %d\n", errno);
	}

	printf("Connection success\n");

	printf("Sending\n");
	char req[] = "GET /tmt/opetus/uusmedia/esim/a2002011001-e02-16kHz.wav HTTP/1.1\r\nHost: www.music.helsinki.fi\r\nConnection: keep-alive\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nReferer: http://www.music.helsinki.fi/tmt/opetus/uusmedia/esim/index-e.html\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: en-US,en;q=0.8,ko;q=0.6\r\n\r\n";
	ret = send(fd, req, sizeof(req), 0);
	printf("Sended %d/%d\n", ret, sizeof(req));
	memset(buf, 0, BUF_MAX_SIZE);
	printf("Start recv\n");

	struct pcm *pcm = pcm_open(0, 0, PCM_OUT, &config);

	recv(fd, buf, 351, 0);
	printf("Header: %s\n", buf);

	int s = 0;
	while(1) {
		s = 4096;
		while(s > 0) {
			ret = recv(fd, buf + 4096 - s, s, 0);
			s -= ret;
			if(s <= 0 || ret <= 0) break;
		}

		if(ret <= 0) break;
		
		/*
		if(data_start == -1) {
			printf(">>%s<<\n", buf);
			for(i = 4; i < ret; i++) {
				if(buf[i - 4] == '\r' && buf[i - 3] == '\n' && buf[i - 2] == '\r' && buf[i - 1] == '\n') {
					data_start = i;
					break;
				}
			}
		}
		*/

		pcm_writei(pcm, buf, pcm_bytes_to_frames(pcm, 4096));
	}

	closesocket(fd);
	pcm_close(pcm);
}

static int media_player_tash_cb(int argc, char **argv)
{
	media_t *music = NULL;
	int option;

	if (argc < 3) {
		usage(argv[0]);
		return 0;
	}


	while ((option = getopt(argc, argv, "prh")) != ERROR) {
		switch (option) {
		case 'p':
			media_init(NULL);

			music = media_open(argv[2], MEDIA_OP_PLAYBACK, MEDIA_TYPE_PCM);
			media_play(music, false);
			printf("Playing started(with Pause/Resume) [%s]\n", argv[2]);
			sleep(5);
			printf("Pause!\n");
			media_pause(music);
			sleep(3);
			printf("Resume!\n");
			media_resume(music);
			sleep(7);
			printf("Done\n");
			media_stop(music);
			media_close(music);
			media_shutdown();
			break;

		case 'r':
			media_init(NULL);

			music = media_open(argv[2], MEDIA_OP_RECORD, MEDIA_TYPE_PCM);
			media_record(music);
			printf("Recording 7secs started [%s]\n", argv[2]);
			sleep(7);
			printf("Done\n");
			media_stop(music);

			media_close(music);
			media_shutdown();
			break;

		case 'h':
			http_streaming();

			break;

		default:
			usage(argv[0]);
			return 0;
			break;
		}
	}

	return 0;
}

/****************************************************************************
 * mediaplayer_tash_main
 ****************************************************************************/
#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int mediaplayer_tash_main(int argc, char **argv)
#endif
{
	tash_cmd_install("mediaplayer_tash", media_player_tash_cb, TASH_EXECMD_ASYNC);

	return 0;
}
