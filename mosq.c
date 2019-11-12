#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "mosquitto.h"
#include "mosq.h"

#define DEBUG

#ifdef DEBUG
#define printf(...) printf(__VA_ARGS__)
#else
#define printf(...)
#endif

static int Timeout(struct timespec *ts, int ms)
{
	struct timespec ts1;

	if ((!clock_gettime(CLOCK_MONOTONIC, &ts1)) && \
			(ms <= ((ts1.tv_sec - ts->tv_sec)*1000 + (ts1.tv_nsec - ts->tv_nsec)/1000000)))
				return 1;

	return 0;
}

int mosq_init(mosq_s *mosq, const char *host, int port, int keepalive, char *topic_pub, char *topic_sub)
{
	if (!mosq)
		return -1;

	mosq->host = host;
	mosq->port = port;
	mosq->keepalive = keepalive;
	mosq->pub_topic = topic_pub;
	mosq->sub_topic = topic_sub;
	mosq->pub_run = STATE_IDLE;
	mosq->sub_run = STATE_IDLE;

	return 0;
}

void mosq_cleanup(mosq_s *mosq)
{
	if (!mosq)
		return;

	mosq_pub_stop(mosq);
	mosq_sub_stop(mosq);
	memset(mosq, 0, sizeof(mosq_s));

	return;
}

void pub_on_connect(struct mosquitto *mosquitto, void *userdata, int result)
{
	int ret;
	mosq_s *mosq = userdata;

	if(result){
		mosq->pub_run = STATE_STOPPED;
	}else{
		mosq->pub_status |= STATUS_CONNECTED;

		ret = mosquitto_publish(mosquitto, &mosq->pub_mid, \
						mosq->pub_topic, \
						strlen(mosq->pub_msg), mosq->pub_msg, 0, false);
		if (ret != MOSQ_ERR_SUCCESS) {
			mosq->pub_run = STATE_STOPPED;
		}
	}
}

void pub_on_publish(struct mosquitto *mosquitto, void *userdata, int mid)
{
	int ret;
	mosq_s *mosq = userdata;

	if(mid == mosq->pub_mid){
		mosq->pub_status |= STATUS_PUBLISHED;
		
		ret = mosquitto_disconnect(mosquitto);
		if (ret != MOSQ_ERR_SUCCESS) {
			mosq->pub_run = STATE_STOPPED;
		} else
			mosq->pub_status |= STATUS_DISCONNECTING;
	}
}

void pub_on_disconnect(struct mosquitto *mosquitto, void *userdata, int rc)
{
	mosq_s *mosq = userdata;

	if(rc){
		mosq->pub_status &= ~STATUS_CONNECTED;
	}else{
		mosq->pub_run = STATE_STOPPED;
		mosq->pub_status = STATUS_DISCONNECTED;
	}
}

int mosq_pub_start(mosq_s *mosq, const char *msg)
{
	int ret;

	if (mosq->pub_run != STATE_IDLE)
		return -1;

	mosq->pub = mosquitto_new(NULL, true, mosq);
	if (!mosq->pub) {
		printf("Error pub mosquitto_new<BR><HR>\n");
		return -1;
	}

	mosq->pub_msg = msg;

	mosquitto_connect_callback_set(mosq->pub, pub_on_connect);
	mosquitto_publish_callback_set(mosq->pub, pub_on_publish);
	mosquitto_disconnect_callback_set(mosq->pub, pub_on_disconnect);

	if(mosquitto_connect(mosq->pub, mosq->host, mosq->port, mosq->keepalive)){
		printf("Error pub mosquitto_connect<BR><HR>\n");
		goto EXIT;
	}

	ret = mosquitto_loop_start(mosq->pub);
	if (ret != MOSQ_ERR_SUCCESS) {
		printf("Error pub mosquitto_loop_start ret=%d<BR><HR>\n", ret);
		goto EXIT;
	}
	mosq->pub_run = STATE_RUNNING;

	return 0;

EXIT:
	mosquitto_destroy(mosq->pub);
	mosq->pub = NULL;
	mosq->pub_msg = NULL;
	return -1;
}

int mosq_pub_stop(mosq_s *mosq)
{
	int ret = -1;

	if (mosq->pub) {
		if (mosq->pub_status & STATUS_PUBLISHED)
			ret = 0;
		if ((mosq->pub_status & STATUS_CONNECTED) && \
				!(mosq->pub_status & STATUS_DISCONNECTING))
			mosquitto_disconnect(mosq->pub);

		mosquitto_loop_stop(mosq->pub, true);
		mosquitto_destroy(mosq->pub);
		mosq->pub = NULL;
		mosq->pub_run = STATE_IDLE;
		mosq->pub_status = 0;
		mosq->pub_msg = NULL;
		mosq->pub_mid = 0;
	}

	return ret;
}

int mosq_pub_running(mosq_s *mosq)
{
	if ((mosq->pub) && (mosq->pub_run == STATE_RUNNING))
		return 1;
	else
		return 0;
}

int mosq_publish(mosq_s *mosq, const char *msg, int ms)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		return -1;

	if (mosq_pub_start(mosq, msg))
		return -1;

	while (!Timeout(&ts, ms)) {
		if (!mosq_pub_running(mosq))
			break;
		usleep(1000);
	}

	return mosq_pub_stop(mosq);
}

void sub_on_connect(struct mosquitto *mosquitto, void *userdata, int result)
{
	int ret;
	mosq_s *mosq = userdata;

	if (result) {
		mosq->sub_run = STATE_STOPPED;
	} else {
		mosq->sub_status |= STATUS_CONNECTED;

		ret = mosquitto_subscribe(mosquitto, NULL, mosq->sub_topic, 0);
		if (ret != MOSQ_ERR_SUCCESS) {
			mosq->sub_run = STATE_STOPPED;
		}
	}
}

void sub_on_message(struct mosquitto *mosquitto, void *userdata, const struct mosquitto_message *msg)
{
	int ret;
	mosq_s *mosq = userdata;

	if(msg->payloadlen){
		if (mosq->sub_callback)
			mosq->sub_callback(mosq->sub_userdata, msg->payload);
		mosq->sub_status |= STATUS_SUBSCRIBED;

		ret = mosquitto_disconnect(mosquitto);
		if (ret != MOSQ_ERR_SUCCESS) {
			mosq->sub_run = STATE_STOPPED;
		} else
			mosq->sub_status |= STATUS_DISCONNECTING;
	}
}

void sub_on_disconnect(struct mosquitto *mosquitto, void *userdata, int rc)
{
	mosq_s *mosq = userdata;

	if(rc){
		mosq->sub_status &= ~STATUS_CONNECTED;
	}else{
		mosq->sub_run = STATE_STOPPED;
	}
}

int mosq_sub_start(mosq_s *mosq, void (*callback)(void *userdata, const char *), void *userdata)
{
	int ret;

	if (mosq->sub_run != STATE_IDLE)
		return -1;

	mosq->sub = mosquitto_new(NULL, true, mosq);
	if (!mosq->sub) {
		printf("Error sub mosquitto_new<BR><HR>\n");
		return -1;
	}

	mosq->sub_callback = callback;
	mosq->sub_userdata = userdata;

	mosquitto_connect_callback_set(mosq->sub, sub_on_connect);
	mosquitto_message_callback_set(mosq->sub, sub_on_message);
	mosquitto_disconnect_callback_set(mosq->sub, sub_on_disconnect);

	if(mosquitto_connect(mosq->sub, mosq->host, mosq->port, mosq->keepalive)){
		printf("Error sub mosquitto_connect<BR><HR>\n"); 
		goto EXIT;
	}

	ret = mosquitto_loop_start(mosq->sub);
	if (ret != MOSQ_ERR_SUCCESS) {
		printf("Error sub mosquitto_loop_start ret=%d<BR><HR>\n", ret);
		goto EXIT;
	}
	mosq->sub_run = STATE_RUNNING;

	return 0;
	
EXIT:
	mosquitto_destroy(mosq->sub);
	mosq->sub = NULL;
	mosq->sub_callback = NULL;
	mosq->sub_userdata = NULL;
	return -1;
}

int mosq_sub_stop(mosq_s *mosq)
{
	int ret = -1;

	if (mosq->sub) {
		if (mosq->sub_status & STATUS_SUBSCRIBED)
			ret = 0;
		if ((mosq->sub_status & STATUS_CONNECTED) && \
				!(mosq->sub_status & STATUS_DISCONNECTING))
			mosquitto_disconnect(mosq->sub);

		mosquitto_loop_stop(mosq->sub, true);
		mosquitto_destroy(mosq->sub);
		mosq->sub = NULL;
		mosq->sub_run = STATE_IDLE;
		mosq->sub_status = 0;
		mosq->sub_callback = NULL;
		mosq->sub_userdata = NULL;
	}

	return ret;
}

int mosq_sub_running(mosq_s *mosq)
{
	if ((mosq->sub) && (mosq->sub_run == STATE_RUNNING))
		return 1;
	else
		return 0;
}

int mosq_subscribe(mosq_s *mosq, void (*callback)(void *userdata, const char *), void *userdata, int ms)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		return -1;

	if (mosq_sub_start(mosq, callback, userdata))
		return -1;

	while (!Timeout(&ts, ms)) {
		if (!mosq_sub_running(mosq))
			break;
		usleep(1000);
	}

	return mosq_sub_stop(mosq);
}
