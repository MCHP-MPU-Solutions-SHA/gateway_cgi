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

#define MCHPDEMO

#define CONFIG_FILE     "/tmp/lights.json"
#define CONFIG_FILE_SWP "/tmp/lights.json.swp"

#define NAME_LEN 15

#define JSON_LIGHTS    "lights"
#define JSON_WEBUPDA        "webupda"
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

char *status1[] = {
	"0",
	"1",
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

typedef enum devitem_t {
	DEV_ID,
	DEV_NAME,
	DEV_VALID,
	DEV_LED,
	DEV_TEMP,
	DEV_RSSI,
	DEV_CLKWS,
	DEV_CNTCLKWS
} devitem_e;

typedef struct updadev_t {
	int i;  //array index
	int webupda;
	devitem_e updaitem;
	char value[JSON_NAME_LEN+1];
} updadev_s;

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
void ParseReport(void *userdata, const char *report);
int LightAdd(context_s *ctx, char *name, char *led, char *temp, char *rssi, char *clkwise, char *cntclkwise);
int LightDelete(context_s *ctx, int num);
char *Names(flag_e flag, int num);
char *Topics(topic_e flag, char *mac);
char *Commands(command_e cmd);
void ShowStatus(context_s *ctx);
void UpdateJson(updadev_s* upda);

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
	updadev_s updadev;
	memset(&updadev, 0 , sizeof(updadev_s));

	clock_gettime(CLOCK_MONOTONIC, &ts1);

	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	/* Top of the page */
	fprintf(cgiOut, "<HTML><HEAD>\n");
#ifdef MCHPDEMO
	fprintf(cgiOut, "<IMG src=\"../mchplogo.png\" alt=\"logo\">\n");
#else
	fprintf(cgiOut, "<IMG src=\"../logo.png\" alt=\"logo\">\n");
#endif
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

	/* Check status buttons */
	for (i=0; i<ctx.light_num; i++) {
		updadev.i = i;
		if (cgiFormSubmitClicked(Names(LIGHT_ON, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_LED;
			updadev.webupda = 1;
			strncpy(updadev.value, "1\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		} else if (cgiFormSubmitClicked(Names(LIGHT_OFF, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_LED;
			updadev.webupda = 1;
			strncpy(updadev.value, "0\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		} else if (cgiFormSubmitClicked(Names(LIGHT_CLKWISE_ON, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_CLKWS;
			updadev.webupda = 2;
			strncpy(updadev.value, "1\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		} else if (cgiFormSubmitClicked(Names(LIGHT_CLKWISE_OFF, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_CLKWS;
			updadev.webupda = 2;
			strncpy(updadev.value, "0\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		} else if (cgiFormSubmitClicked(Names(LIGHT_CNTCLKWISE_ON, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_CNTCLKWS;
			updadev.webupda = 3;
			strncpy(updadev.value, "1\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		} else if (cgiFormSubmitClicked(Names(LIGHT_CNTCLKWISE_OFF, i)) == cgiFormSuccess) {
			updadev.updaitem = DEV_CNTCLKWS;
			updadev.webupda = 3;
			strncpy(updadev.value, "0\0", 2);
			UpdateJson(&updadev);
			pageIndex = PAGE_STATUS;
			break;
		}
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

void UpdateJson(updadev_s* upda)
{
	cJSON *json, *json_light, *json_array_ori, *json_array,*json_id, *json_name, *json_valid, *json_led, *json_temp, *json_rssi, *json_clkwise, *json_cntclkwise;
	char webupda[JSON_NAME_LEN+1];
	char id[JSON_NAME_LEN+1];
	char name[JSON_NAME_LEN+1];
	char valid[JSON_NAME_LEN+1];
	char led[JSON_NAME_LEN+1];
	char temp[JSON_NAME_LEN+1];
	char rssi[JSON_NAME_LEN+1];
	char clkwise[JSON_NAME_LEN+1];
	char cntclkwise[JSON_NAME_LEN+1];

	json = OpenJson(CONFIG_FILE);
	json_light = cJSON_GetObjectItem(json, "lights");
	if (!json_light) {
		fprintf(cgiOut, "ERROR get json first node\n");
		CloseJson(json);
		return;
	}


	json_array_ori = cJSON_GetArrayItem(json_light, upda->i);
	if (!json_array_ori) {
		fprintf(cgiOut, "Error cJSON_GetArrayItem %d\n<BR><HR>\n", upda->i);
		return;
	}

	json_id         = cJSON_GetObjectItem(json_array_ori, JSON_ID);
	json_name       = cJSON_GetObjectItem(json_array_ori, JSON_NAME);
	json_valid       = cJSON_GetObjectItem(json_array_ori, JSON_VALID);
	json_led        = cJSON_GetObjectItem(json_array_ori, JSON_LED);
	json_temp          = cJSON_GetObjectItem(json_array_ori, JSON_TEMPERATURE);
	json_rssi             = cJSON_GetObjectItem(json_array_ori, JSON_RSSI);
	json_clkwise      = cJSON_GetObjectItem(json_array_ori, JSON_CLKWISE);
	json_cntclkwise = cJSON_GetObjectItem(json_array_ori, JSON_CNTCLKWISE);
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
		return;
	}

	snprintf(webupda, JSON_NAME_LEN, "%d", upda->webupda);

	memcpy(id, json_id->valuestring, strlen(json_id->valuestring));
	id[strlen(json_id->valuestring)] = '\0';

	memcpy(name, json_name->valuestring, strlen(json_name->valuestring));
	name[strlen(json_name->valuestring)] = '\0';

	memcpy(valid, json_valid->valuestring, strlen(json_valid->valuestring));
	valid[strlen(json_valid->valuestring)] = '\0';

	memcpy(led, json_led->valuestring, strlen(json_led->valuestring));
	led[strlen(json_led->valuestring)] = '\0';

	memcpy(temp, json_temp->valuestring, strlen(json_temp->valuestring));
	temp[strlen(json_temp->valuestring)] = '\0';

	memcpy(rssi, json_rssi->valuestring, strlen(json_rssi->valuestring));
	rssi[strlen(json_rssi->valuestring)] = '\0';

	memcpy(clkwise, json_clkwise->valuestring, strlen(json_clkwise->valuestring));
	clkwise[strlen(json_clkwise->valuestring)] = '\0';

	memcpy(cntclkwise, json_cntclkwise->valuestring, strlen(json_cntclkwise->valuestring));
	cntclkwise[strlen(json_cntclkwise->valuestring)] = '\0';

	switch (upda->updaitem) {
		case DEV_ID:
			strncpy(id, upda->value, strlen(upda->value));
			break;
		case DEV_NAME:
			strncpy(name, upda->value, strlen(upda->value));
			break;
		case DEV_VALID:
			strncpy(valid, upda->value, strlen(upda->value));
			break;
		case DEV_LED:
			strncpy(led, upda->value, strlen(upda->value));
			break;
		case DEV_TEMP:
			strncpy(temp, upda->value, strlen(upda->value));
			break;
		case DEV_RSSI:
			strncpy(rssi, upda->value, strlen(upda->value));
			break;
		case DEV_CLKWS:
			strncpy(clkwise, upda->value, strlen(upda->value));
			break;
		case DEV_CNTCLKWS:
			strncpy(cntclkwise, upda->value, strlen(upda->value));
			break;
		default:
			break;
	}

	json_array = cJSON_CreateObject();
	if (!json_array) {
		fprintf(cgiOut, "ERROR create object json\n");
		CloseJson(json);
		return;
	}
	//name[strlen(name)] = '\0';

	//printf("temp:%s, len:%d\n", temp, strlen(temp));
	//printf("rssi:%s, len:%d\n", rssi, strlen(rssi));
	if (!cJSON_AddStringToObject(json_array, JSON_WEBUPDA, webupda) || \
		!cJSON_AddStringToObject(json_array, JSON_ID, id) || \
		!cJSON_AddStringToObject(json_array, JSON_VALID, valid) || \
		!cJSON_AddStringToObject(json_array, JSON_NAME, name) || \
		!cJSON_AddStringToObject(json_array, JSON_LED, led) || \
		!cJSON_AddStringToObject(json_array, JSON_TEMPERATURE, temp) || \
		!cJSON_AddStringToObject(json_array, JSON_RSSI, rssi) || \
		!cJSON_AddStringToObject(json_array, JSON_CLKWISE, clkwise) || \
		!cJSON_AddStringToObject(json_array, JSON_CNTCLKWISE, cntclkwise)) {
		fprintf(cgiOut, "ERROR add string to json\n");
		CloseJson(json);
		return;
	}

	cJSON_ReplaceItemInArray(json_light, upda->i, json_array);
	SaveJson(CONFIG_FILE, json);
	CloseJson(json);
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
	//struct stat state;

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
	}

#if 0
	if (0 > stat(CONFIG_FILE, &state)) {
		printf("stat json error\n");
		return -1;
	}
	printf("st_mtime:%ld\n", state.st_mtime);
#endif

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
		if ((!json_id || !json_name || !json_valid || !json_led || !json_temp || !json_rssi || !json_clkwise || !json_cntclkwise) || \
				(json_id->type != cJSON_String || \
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

char *Names(flag_e flag, int num)
{
	static char name[NAME_LEN+1];

	switch (flag) {
		case LIGHT_OFF:
			snprintf(name, NAME_LEN, "Off%d", num);
			break;
		case LIGHT_CLKWISE_OFF:
			snprintf(name, NAME_LEN, "Clkoff%d", num);
			break;
		case LIGHT_CNTCLKWISE_OFF:
			snprintf(name, NAME_LEN, "Cckoff%d", num);
			break;
		case LIGHT_ON:
			snprintf(name, NAME_LEN, "On%d", num);
			break;
		case LIGHT_CLKWISE_ON:
			snprintf(name, NAME_LEN, "Clkon%d", num);
			break;
		case LIGHT_CNTCLKWISE_ON:
			snprintf(name, NAME_LEN, "Cckon%d", num);
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
		case LIGHT_OFFLINE:
		case LIGHT_ERROR:
			break;
		default:
			name[0] = '\0';
			break;
	}
	return name;
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
#ifdef MCHPDEMO
	fprintf(cgiOut, "<tr align=\"center\"> <th>ID</th>  <th>Name</th> <th>LED</th> <th>Temperature</th> <th>RSSI</th> <th>GPIO1 </th> <th>GPIO2 </th> </tr>\n");
#else
	fprintf(cgiOut, "<tr align=\"center\"> <th>ID</th>  <th>Name</th> <th>LED</th> <th>Temperature</th> <th>RSSI</th> <th>Forward </th> <th>Reverse </th> </tr>\n");
#endif
	for (i=0; i<ctx->light_num; i++) {
		fprintf(cgiOut, "<tr align=\"center\">\n");
		fprintf(cgiOut, "<td>%d</td> <td>%s</td>\n", i, (*ctx->lights)[i].name);
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
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"1\"> </td>\n", status1[0], Names(LIGHT_CLKWISE_OFF, i));
		} else if ((*ctx->lights)[i].status1 == LIGHT_CLKWISE_OFF) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"0\"> </td>\n", status1[1], Names(LIGHT_CLKWISE_ON, i));
		}

		if ((*ctx->lights)[i].status2 == LIGHT_CNTCLKWISE_ON) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"1\"> </td>\n", status1[0], Names(LIGHT_CNTCLKWISE_OFF, i));
		} else if ((*ctx->lights)[i].status2 == LIGHT_CNTCLKWISE_OFF) {
			fprintf(cgiOut, "<td>%s <input type=\"submit\" name=\"%s\" value=\"0\"> </td>\n", status1[1], Names(LIGHT_CNTCLKWISE_ON, i));
		}

	}
	fprintf(cgiOut, "</table>\n");
	fprintf(cgiOut, "</form>\n");
	fprintf(cgiOut, "<meta http-equiv=\"refresh\" content=\"2\">\n");
}
