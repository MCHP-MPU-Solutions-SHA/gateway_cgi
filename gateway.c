#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "cgic.h"
#include "cjson/cJSON.h"
//#include "mosq.h"

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

#define CONFIG_FILE     "lights.json"
#define CONFIG_FILE_SWP "lights.json.swp"

#define NAME_LEN 10

#define JSON_LIGHTS    "lights"
#define JSON_ID        "id"
#define JSON_NAME		   "name"
#define JSON_VALID		   "valid"
#define JSON_LED		   "led"
#define JSON_TEMPERATURE "temperature"
#define JSON_RSSI "rssi"
#define JSON_CLKWISE "clockwise"
#define JSON_CNTCLKWISE "counterclockwise"
#define JSON_NAME_LEN	12


char *status[] = {
	"Off",
	"On",
	"Offline",
	"Error"
};

typedef enum flag_t {
	LIGHT_OFF = 0,
	LIGHT_ON,
	LIGHT_OFFLINE,
	LIGHT_ERROR,
	LIGHT_ALL_OFF,
	LIGHT_ALL_ON,
	LIGHT_CONFIG,
	LIGHT_REFRESH,
	LIGHT_ADD,
	LIGHT_DELETE,
	LIGHT_BACK,
	LIGHT_NAME,
	LIGHT_LED,
	LIGHT_TEMP,
	LIGHT_RSSI,
	LIGHT_CLKWISE,
	LIGHT_CNTCLKWISE,
	LIGHT_CLKWISE_ON,
	LIGHT_CLKWISE_OFF,
	LIGHT_CNTCLKWISE_ON,
	LIGHT_CNTCLKWISE_OFF
} flag_e;

typedef enum topic_t {
	TOPIC_REPORT,
	TOPIC_CONTROL
} topic_e;

typedef enum command_t {
	CMD_LIGHT_OFF,
	CMD_LIGHT_ON,
	CMD_STATUS
} command_e;

typedef struct light_t {
	char *id;
	char *name;
	char *valid;
	char *led;
	char *temperature;
	char *rssi;
	char *clockwise;
	char *counterclockwise;
	int status;
	int status1;
	int status2;
} light_s;

typedef struct context_t {
	int us;
	cJSON *json;
	int light_num;
	light_s (*lights)[];
} context_s;

typedef enum page_t {
	PAGE_STATUS,
	PAGE_CONFIG,
} page_e;

cJSON *OpenJson(char *file);
void CloseJson(cJSON *json);
int SaveJson(char *file, cJSON *json);
int LoadConfig(context_s *ctx);
void FreeConfig(context_s *ctx);
int ReloadConfig(context_s *ctx);
void ParseReport(void *userdata, const char *report);
int LightAdd(context_s *ctx, char *name, char *led, char *temp, char *rssi, char *clkwise, char *cntclkwise);
int LightDelete(context_s *ctx, int num);
char *Names(flag_e flag, int num);
char *Topics(topic_e flag, char *mac);
char *Commands(command_e cmd);
void ShowStatus(context_s *ctx);
void ShowConfig(context_s *ctx);

static int Timeout(struct timespec *ts, int ms)
{
	struct timespec ts1;

	if ((!clock_gettime(CLOCK_MONOTONIC, &ts1)) && \
			(ms <= ((ts1.tv_sec - ts->tv_sec)*1000 + (ts1.tv_nsec - ts->tv_nsec)/1000000)))
				return 1;

	return 0;
}

int cgiMain() {
	int i;
	int stoped;
	page_e pageIndex = PAGE_STATUS;
	struct timespec ts1, ts2, ts;
	context_s ctx = {0};

	clock_gettime(CLOCK_MONOTONIC, &ts1);

	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	/* Top of the page */
	fprintf(cgiOut, "<HTML><HEAD>\n");
	fprintf(cgiOut, "<IMG src=\"../logo.png\" alt=\"logo\">\n");
	fprintf(cgiOut, "<TITLE>Mi-Wi Gateway</TITLE></HEAD>\n");
	fprintf(cgiOut, "<BODY><H1>Mi-Wi IoT Control Center</H1>\n");

	/* Load lights configuration from JSON file */
	ctx.json = OpenJson(CONFIG_FILE);
	if (!ctx.json) {
		fprintf(cgiOut, "Error OpenJson<BR><HR>\n");
		goto EXIT;
	}

	if (LoadConfig(&ctx)) {
		fprintf(cgiOut, "Error LoadConfig<BR><HR>\n");
		goto EXIT1;
	}

	/* If a submit button has already been clicked, act on the
		submission of the form. */
	/* Check config buttons */
	if (cgiFormSubmitClicked(Names(LIGHT_CONFIG, 0)) == cgiFormSuccess) {
		pageIndex = PAGE_CONFIG;
	} else if (cgiFormSubmitClicked(Names(LIGHT_ADD, 0)) == cgiFormSuccess) {
		char name[JSON_NAME_LEN+1];
		char led[JSON_NAME_LEN+1];
		char temp[JSON_NAME_LEN+1];
		char rssi[JSON_NAME_LEN+1];
		char clkwise[JSON_NAME_LEN+1];
		char cntclkwise[JSON_NAME_LEN+1];

		cgiFormString(Names(LIGHT_NAME, 0), name, JSON_NAME_LEN+1);
		cgiFormString(Names(LIGHT_LED, 0), led, JSON_NAME_LEN+1);
		cgiFormString(Names(LIGHT_TEMP, 0), temp, JSON_NAME_LEN+1);
		cgiFormString(Names(LIGHT_RSSI, 0), rssi, JSON_NAME_LEN+1);
		cgiFormString(Names(LIGHT_CLKWISE, 0), clkwise, JSON_NAME_LEN+1);
		cgiFormString(Names(LIGHT_CNTCLKWISE, 0), cntclkwise, JSON_NAME_LEN+1);
		if (strlen(name) && strlen(led) && strlen(temp) && strlen(rssi) && strlen(clkwise) && strlen(cntclkwise)) {
			LightAdd(&ctx, name, led, temp, rssi, clkwise, cntclkwise);
			SaveJson(CONFIG_FILE, ctx.json);
			ReloadConfig(&ctx);
		}
		pageIndex = PAGE_CONFIG;
	} else {
		for (i=0; i<ctx.light_num; i++) {
			if (cgiFormSubmitClicked(Names(LIGHT_DELETE, i)) == cgiFormSuccess) {
				LightDelete(&ctx, i);
				SaveJson(CONFIG_FILE, ctx.json);
				ReloadConfig(&ctx);
				pageIndex = PAGE_CONFIG;
				break;
			}
		}
	}

	/* Show the config page */
	if (pageIndex == PAGE_CONFIG) {
		ShowConfig(&ctx);
		goto EXIT2;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);
	while (!Timeout(&ts, 200)) {
		stoped = 0;
		for (i=0; i<ctx.light_num; i++) {
			//if (!mosq_pub_running(&(*ctx.lights)[i].mosq))
			stoped++;
		}

		if (stoped == ctx.light_num)
			break;
		usleep(1*1000);
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);
	while (!Timeout(&ts, 2000)) {
		stoped = 0;
		for (i=0; i<ctx.light_num; i++) {
			//if (!mosq_sub_running(&(*ctx.lights)[i].mosq))
				stoped++;
		}
		if (stoped == ctx.light_num)
			break;
		usleep(1*1000);
	}

	/* Show the status page */
	if (pageIndex == PAGE_STATUS) {
		ShowStatus(&ctx);
	}

EXIT2:
	FreeConfig(&ctx);
EXIT1:
	CloseJson(ctx.json);
EXIT:
	/* Show processing time */
	clock_gettime(CLOCK_MONOTONIC, &ts2);
	ctx.us = (ts2.tv_sec - ts1.tv_sec)*1000000 + (ts2.tv_nsec - ts1.tv_nsec)/1000;
	//fprintf(cgiOut, "<i>time %d.%dms</i>\n", ctx.us/1000, ctx.us%1000);
	/* Finish up the page */
	fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}

cJSON *OpenJson(char *file)
{
	int fd;
	int ret;
	char *text;
	struct stat state;
	cJSON *json = NULL;

	ret = stat(file, &state);
	if (ret < 0) {
		if (errno == ENOENT) { // Creat an empty json instance
			json = cJSON_CreateObject();
			if (!json) {
				fprintf(cgiOut, "Error cJSON_CreateObject<BR><HR>\n");
				return NULL;
			}

			if (!cJSON_AddArrayToObject(json, JSON_LIGHTS)) {
				cJSON_Delete(json);
				fprintf(cgiOut, "Error cJSON_AddArrayToObject<BR><HR>\n");
				return NULL;
			}

			return json;
		} else {
			fprintf(cgiOut, "Error stat %s<BR><HR>\n", file);
			goto EXIT;
		}
	}

	text = malloc(state.st_size + 1);
	if (!text) {
		fprintf(cgiOut, "Error malloc<BR><HR>\n");
		goto EXIT;
	}

	if ((fd = open(file, O_RDONLY)) < 0) {
		fprintf(cgiOut, "Error open %s<BR><HR>\n", file);
		goto EXIT1;
	}

	ret = read(fd, text, state.st_size);
	if (ret != state.st_size) {
		fprintf(cgiOut, "Error read %s, ret=%d<BR><HR>\n", file, ret);
		goto EXIT2;
	}
	text[state.st_size] = '\0';

	json = cJSON_Parse(text);
	if (!json) {
		fprintf(cgiOut, "Error cJSON_Parse %s<BR><HR>\n", file);
	}

EXIT2:
	close(fd);
EXIT1:
	free(text);
EXIT:
	return json;
}

void CloseJson(cJSON *json)
{
	if (json)
		 cJSON_Delete(json);

	return;
}

int SaveJson(char *file, cJSON *json)
{
	FILE *fp;
	int ret;

	fp=fopen(file, "w");
	if (!fp) {
		fprintf(cgiOut, "Error open %s for save<BR><HR>\n", file);
		return -1;
	}

	ret = fputs(cJSON_Print(json), fp);
	fclose(fp);
	if (ret < 0) {
		fprintf(cgiOut, "Error fpust %s for save ret=%d<BR><HR>\n", file, ret);
		return -1;
	} else
		return 0;
}

int LoadConfig(context_s *ctx)
{
	int i;
	cJSON *json_lights, *json_light, *json_id, *json_name, *json_valid, *json_led, *json_temp, *json_rssi, *json_clkwise, *json_cntclkwise;

	json_lights = cJSON_GetObjectItem(ctx->json, JSON_LIGHTS);
	if (!json_lights) {
		fprintf(cgiOut, "Error cJSON_GetObjectItem lights<BR><HR>\n");
		goto EXIT;
	}

	if (json_lights->type != cJSON_Array) {
		fprintf(cgiOut, "Error json_lights->type=%d<BR><HR>\n", json_lights->type);
		goto EXIT;
	}

	ctx->light_num = cJSON_GetArraySize(json_lights);
	if (ctx->light_num < 0) {
		fprintf(cgiOut, "Error light number: %d<BR><HR>\n", ctx->light_num);
		goto EXIT;
	} else if (ctx->light_num == 0)
		return 0;

	ctx->lights = malloc(ctx->light_num*sizeof(light_s));
	if (!ctx->lights) {
		fprintf(cgiOut, "Error malloc ctx->lights\n<BR><HR>\n");
		goto EXIT1;
	}
	memset(ctx->lights, 0, ctx->light_num*sizeof(light_s));

	for (i=0; i<ctx->light_num; i++) {
		json_light = cJSON_GetArrayItem(json_lights, i);
		if (!json_light) {
			fprintf(cgiOut, "Error cJSON_GetArrayItem %d\n<BR><HR>\n", i);
			goto EXIT2;
		}

		json_id         = cJSON_GetObjectItem(json_light, JSON_ID);
		json_name       = cJSON_GetObjectItem(json_light, JSON_NAME);
		json_valid       = cJSON_GetObjectItem(json_light, JSON_VALID);
		json_led        = cJSON_GetObjectItem(json_light, JSON_LED);
		json_temp          = cJSON_GetObjectItem(json_light, JSON_TEMPERATURE);
		json_rssi             = cJSON_GetObjectItem(json_light, JSON_RSSI);
		json_clkwise      = cJSON_GetObjectItem(json_light, JSON_CLKWISE);
		json_cntclkwise = cJSON_GetObjectItem(json_light, JSON_CNTCLKWISE);
		if ((!json_name || !json_valid || !json_led || !json_temp || !json_rssi || !json_clkwise || !json_cntclkwise) || \
				(
					json_name->type != cJSON_String || \
					json_valid->type != cJSON_String || \
					json_led->type != cJSON_String || \
					json_temp->type != cJSON_String || \
					json_rssi->type != cJSON_String || \
					json_clkwise->type != cJSON_String || \
					json_cntclkwise->type != cJSON_String)) {
			fprintf(cgiOut, "Error get name and topic<BR><HR>\n");
			goto EXIT2;
		}

		(*ctx->lights)[i].id               = json_id->valuestring;
		(*ctx->lights)[i].name             = json_name->valuestring;
		(*ctx->lights)[i].valid             = json_valid->valuestring;
		(*ctx->lights)[i].led              = json_led->valuestring;
		(*ctx->lights)[i].temperature      = json_temp->valuestring;
		(*ctx->lights)[i].rssi             = json_rssi->valuestring;
		(*ctx->lights)[i].clockwise        = json_clkwise->valuestring;
		(*ctx->lights)[i].counterclockwise = json_cntclkwise->valuestring;

		fprintf(cgiOut, "temp=%s\n", (*ctx->lights)[i].temperature);

		if (atoi(json_led->valuestring))
			(*ctx->lights)[i].status = LIGHT_ON;
		else
			(*ctx->lights)[i].status = LIGHT_OFF;

		if (!atoi(json_valid->valuestring))
			(*ctx->lights)[i].status = LIGHT_OFFLINE;

		if (atoi(json_clkwise->valuestring))
			(*ctx->lights)[i].status1 = LIGHT_CLKWISE_ON;
		else
			(*ctx->lights)[i].status1 = LIGHT_CLKWISE_OFF;

		if (atoi(json_cntclkwise->valuestring))
			(*ctx->lights)[i].status2 = LIGHT_CNTCLKWISE_ON;
		else
			(*ctx->lights)[i].status2 = LIGHT_CNTCLKWISE_OFF;
	}

	return 0;
EXIT2:
	free(ctx->lights);
	ctx->lights = NULL;
EXIT1:
	ctx->light_num = 0;
EXIT:
	return -1;
}

void FreeConfig(context_s *ctx)
{
	if (ctx->lights) {
		free(ctx->lights);
		ctx->lights = NULL;
	}
	ctx->light_num = 0;

	return;
}

int ReloadConfig(context_s *ctx)
{
	FreeConfig(ctx);
	return LoadConfig(ctx);
}

int LightAdd(context_s *ctx, char *name, char *led, char *temp, char *rssi, char *clkwise, char *cntclkwise)
{
	cJSON *json_lights, *json_light;
	char id[4] = {0};
	json_lights = cJSON_GetObjectItem(ctx->json, "lights");
	if (!json_lights) {
		fprintf(cgiOut, "Error cJSON_GetObjectItem lights<BR><HR>\n");
		goto EXIT;
	}

	json_light = cJSON_CreateObject();
	if (!json_light) {
		fprintf(cgiOut, "Error cJSON_CreateObject<BR><HR>\n");
		goto EXIT;
	}

	sprintf(id, "%d", ctx->light_num + 1);
	if (!cJSON_AddStringToObject(json_light, JSON_ID, id) || \
		!cJSON_AddStringToObject(json_light, JSON_NAME, name) || \
		!cJSON_AddStringToObject(json_light, JSON_LED, led) || \
		!cJSON_AddStringToObject(json_light, JSON_TEMPERATURE, temp) || \
		!cJSON_AddStringToObject(json_light, JSON_RSSI, rssi) || \
		!cJSON_AddStringToObject(json_light, JSON_CLKWISE, clkwise) || \
		!cJSON_AddStringToObject(json_light, JSON_CNTCLKWISE, cntclkwise)) {
		fprintf(cgiOut, "Error cJSON_AddStringToObject<BR><HR>\n");
		goto EXIT1;
	}

	cJSON_AddItemToArray(json_lights, json_light);

	return 0;
EXIT1:
	cJSON_Delete(json_light);
EXIT:
	return -1;
}

int LightDelete(context_s *ctx, int num)
{
	cJSON *json_lights = cJSON_GetObjectItem(ctx->json, "lights");

	json_lights = cJSON_GetObjectItem(ctx->json, "lights");
	if (!json_lights) {
		fprintf(cgiOut, "Error cJSON_GetObjectItem lights<BR><HR>\n");
		goto EXIT;
	}

	cJSON_DeleteItemFromArray(json_lights, num);

	return 0;
EXIT:
	return -1;
}

#if 0
void ParseReport(void *userdata, const char *report)
{
	light_s *light = userdata;
	cJSON *json_report, *json_params, *json_light, *json_temp, *json_hum, *json_uv;

	if (!report)
		return;

	light->status = LIGHT_ERROR;
	json_report = cJSON_Parse(report);
	if (json_report) {
		json_params = cJSON_GetObjectItem(json_report, "params");
		if (json_params) {
			json_light = cJSON_GetObjectItem(json_params, "light_switch");
			if (json_light) {
				if (json_light->valueint == 1)
					light->status = LIGHT_ON;
				else if (json_light->valueint == 0)
					light->status = LIGHT_OFF;
			}
			json_temp = cJSON_GetObjectItem(json_params, "temp");
			if (json_temp) {
				light->temperature = json_temp->valuedouble;
			}
			json_hum = cJSON_GetObjectItem(json_params, "hum");
			if (json_hum) {
				light->hum = json_hum->valueint;
			}
			json_uv = cJSON_GetObjectItem(json_params, "uv");
			if (json_uv) {
				light->uv = json_uv->valueint;
			}

			}
		cJSON_Delete(json_report);
	}

	return;
}
#endif

char *Names(flag_e flag, int num)
{
	static char name[NAME_LEN+1];

	switch (flag) {
		case LIGHT_OFF:
		case LIGHT_CLKWISE_OFF:
		case LIGHT_CNTCLKWISE_OFF:
			snprintf(name, NAME_LEN, "Off%d", num);
			break;
		case LIGHT_ON:
		case LIGHT_CLKWISE_ON:
		case LIGHT_CNTCLKWISE_ON:
			snprintf(name, NAME_LEN, "On%d", num);
			break;
		case LIGHT_ALL_OFF:
			return "OffAll";
		case LIGHT_ALL_ON:
			return "OnAll";
		case LIGHT_CONFIG:
			return "Config";
		case LIGHT_REFRESH:
			return "Refresh";
		case LIGHT_ADD:
			return "Add";
		case LIGHT_DELETE:
			snprintf(name, NAME_LEN, "Delete%d", num);
			break;
		case LIGHT_BACK:
			return "Back";
		case LIGHT_NAME:
			return "Name";
		case LIGHT_LED:
			return "LED";
		case LIGHT_TEMP:
			return "Temperature";
		case LIGHT_RSSI:
			return "RSSI";
		case LIGHT_CLKWISE:
			return "Clockwise";
		case LIGHT_CNTCLKWISE:
			return "Counterclockwise";
	}
	name[0] = '\0';
	return name;
}

#if 0
char *Topics(topic_e flag, char *mac)
{
	static char topic[MQTT_TOPIC_LEN];

	if (flag == TOPIC_REPORT)
		snprintf(topic, MQTT_TOPIC_LEN, MQTT_TOPIC_PATTERN, mac, MQTT_TOPIC_REPORT);
	else if (flag == TOPIC_CONTROL)
		snprintf(topic, MQTT_TOPIC_LEN, MQTT_TOPIC_PATTERN, mac, MQTT_TOPIC_CONTROL);
	else
		topic[0] = '\0';

	return topic;
}
#endif

char *Commands(command_e cmd)
{
	switch (cmd) {
		case CMD_LIGHT_OFF:
			return "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"light_switch\":0,\"light_intensity\":88,\"led_r\":0,\"led_g\":0,\"led_b\":0}}";

		case CMD_LIGHT_ON:
			return "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"light_switch\":1,\"light_intensity\":88,\"led_r\":0,\"led_g\":0,\"led_b\":0}}";

		case CMD_STATUS:
			return "{\"version\":\"1.0\",\"params\":{\"get_status\":1}}";

		default:
			return "\0";
	}
}

void ShowStatus(context_s *ctx)
{
	int i;
	fprintf(cgiOut, "<!-- 2.0: multipart/form-data is required for file uploads. -->");
	fprintf(cgiOut, "<form method=\"POST\" enctype=\"multipart/form-data\" ");
	fprintf(cgiOut, "	action=\"");
	cgiValueEscape(cgiScriptName);
	fprintf(cgiOut, "\">\n");
	fprintf(cgiOut, "<p>\n");
	fprintf(cgiOut, "<table border=\"0\" cellpadding=\"5\">\n");
	fprintf(cgiOut, "<tr align=\"center\"> <th>ID</th>  <th>Name</th> <th>LED</th> <th>Temperature</th> <th>RSSI</th> <th>Clockwise </th> <th>Countercslockwise </th> </tr>\n");
	for (i=0; i<ctx->light_num; i++) {
		fprintf(cgiOut, "<tr align=\"center\">\n");
		fprintf(cgiOut, "<td>%d</td> <td>%s</td>\n", i+1, (*ctx->lights)[i].name);
		if ((*ctx->lights)[i].status == LIGHT_ON) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"Off\"> </td>\n", status[(*ctx->lights)[i].status], Names(LIGHT_OFF, i));
		} else if ((*ctx->lights)[i].status == LIGHT_OFF) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"On\"> </td>\n", status[(*ctx->lights)[i].status], Names(LIGHT_ON, i));
		} else {
			fprintf(cgiOut, "<td>%s</td>\n", status[(*ctx->lights)[i].status]);
			continue;
		}
		//fprintf(cgiOut, "<td>%s</td> <td>%s</td> <td>%s</td> <td>%s</td>\n",
		//									(*ctx->lights)[i].temperature, (*ctx->lights)[i].rssi, (*ctx->lights)[i].clockwise, (*ctx->lights)[i].counterclockwise);
		fprintf(cgiOut, "<td>%s</td> <td>%s</td>\n", \
											(*ctx->lights)[i].temperature, (*ctx->lights)[i].rssi);

		if ((*ctx->lights)[i].status1 == LIGHT_CLKWISE_ON) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"Off\"> </td>\n", status[(*ctx->lights)[i].status1], Names(LIGHT_CLKWISE, i));
		} else if ((*ctx->lights)[i].status1 == LIGHT_CLKWISE_OFF) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"On\"> </td>\n", status[(*ctx->lights)[i].status1], Names(LIGHT_CLKWISE, i));
		}

	}
	fprintf(cgiOut, "</table>\n");
	fprintf(cgiOut, "</form>\n");
	fprintf(cgiOut, "<meta http-equiv=\"refresh\" content=\"2\">\n");
}

void ShowConfig(context_s *ctx)
{
	int i;

	fprintf(cgiOut, "<!-- 2.0: multipart/form-data is required for file uploads. -->");
	fprintf(cgiOut, "<form method=\"POST\" enctype=\"multipart/form-data\" ");
	fprintf(cgiOut, " action=\"");
	cgiValueEscape(cgiScriptName);
	fprintf(cgiOut, "\">\n");
	fprintf(cgiOut, "<p>\n");
	fprintf(cgiOut, "<input type=\"submit\" name=\"%s\" value=\"<--  Back\">\n", Names(LIGHT_BACK, 0));
	fprintf(cgiOut, "<table border=\"0\" cellpadding=\"5\">\n");
	fprintf(cgiOut, "<tr align=\"left\"> <th>ID</th>  <th>Name</th> <th>LED</th> <th>Temperature</th> <th>RSSI</th> <th>Clockwise</th> <th>Counterclockwise</th> <th>Action</th> </tr>\n");
	for (i=0; i<ctx->light_num; i++) {
		fprintf(cgiOut, "<tr align=\"left\">\n");
		fprintf(cgiOut, "<td>%d</td> <td>%s</td> <td>%s</td>\n", i+1, (*ctx->lights)[i].name, (*ctx->lights)[i].led);
		fprintf(cgiOut, "<td><input type=\"submit\" name=\"%s\" value=\"Delete\"></td>\n", Names(LIGHT_DELETE, i));
		fprintf(cgiOut, "</tr>\n");
	}
	fprintf(cgiOut, "<tr align=\"left\"> <td></td>\n");
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"6\" maxlength=\"%d\"></td>\n", Names(LIGHT_NAME, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"12\" maxlength=\"%d\"></td>\n", Names(LIGHT_LED, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"12\" maxlength=\"%d\"></td>\n", Names(LIGHT_TEMP, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"12\" maxlength=\"%d\"></td>\n", Names(LIGHT_RSSI, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"12\" maxlength=\"%d\"></td>\n", Names(LIGHT_CLKWISE, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"text\" name=\"%s\" size=\"12\" maxlength=\"%d\"></td>\n", Names(LIGHT_CNTCLKWISE, 0), JSON_NAME_LEN);
	fprintf(cgiOut, "<td><input type=\"submit\" name=\"%s\" value=\"Add\"></td>\n", Names(LIGHT_ADD, 0));
	fprintf(cgiOut, "</tr>\n");
	fprintf(cgiOut, "</table>\n");
	fprintf(cgiOut, "</form>\n");
}
