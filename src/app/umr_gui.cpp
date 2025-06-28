/*
 * Copyright © 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
#include "parson.h"
#include <fcntl.h>
#include <mutex>
#include <unistd.h>
#include <vector>
#include <stdio.h>
#include <pthread.h>
#include <regex.h>
#include <limits.h>

#if UMR_SERVER
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#endif
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "glad/glad.h"

#include <SDL.h>
#include "gui/panels.h"
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "gui/qoi/qoi.h"

/* Random helpers */
extern void send_request(JSON_Value *req, struct umr_asic *asic);
extern void force_redraw();
extern void add_vertical_line(const ImVec2& avail);
extern bool kb_shortcut(int keycode);
extern GLuint texture_from_qoi_buffer(int width, int height, void *buffer, int buffer_size);
extern void goto_tab(int keycode);
extern float get_gui_scale();
extern "C" {
	JSON_Value *umr_process_json_request(JSON_Object *request, void **raw_data, unsigned int *raw_data_size);
}


class SyntaxHighlighter {
public:
	SyntaxHighlighter();
	~SyntaxHighlighter();

	void add_definition(const char *regexp, std::vector<const char *> colors);

	char * transform(const char *input);

private:
	struct Def {
	   const char *expression;
	   regex_t preg;
	   const char * colors[6]; /* 6 = max capture groups in the expression */
	};
	std::vector<Def> definitions;
	regmatch_t *pmatch;
	char *output;
};

static pthread_mutex_t mtx;

#include "gui/info_panel.cpp"
#include "gui/registers_panel.cpp"
#include "gui/power_panel.cpp"
#include "gui/rings_panel.cpp"
#include "gui/top_panel.cpp"
#include "gui/memory_usage_panel.cpp"
#include "gui/memory_debug_panel.cpp"
#include "gui/waves_panel.cpp"
#include "gui/kms_panel.cpp"
#include "gui/buffer_object_panel.cpp"

struct Link {
	int sock;
	int endpoint;
	bool use_sock;
};

static void save_to_disk(const char *session_folder, int msg_idx,
						 const char *answer_as_str, size_t answer_len,
						 void *raw_data, int raw_data_size) {
	char filename[PATH_MAX];

	sprintf(filename, "%s/%d.json", session_folder, msg_idx);
	FILE *f = fopen(filename, "w");
	if (f) {
		fwrite(answer_as_str, 1, answer_len, f);
		fclose(f);

		if (raw_data_size) {
			sprintf(filename, "%s/%d.raw", session_folder, msg_idx);
			FILE *f = fopen(filename, "wb");
			if (f) {
				uint32_t s = htole32(raw_data_size);
				fwrite(&s, 1, sizeof(raw_data_size), f);
				fwrite(raw_data, 1, raw_data_size, f);
				fclose(f);
			}
		}
	}
}

JSON_Value *query(struct Link& lnk, JSON_Value *request,
				  void **raw_data, unsigned *raw_data_size,
				  const char *session_folder, int msg_idx) {
	#if UMR_SERVER
	if (lnk.use_sock) {
		char* s = json_serialize_to_string(request);
		int len = strlen(s) + 1;
		int r = nn_send(lnk.sock, s, len, 0);
		json_free_serialized_string(s);

		if (r < 0)
			exit(0);
		json_value_free(request);

		if (r < 0)
			return NULL;

		char *buffer;
		len = nn_recv(lnk.sock, &buffer, NN_MSG, 0);
		if (len < 0)
			exit(0);

		if (len == 0)
			return NULL;

		int strl = strlen(&buffer[sizeof(uint32_t)]) + 1;
		memcpy(raw_data_size, buffer, sizeof(uint32_t));
		assert(len == sizeof(uint32_t) + strl + *raw_data_size);
		JSON_Value *out = json_parse_string(&buffer[sizeof(uint32_t)]);
		if (json_object_get_boolean(json_object(out), "has_raw_data")) {
			assert(*raw_data_size);
			*raw_data = malloc(*raw_data_size);
			memcpy(*raw_data,
				   &buffer[sizeof(uint32_t) + strl],
				   *raw_data_size);
		} else {
			assert(*raw_data_size == 0);
		}

		/* Save to disk for replay */
		if (session_folder)
			save_to_disk(session_folder, msg_idx,
						 &buffer[sizeof(uint32_t)], strl,
						 raw_data ? *raw_data : NULL, *raw_data_size);

		nn_freemsg(buffer);

		return out;
	} else
	#endif
	{
		JSON_Value *in = umr_process_json_request(json_object(request), raw_data, raw_data_size);

		if (session_folder) {
			char *s = json_serialize_to_string(in);
			save_to_disk(session_folder, msg_idx,
						 s, strlen(s),
						 raw_data ? *raw_data : NULL, *raw_data_size);
			json_free_serialized_string(s);
		}

		return in;
	}
}

void force_redraw() {
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	SDL_PushEvent(&evt);
}

static struct Link lnk;
static pthread_cond_t cond;
static bool done;

struct AsicData {
	AsicData(JSON_Object *answer, char *ip_discovery_dump, long did, int instance) {
		int tryipdiscovery = 0;
		memset(&options, 0, sizeof options);
		options.instance = instance;
		options.database_path[0] = '\0';
		options.no_disasm = 0;
		options.no_follow_ib = 1;
		options.vm_partition = -1;
		options.no_follow_loadx = 1;

		if (ip_discovery_dump && strlen(ip_discovery_dump)) {
			struct umr_test_harness *th = umr_create_test_harness(ip_discovery_dump);

			options.test_log = 1;
			options.th = th;
			asic = umr_discover_asic_by_discovery_table(
				(char*)json_object_get_string(answer, "name"),
				&options,
				printf);

			if (asic) {
				umr_scan_config(asic, 0);
				asic->did = did;
			}
			umr_free_test_harness(th);
		} else {
			/* Don't rely on local IP discovery data if available, because
			 * it probably won't match the one on the server.
			 */
			options.force_asic_file = 1;
			asic = umr_discover_asic_by_did(&options, did, printf, &tryipdiscovery);
		}

		if (!asic) {
			fprintf(stderr, "Failed to create asic, aborting.\n");
			abort();
		}

		asic->instance = instance;

		panels.push_back(new InfoPanel(asic));
		panels.push_back(new RegistersPanel(asic));
		panels.push_back(new PowerPanel(asic));
		panels.push_back(new RingsPanel(asic));
		panels.push_back(new TopPanel(asic));
		panels.push_back(new MemoryUsagePanel(asic));
		panels.push_back(new MemoryDebugPanel(asic));
		panels.push_back(new WavesPanel(asic));
		panels.push_back(new KmsPanel(asic));
		panels.push_back(new BufferObjectPanel(asic));

		for (auto panel: panels) {
			panel->store_info(json_object_get_wrapping_value(answer));
		}
	}
	~AsicData() {
		for (auto p: panels)
			delete p;
		umr_close_asic(asic);
	}

	struct umr_options options;
	struct umr_asic *asic;
	std::vector<Panel*> panels;
};

std::vector<JSON_Value*> pending_request;

void send_request(JSON_Value *req, struct umr_asic *asic) {
	if (asic) {
		JSON_Value *a = json_value_init_object();
		json_object_set_number(json_object(a), "did", asic->did);
		json_object_set_number(json_object(a), "instance", asic->instance);
		json_object_set_value(json_object(req), "asic", a);
	}
	pthread_mutex_lock(&mtx);
	pending_request.push_back(req);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mtx);
}

void Panel::send_request(JSON_Value *req) {
	if (asic) {
		JSON_Value *a = json_value_init_object();
		json_object_set_number(json_object(a), "did", asic->did);
		json_object_set_number(json_object(a), "instance", asic->instance);
		json_object_set_value(json_object(req), "asic", a);
	}
	pthread_mutex_lock(&mtx);
	pending_request.push_back(req);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mtx);
}

AsicData *answer_to_asic_data(std::vector<AsicData*> *asics, JSON_Object *request) {
	JSON_Object *asc = json_object(json_object_get_value(request, "asic"));
	if (!asc)
		return NULL;

	int did = json_object_get_number(asc, "did");
	int instance = json_object_get_number(asc, "instance");

	for (int i = 0; i < (int) asics->size(); i++) {
		if ((*asics)[i]->asic->did == did && (*asics)[i]->asic->instance == instance)
			return (*asics)[i];
	}
	return NULL;
}

static int64_t time_ns(void)
{
   struct timespec ts;
   timespec_get(&ts, CLOCK_MONOTONIC);
   return ts.tv_nsec + ts.tv_sec * 1000000000;
}

static float ping_value = -1;

static void process_response(std::vector<AsicData*> *asics, JSON_Object *response, void *raw_data, unsigned raw_data_size) {
	JSON_Object *request = json_object(json_object_get_value(response, "request"));
	const char *cmd = json_object_get_string(request, "command");
	JSON_Value *error = json_object_get_value(response, "error");

	if (cmd) {
		if (!error && !strcmp(cmd, "enumerate")) {
			JSON_Array *as_array = json_array(json_object_get_value(response, "answer"));
			if (as_array) {
				int s = json_array_get_count(as_array);
				for (int i = 0; i < s; i++) {
					JSON_Object *v = json_object(json_array_get_value(as_array, i));

					char *ip_discovery_dump = NULL;

					int off = json_object_get_number(v, "ip_discovery_offset");
					int len = json_object_get_number(v, "ip_discovery_len");
					if (len > 0)
						ip_discovery_dump = strndup(&((char*)raw_data)[off], len);

					AsicData *d = new AsicData(v,
						ip_discovery_dump,
						json_object_get_number(v, "did"),
						json_object_get_number(v, "instance"));
					asics->push_back(d);

					free(ip_discovery_dump);
				}
				return;
			}
		}
		if (!error && !strcmp(cmd, "ping")) {
			int64_t pong = time_ns();
			int64_t ping = (int64_t)json_object_get_number(request, "ts");
			ping_value =(pong - ping) / 1000000.0f;
			return;
		}

		AsicData *data = answer_to_asic_data(asics, request);

		if (data) {
			JSON_Value *v = json_object_get_value(response, "answer");

			for (auto panel: data->panels) {
				if (panel->asic == data->asic)
					panel->process_server_message(response, raw_data, raw_data_size);
			}
		}
	}

	force_redraw();
}

static void *communication_thread(void *_job) {
	int id = 0;
	char session_folder[PATH_MAX];
	while (id < 1024) {
		struct stat statbuf;
		snprintf(session_folder, sizeof(session_folder), "/tmp/umr_session.%d", id++);
		if (stat(session_folder, &statbuf) == -1 && errno == ENOENT) {
			break;
		}
	}

	int save_to_disk = mkdir(session_folder, 0755) == 0;
	if (!save_to_disk) {
		printf("Failed to create the replay folder (error: %d)\n", errno);
	}

	std::vector<AsicData*> *asics = (std::vector<AsicData*> *)_job;
	int64_t last_ping = time_ns();

	int msg_count = 0;
	while (!done) {
		pthread_mutex_lock(&mtx);
		if (pending_request.empty()) {
			int64_t now = time_ns();

			if (now - last_ping > 1000000000) {
				JSON_Value *req = json_value_init_object();
				json_object_set_string(json_object(req), "command", "ping");
				json_object_set_number(json_object(req), "ts", now);
				last_ping = now;
				pending_request.push_back(req);
			} else {
				struct timespec t;
				clock_gettime(CLOCK_REALTIME, &t);
				t.tv_sec += 1;
				pthread_cond_timedwait(&cond, &mtx, &t);
			}
		}
		for (int i = 0; i < pending_request.size(); i++) {
			void *raw_data = NULL;
			unsigned raw_data_size = 0;
			JSON_Value* req = pending_request[i];
			pthread_mutex_unlock(&mtx);
			bool is_ping = strcmp(json_object_get_string(json_object(req), "command"), "ping") == 0;
			JSON_Value *in = query(lnk, req, &raw_data, &raw_data_size,
										  (save_to_disk && !is_ping) ? session_folder : NULL, msg_count);
			if (!is_ping)
				msg_count++;

			pthread_mutex_lock(&mtx);

			process_response(asics, json_object(in), raw_data, raw_data_size);

			json_value_free(in);
		}
		pending_request.clear();
		pthread_mutex_unlock(&mtx);
	}
	return 0;
}

static int goto_tab_on_next_redraw = -1;
bool kb_shortcut(int keycode) {
	return (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased(SDL_GetScancodeFromKey(keycode))) ||
			goto_tab_on_next_redraw == keycode;
}

void add_vertical_line(const ImVec2& avail) {
	ImVec2 start = ImGui::GetCursorScreenPos();
	start.x -= ImGui::GetStyle().FramePadding.x;
	ImVec2 end = start;
	end.y += avail.y;
	ImGui::GetWindowDrawList()->AddLine(start, end, ImGui::GetColorU32(ImGuiCol_TabActive));
}

GLuint texture_from_qoi_buffer(int width, int height, void *buffer, int buffer_size)
{
	GLuint texture_id;

	qoi_desc desc;
	desc.width = width;
	desc.height = height;
	desc.channels = 4;
	desc.colorspace = QOI_LINEAR;
	void *data = qoi_decode(buffer, buffer_size, &desc, 4);

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, data);
	free(data);

	return texture_id;
}

void goto_tab(int key_code) {
	goto_tab_on_next_redraw = key_code;
	force_redraw();
}

static float gui_scale = 1.0;
float get_gui_scale() {
	return gui_scale;
}

static int replay_up_to(const char *url, std::vector<AsicData*> &asics,
								std::vector<std::string>& replay_commands,
								int idx) {
	char filename[PATH_MAX];
	void *raw_data = NULL;
	int msg_idx = 0, fd;

	while (idx < 0 || msg_idx <= idx) {
		uint32_t raw_data_size = 0;
		JSON_Value *msg;

		sprintf(filename, "%s/%d.json", url, msg_idx);

		msg = json_parse_file(filename);
		if (msg == NULL) {
			/* We're done replaying everything. */
			break;
		} else {
			JSON_Object *e = json_object(msg);

			if (idx < 0) {
				JSON_Object *req = json_object_get_object(e, "request");
				replay_commands.push_back(json_object_get_string(req, "command"));
			}

			if (json_object_get_boolean(e, "has_raw_data")) {
				sprintf(filename, "%s/%d.raw", url, msg_idx);

				fd = open(filename, O_RDONLY);
				if (fd >= 0) {
					uint32_t s;
					read(fd, &s, sizeof(raw_data_size));
					raw_data_size = le32toh(s);
					raw_data = malloc(raw_data_size);
					read(fd, raw_data, raw_data_size);
					close(fd);
				}
			}

			process_response(&asics, e, raw_data, raw_data_size);

			free(raw_data);
			raw_data = NULL;
			json_value_free(msg);
		}

		msg_idx++;
	}

	return msg_idx - 1;
}

void reset_before_replay(std::vector<AsicData*> &asics) {
	for (auto ad: asics)
		delete ad;
	asics.clear();
}

static int run_gui(const char *url)
{
	pthread_mutexattr_t mat;
	pthread_mutexattr_init(&mat);
	pthread_mutexattr_settype(&mat, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mtx, &mat);
	pthread_cond_init(&cond, NULL);

	bool replay = false;
	int current_replay, max_replay;
	if (url) {
		struct stat statbuf;
		int r = stat(url, &statbuf);
		if (r == 0 && S_ISDIR(statbuf.st_mode)) {
			replay = true;
		} else {
			#if UMR_SERVER
			int rv;
			if ((lnk.sock = nn_socket(AF_SP, NN_REQ)) < 0) {
				exit(1);
			}
			if ((rv = nn_connect (lnk.sock, url)) < 0) {
				printf("Error: invalid url '%s'\n", url);
				exit(1);
			}
			int size = 100000000;
			if (nn_setsockopt(lnk.sock, NN_SOL_SOCKET, NN_RCVMAXSIZE, &size, sizeof(size)) < 0) {
				exit(0);
			}
			lnk.use_sock = true;
			lnk.endpoint = rv;
			#else
			printf("Error: UMR remote GUI feature was not enabled at build time.\n");
			exit(1);
			#endif
		}
	} else {
		lnk.use_sock = false;
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	std::vector<AsicData*> asics;

	pthread_t t_id;
	std::vector<std::string> replay_commands;
	if (replay) {
		current_replay = replay_up_to(url, asics, replay_commands, -1);
	} else {
		pthread_create(&t_id, NULL, communication_thread, &asics);
	}

	ImVec4 clear_color = ImColor(0, 43, 54, 255).Value;

	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags =
		(SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

	char title[512];
	if (lnk.use_sock)
		sprintf(title, "umr (%s)", url);
	else
		strcpy(title, "umr");
	SDL_Window *window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
													  SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	// Initialize OpenGL loader
	if (gladLoadGL() == 0) {
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return 1;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	char ini_path[8096];
	const char *config_filename = NULL;

	const char *bases[] = { "XDG_CONFIG_HOME", "HOME" };
	for (int i = 0; i < 2; i++) {
		if (getenv(bases[i])) {
			const char *base = getenv(bases[i]);
			if (i == 0) {
				strcpy(ini_path, base);
			} else {
				sprintf(ini_path, "%s/.config", base);
			}
			struct stat statbuf;
			if (stat(ini_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
				char *copy = strdup(ini_path);
				sprintf(ini_path, "%s/umr/", copy);
				if (stat(ini_path, &statbuf) == -1 && errno == ENOENT) {
					if (mkdir(ini_path, 0755) == 0) {
						sprintf(ini_path, "%s/umr/umr_gui.ini", copy);
						config_filename = strdup(ini_path);
					}
				} else {
					sprintf(ini_path, "%s/umr/umr_gui.ini", copy);
					config_filename = strdup(ini_path);
				}
				free(copy);
			}
		}
	}

	bool rebuild_scaled_font = false;
	ImFont *scaled_font = NULL;
	float old_scale = 1;

	if (config_filename) {
		FILE *f = fopen(config_filename, "r");
		if (f) {
			if (fscanf(f, "ui_scale=%f", &gui_scale) == 1) {
				gui_scale = std::max(1.0f, std::min(2.0f, gui_scale));
				old_scale = gui_scale;
				if (gui_scale != 1.0)
					rebuild_scaled_font = true;
			}
			fclose(f);
		}
		free((void*)config_filename);
	}

	// Setup Dear ImGui style (todo: support switch to light theme)
	ImGui::StyleColorsDark();

	ImGui::GetStyle().AntiAliasedLines = false;

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	int need_auto_refresh = 0;

	struct timespec before;
	clock_gettime(CLOCK_MONOTONIC, &before);
	// Main loop
	done = false;

	io.FontGlobalScale = 1;
	/* Discover asics */
	JSON_Value *req = json_value_init_object();
	json_object_set_string(json_object(req), "command", "enumerate");
	send_request(req, NULL);

	bool clear_goto_tab_flag = false;
	float previous_ping = -1;

	while (!done) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		float dt = now.tv_sec - before.tv_sec;
		if (now.tv_nsec < before.tv_nsec) {
			dt += ((1000000000 + now.tv_nsec) - before.tv_nsec) * 0.000000001 - 1;
		} else {
			dt += (now.tv_nsec - before.tv_nsec) * 0.000000001;
		}
		memcpy(&before, &now, sizeof(now));

		if (lnk.use_sock && previous_ping != ping_value) {
			char title[512];
			sprintf(title, "umr (%s, %.1f ms)", url, ping_value);
			previous_ping = ping_value;
			SDL_SetWindowTitle(window, title);
		}

		if (rebuild_scaled_font) {
		    ImFontConfig cfg;
			cfg.SizePixels = 13 * gui_scale;
			scaled_font = ImGui::GetIO().Fonts->AddFontDefault(&cfg);
			ImGui::GetIO().Fonts->Build();
			ImGui_ImplOpenGL3_CreateFontsTexture();
			old_scale = gui_scale;
			ImGui::GetStyle().ScaleAllSizes(gui_scale / old_scale);

			rebuild_scaled_font = false;
			force_redraw();
		}

		SDL_PumpEvents();

		SDL_Event event;
		/* Process all events, so the app get stuck SDL_WaitEvent() when there's nothing to do */
		if (need_auto_refresh && SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 0) {
			need_auto_refresh--;
			/* Skip event processing */
			goto after_event_processing;
		}

		event_handling:
		if (SDL_WaitEventTimeout(&event, 500)) {
			need_auto_refresh = 3;
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
				 event.window.windowID == SDL_GetWindowID(window))
				done = true;
		}

		if (SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) > 0)
			goto event_handling;

		after_event_processing:

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		ImGui::SetNextWindowSize(ImVec2(w, h));
		ImGui::Begin("umr", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
								  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		if (scaled_font)
			ImGui::PushFont(scaled_font);

		ImVec2 topleft = ImGui::GetCursorScreenPos();
		ImVec2 avail = ImGui::GetContentRegionAvail();

		pthread_mutex_lock(&mtx);

		if (replay) {
			const int n_replay = (int)replay_commands.size() - 1;

			ImGui::SameLine();
			ImGui::Text("Replaying: %s", url);
			ImGui::SameLine();
			ImGui::Text("Up to:");
			ImGui::SameLine();
			ImGui::BeginDisabled(current_replay == 0);
			ImGui::SameLine();
			if (ImGui::ArrowButton("left", ImGuiDir_Left)) {
					/* Destroy everything, and replay again. */
					reset_before_replay(asics);
					current_replay--;
					replay_up_to(url, asics, replay_commands, current_replay);
			}
			ImGui::EndDisabled();
			ImGui::BeginDisabled(current_replay >= n_replay);
			ImGui::SameLine();
			if (ImGui::ArrowButton("rt", ImGuiDir_Right)) {
					/* Destroy everything, and replay again. */
					reset_before_replay(asics);
					current_replay++;
					replay_up_to(url, asics, replay_commands, current_replay);
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			char label[256];
			sprintf(label, "%d/%d (%s)", current_replay, n_replay, replay_commands[current_replay].c_str());
			float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 10;
			ImGui::SetNextItemWidth(w);
			if (ImGui::BeginCombo("", label)) {
				for (int i = 0; i <= n_replay; i++) {
					sprintf(label, "%d/%d (%s)", i, n_replay, replay_commands[i].c_str());
					if (ImGui::Selectable(label, i == current_replay) && current_replay != i) {
						/* Destroy everything, and replay again. */
						reset_before_replay(asics);
						current_replay = i;
						replay_up_to(url, asics, replay_commands, current_replay);
					}
				}
				ImGui::EndCombo();
			}
		}

		ImGui::SetNextItemWidth(avail.x / 16);
		if (ImGui::SliderFloat("scale", &gui_scale, 1, 2, "%.1f")) {
			if (gui_scale < 1)
				gui_scale = 1;
			if (gui_scale > 2)
				gui_scale = 2;

			rebuild_scaled_font = true;
		}
		ImGui::SameLine();

		ImGui::BeginTabBar("asics", ImGuiTabBarFlags_FittingPolicyScroll);

		if (asics.empty()) {
			if (url)
				ImGui::Text("No answer from %s yet...", url);
			else if (getuid() != 0 && geteuid() != 0) {
				ImGui::Text("No amdgpu devices found. Try running umr as root/sudo.");
			} else {
				ImGui::Text("No amdgpu devices found");
			}
		}

		for (int i = 0; i < asics.size(); i++) {
			AsicData &data = *asics[i];

			char asic[64];
			sprintf(asic, "%s (instance: %d)", data.asic->asicname, data.asic->instance);
			if (!ImGui::BeginTabItem(asic, NULL))
				continue;

			bool can_send_request = pending_request.empty();

			ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_None);

			if (ImGui::BeginTabItem("#b58900I#ffffffnfo", NULL, kb_shortcut(SDLK_i) ? ImGuiTabItemFlags_SetSelected : 0)) {
				data.panels[0]->display(dt, avail, can_send_request);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900R#ffffffegisters", NULL, kb_shortcut(SDLK_r) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[1]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900W#ffffffaves", NULL, kb_shortcut(SDLK_w) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[7]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Rin#b58900g#ffffffs", NULL, kb_shortcut(SDLK_g) ? ImGuiTabItemFlags_SetSelected : 0)) {
				data.panels[3]->display(dt, avail, can_send_request);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900P#ffffffower", NULL, kb_shortcut(SDLK_p) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[2]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900M#ffffffemory Usage", NULL, kb_shortcut(SDLK_m) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[5]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900T#ffffffop", NULL, kb_shortcut(SDLK_t) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[4]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("#b58900K#ffffffMS", NULL, kb_shortcut(SDLK_k) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[8]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Memory #b58900I#ffffffnspector", NULL, kb_shortcut(SDLK_i) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[6]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Buffer #b58900O#ffffffjects", NULL, kb_shortcut(SDLK_o) ? ImGuiTabItemFlags_SetSelected : 0)) {
				if (data.panels[9]->display(dt, avail, can_send_request))
					need_auto_refresh = -1;
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();

		if (replay) {
			/* */
		} else if (!pending_request.empty()) {
			avail.x += 2 * ImGui::GetStyle().WindowPadding.x;
			ImVec2 c(avail.x - 10, topleft.y);
			ImGui::SetCursorScreenPos(c);
			ImGui::Spinner("Waiting", 5, 2, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
			need_auto_refresh = -1;
		}

		pthread_mutex_unlock(&mtx);

		if (scaled_font)
			ImGui::PopFont();

		ImGui::End();

		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);

		if (clear_goto_tab_flag)
			goto_tab_on_next_redraw = -1;
		else if (goto_tab_on_next_redraw != -1)
			clear_goto_tab_flag = true;
	}

	pthread_mutex_lock(&mtx);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mtx);

#if UMR_SERVER
	if (lnk.use_sock) {
		nn_shutdown(lnk.sock, lnk.endpoint);
		nn_close(lnk.sock);
	}
#endif

	if (!replay) {
		void *res;
		pthread_join(t_id, &res);
	}

	for (int i = 0; i < asics.size(); i++)
		delete asics[i];

	if (config_filename) {
		FILE *f = fopen(config_filename, "w");
		if (f) {
			fprintf(f, "ui_scale=%.1f\n", gui_scale);
			fclose(f);
		}
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

SyntaxHighlighter::SyntaxHighlighter() : pmatch(NULL), output(NULL) { }
SyntaxHighlighter::~SyntaxHighlighter() {
	free (pmatch);
	free (output);
	for (Def& def: definitions) {
		for (int i = 0; i < def.preg.re_nsub; i++) {
			free((void*)def.colors[i]);
		}
		regfree(&def.preg);
	}
}

void SyntaxHighlighter::add_definition(const char *expression, std::vector<const char *> colors) {
	regex_t preg;
	if (regcomp(&preg, expression, REG_EXTENDED) != 0) {
		printf("REGEXP '%s' failed\n", expression);
		assert(false);
		return;
	}
	Def d;
	d.preg = preg;
	for (int i = 0; i < preg.re_nsub; i++) {
		d.colors[i] = strdup(colors[i]);
	}
	definitions.push_back(d);

	size_t max_nmatch = 0;
	for (const Def& d: definitions) {
		max_nmatch = std::max(max_nmatch, d.preg.re_nsub);
	}
	pmatch = (regmatch_t *)realloc(pmatch, sizeof(regmatch_t) * (max_nmatch + 1));
}

char * SyntaxHighlighter::transform(const char *in) {
	static char out[4096];
	char input[4096];

	if (*in == '\0') {
		out[0] = '\0';
		return out;
	}

	strcpy(input, in);

	for (const Def& def: definitions) {
		int read_cursor = 0;
		int write_cursor = 0;
		int size = strlen(input);
		int end = 0;
		while (true) {
			if ((regexec(&def.preg, &input[read_cursor], def.preg.re_nsub + 1, pmatch, 0) == REG_NOMATCH) ||
				(pmatch[0].rm_so == -1))
				break;

			for (size_t group = 1; group <= def.preg.re_nsub; group++) {
				int start = pmatch[group].rm_so;
				if (start < 0)
					continue;

				/* Copy everything until the match */
				if (start > end) {
					memcpy(&out[write_cursor], &input[read_cursor + end], start - end);
					write_cursor += start - end;
				}

				end = pmatch[group].rm_eo;
				/* Insert the color code, and copy the matching part */
				write_cursor += sprintf(&out[write_cursor], "%s%.*s%s", def.colors[group - 1],
					end - start,
					&input[read_cursor + start],
					"#ffffff");
			}
			read_cursor += end;
			end = 0;
		}

		/* Copy the reminder */
		memcpy(&out[write_cursor], &input[read_cursor], size - read_cursor + 1);
		write_cursor += size - read_cursor + 1;
		out[write_cursor] = '\0';

		/* Use the output as the input for the next regexp */
		strcpy(input, out);
	}

	return out;
}

extern "C" {
	void umr_run_gui(const char *url) {
		run_gui(url);
	}
}
