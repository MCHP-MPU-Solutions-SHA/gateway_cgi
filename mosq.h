#ifndef MOSQ_H
#define MOSQ_H

#include <stdbool.h>

typedef enum mosq_state_t {
	STATE_IDLE,
	STATE_RUNNING,
	STATE_STOPPED
} mosq_state_e;

typedef enum mosq_status_t {
	STATUS_CONNECTED,
	STATUS_PUBLISHED,
	STATUS_SUBSCRIBED,
	STATUS_DISCONNECTING,
	STATUS_DISCONNECTED
} mosq_status_e;

typedef struct mosq_t {
	struct mosquitto *pub;
	char             *pub_topic;
	mosq_state_e      pub_run;
	mosq_status_e     pub_status;
	int               pub_mid;
	const char       *pub_msg;
	struct mosquitto *sub;
	char             *sub_topic;
	mosq_state_e      sub_run;
	mosq_status_e     sub_status;
	void            (*sub_callback)(void *userdata, const char *);
	void             *sub_userdata;
	const char       *host;
	int               port;
	int               keepalive;
} mosq_s;

extern int mosquitto_lib_init(void);
extern int mosquitto_lib_cleanup(void);

int mosq_init(mosq_s *mosq, const char *host, int port, int keepalive, char *topic_pub, char *topic_sub);
void mosq_cleanup(mosq_s *mosq);
int mosq_pub_start(mosq_s *mosq, const char *msg);
int mosq_pub_stop(mosq_s *mosq);
int mosq_pub_running(mosq_s *mosq);
int mosq_publish(mosq_s *mosq, const char *msg, int ms);
int mosq_sub_start(mosq_s *mosq, void (*callback)(void *userdata, const char *), void *userdata);
int mosq_sub_stop(mosq_s *mosq);
int mosq_sub_running(mosq_s *mosq);

#endif /* MOSQ_C */
