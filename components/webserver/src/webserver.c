#include "webserver.h"

#include <httpd_server/esp_http_server.h>
#include <esp_log.h>
#include <esp_spiffs.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "webserver";
static const char SERVER_FILES_FILEPATH[] = "server_files.csv";

#define MAX_ETAG_LENGTH 64 // sha_1 hexdigest is 40 hexadecimal digits
#define SCRATCH_BUFFER_SIZE 512
static uint8_t SCRATCH_BUFFER[SCRATCH_BUFFER_SIZE] = {0};

struct EndpointFile {
    char* filepath;
    char* mimetype;
    char* sha1_hash;
    size_t file_size;
};

void free_endpoint(struct EndpointFile* file) {
    if (file->filepath != NULL) free(file->filepath);
    if (file->mimetype != NULL) free(file->mimetype);
    if (file->sha1_hash != NULL) free(file->sha1_hash);
    if (file != NULL) free(file);
}

static esp_err_t handle_endpoint_file_request(httpd_req_t *request) {
    const struct EndpointFile* file = (struct EndpointFile*)(request->user_ctx);

    // SOURCE: https://devdojo.com/vnnvanhuong/demo-http-caching-with-etag
    // Support file caching
    bool is_cache = false;
    const esp_err_t etag_status = httpd_req_get_hdr_value_str(request, "If-None-Match", (char *)SCRATCH_BUFFER, MAX_ETAG_LENGTH);
    if (etag_status == ESP_OK) {
        if (strncmp(file->sha1_hash, (char *)SCRATCH_BUFFER, MAX_ETAG_LENGTH) == 0) {
            is_cache = true;
        } else {
            is_cache = false;
            ESP_LOGI(TAG, "cache miss: rx_sha1='%s', stored_sha1='%s', uri='%s'", (char *)SCRATCH_BUFFER, file->sha1_hash, request->uri);
        }
    } else if (etag_status != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "request contained malformed 'If-None-Match' etag (%s), uri='%s'", esp_err_to_name(etag_status), request->uri);
    }
    
    httpd_resp_set_hdr(request, "ETag", file->sha1_hash);
    // cache for 1 week, always check if etag matches
    httpd_resp_set_hdr(request, "Cache-Control", "max-age=604800, public, no-cache");
    httpd_resp_set_type(request, file->mimetype);
    if (is_cache) {
        httpd_resp_set_status(request, "304 Not Modified");
        httpd_resp_send(request, NULL, 0);
        return ESP_OK;
    }

    // stream the file
    FILE* fd = fopen(file->filepath, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", file->filepath);
        httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    bool is_success = true;
    while (true) {
        const size_t total_read_bytes = fread(SCRATCH_BUFFER, 1, SCRATCH_BUFFER_SIZE, fd);
        if (total_read_bytes == 0) break;
        const esp_err_t block_send_status = httpd_resp_send_chunk(request, (char *)SCRATCH_BUFFER, total_read_bytes);
        if (block_send_status != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stream block of size %u for uri='%s' due to error '%s'", total_read_bytes, request->uri, esp_err_to_name(etag_status));
            is_success = false;
            break;
        }
    }
    httpd_resp_send_chunk(request, NULL, 0);
    fclose(fd);
    return is_success ? ESP_OK : ESP_FAIL;
}

static const char SPIFFS_ROOT_PATH[] = "";
static esp_vfs_spiffs_conf_t spiffs_config = {
  .base_path = SPIFFS_ROOT_PATH,
  .partition_label = NULL,
  .max_files = 12,
  .format_if_mount_failed = true,
};

static esp_err_t init_spiffs(void) {
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    const esp_err_t spiffs_register_status = esp_vfs_spiffs_register(&spiffs_config);
    switch (spiffs_register_status) {
    case ESP_OK:   
        ESP_LOGI(TAG, "initialising spiffs filesystem"); 
        break;
    case ESP_FAIL: 
        ESP_LOGE(TAG, "failed to mount or format spiffs filesystem"); 
        break;
    case ESP_ERR_NOT_FOUND: 
        ESP_LOGE(TAG, "failed to find spiffs partition");
        break;
    default:
        ESP_LOGE(TAG, "failed to initialize spiffs (%s)", esp_err_to_name(spiffs_register_status));
        break;
    }
    if (spiffs_register_status != ESP_OK) {
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    const esp_err_t spiffs_info_status = esp_spiffs_info(NULL, &total, &used);
    if (spiffs_info_status == ESP_OK) {
        ESP_LOGI(TAG, "got spiffs partition size: total=%d, used=%d", total, used);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "failed to get spiffs partition information (%s)", esp_err_to_name(spiffs_info_status));
        return ESP_FAIL;
    }
}

static struct EndpointFile* read_endpoint_from_line(char *line) {
    if (line == NULL) return NULL;
    static const char* DELIMITERS = ",\n";
    static const size_t EXPECTED_TOKENS = 4;
    const char* tokens[EXPECTED_TOKENS];

    size_t total_tokens = 0;
    char* token = strtok(line, DELIMITERS);
    while (token != NULL) {
        tokens[total_tokens] = token;
        total_tokens++;
        if (total_tokens >= EXPECTED_TOKENS) break;
        token = strtok(NULL, DELIMITERS);
    }

    if (total_tokens != EXPECTED_TOKENS) {
        ESP_LOGE(TAG, "Failed to read endpoint from line: '%s'", line);
        return NULL;
    }

    const char* filepath = tokens[0];
    const char* file_size_str = tokens[1];
    const char* mimetype = tokens[2];
    const char* sha1_hash = tokens[3];
    const size_t filepath_length = strlen(filepath)+sizeof(SPIFFS_ROOT_PATH)+1; // extend for prepending root directory
    const size_t mimetype_length = strlen(mimetype);
    const size_t sha1_hash_length = strlen(sha1_hash);

    unsigned int file_size = 0;
    if (sscanf(file_size_str, "%u", &file_size) != 1) {
        ESP_LOGE(TAG, "Failed to convert file size string '%s' to integer", file_size_str);
        return NULL;
    }

    struct EndpointFile* file = malloc(sizeof(struct EndpointFile));
    file->filepath = malloc(filepath_length+1);
    file->file_size = file_size;
    file->mimetype = malloc(mimetype_length+1);
    file->sha1_hash = malloc(sha1_hash_length+1);

    if (file->filepath == NULL) goto error;
    if (file->mimetype == NULL) goto error;
    if (file->sha1_hash == NULL) goto error;

    snprintf(file->filepath, filepath_length+1, "%s/%s", SPIFFS_ROOT_PATH, filepath);
    snprintf(file->mimetype, mimetype_length+1, "%s", mimetype);
    snprintf(file->sha1_hash, sha1_hash_length+1, "%s", sha1_hash);

    return file;
error:
    free_endpoint(file);
    return NULL;
}

static esp_err_t add_endpoints(httpd_handle_t server) {
    DIR *dir = opendir(SPIFFS_ROOT_PATH);
    if (dir == NULL) {
        ESP_LOGE(TAG, "failed to open spiffs folder: '%s'", SPIFFS_ROOT_PATH);
        return ESP_FAIL;
    }

    char* FILEPATH = (char*)SCRATCH_BUFFER;
    const size_t MAX_FILEPATH_LENGTH = SCRATCH_BUFFER_SIZE;

    ESP_LOGI(TAG, "listing spiffs files");
    struct stat file_stat;
    while (true) {
        struct dirent *entry = readdir(dir);
        if (entry == NULL) break;
        snprintf(FILEPATH, MAX_FILEPATH_LENGTH, "%s/%s", SPIFFS_ROOT_PATH, entry->d_name);
        if (stat(FILEPATH, &file_stat) == -1) {
            ESP_LOGI(TAG, "- name=%s (stat_failed)", FILEPATH);
            continue;
        }
        ESP_LOGI(TAG, "+ name=%s, size=%ld", FILEPATH, file_stat.st_size);
    }
    closedir(dir);

    snprintf(FILEPATH, MAX_FILEPATH_LENGTH, "%s/%s", SPIFFS_ROOT_PATH, SERVER_FILES_FILEPATH);
    FILE* fd = fopen(FILEPATH, "r");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open webserver index file: %s", FILEPATH);
        return ESP_FAIL;
    }

    char* LINE_BUFFER = (char*)SCRATCH_BUFFER;
    const size_t MAX_LINE_BUFFER_SIZE = SCRATCH_BUFFER_SIZE;
    fgets(LINE_BUFFER, MAX_LINE_BUFFER_SIZE, fd); // skip csv header line
    static const char INDEX_FILEPATH[] = "/index.html";

    size_t total_registered_endpoints = 0;
    while (true) {
        if (fgets(LINE_BUFFER, MAX_LINE_BUFFER_SIZE, fd) == NULL) {
            break;
        }
        struct EndpointFile* endpoint = read_endpoint_from_line(LINE_BUFFER);
        if (endpoint == NULL) continue;

        if (stat(endpoint->filepath, &file_stat) == -1) {
            ESP_LOGE(TAG, "Failed to get file stat for '%s'", endpoint->filepath);
            free_endpoint(endpoint);
            continue;
        }

        if ((off_t)endpoint->file_size != file_stat.st_size) {
            ESP_LOGE(TAG, "Mismatch in indexed file size (%u) and actual file size (%ld) for '%s'", endpoint->file_size, file_stat.st_size, endpoint->filepath);
            free_endpoint(endpoint);
            continue;
        }

        httpd_uri_t uri_handler = {
            .uri = endpoint->filepath,
            .method = HTTP_GET,
            .handler = handle_endpoint_file_request,
            .user_ctx = (void *)endpoint,
            .is_websocket = false,
            .handle_ws_control_frames = false,
            .supported_subprotocol = NULL,
        };
        {
            const esp_err_t status = httpd_register_uri_handler(server, &uri_handler);
            if (status == ESP_OK) {
                ESP_LOGI(TAG,
                    "registered endpoint: uri='%s', size=%u, mimetype=%s, sha1_hash=%s",
                    uri_handler.uri, endpoint->file_size, endpoint->mimetype, endpoint->sha1_hash
                );
                total_registered_endpoints++;
            } else {
                ESP_LOGE(TAG,
                    "failed to register endpoint: uri='%s', size=%u, mimetype=%s, sha1_hash=%s, error=%s",
                    uri_handler.uri, endpoint->file_size, endpoint->mimetype, endpoint->sha1_hash, esp_err_to_name(status)
                );
            }
        }
        const bool is_index_file = strncmp(endpoint->filepath, INDEX_FILEPATH, sizeof(INDEX_FILEPATH)) == 0;
        if (is_index_file) {
            uri_handler.uri = "/";
            const esp_err_t status = httpd_register_uri_handler(server, &uri_handler);
            if (status == ESP_OK) {
                ESP_LOGI(TAG, "registered '%s' to '/'", INDEX_FILEPATH);
                total_registered_endpoints++;
            } else {
                ESP_LOGE(TAG, "failed to register '%s' to '/'", INDEX_FILEPATH);
            }
        }
    }
    fclose(fd);

    ESP_LOGI(TAG, "Registered %u endpoints to httpd server", total_registered_endpoints);
    return ESP_OK;
}

esp_err_t webserver_register_endpoints(httpd_handle_t server) {
    if (server == NULL) {
        ESP_LOGE(TAG, "server was null");
        return ESP_FAIL;
    }

    if (init_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read files from spiffs partition");
        return ESP_FAIL;
    }

    if (add_endpoints(server) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create webserver endpoints");
        return ESP_FAIL;
    }

    return ESP_OK;
}
