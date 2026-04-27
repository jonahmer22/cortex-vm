#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/ui_html.h"
#include "../include/favicon_png.h"
#include "../include/cm_css.h"
#include "../include/cm_theme_css.h"
#include "../include/cm_js.h"
#include "../include/cm_gas_js.h"

#include "../include/core.h"
#include "../include/defs.h"
#include "../include/header.h"
#include "../include/utils.h"
#include "../include/server.h"
#include "../deps/arena/arena.h"

static char *g_argv0;
static const char *g_source_path;

// interactive run session

static struct {
	pid_t pid;
	int stdin_wr;
	int stdout_rd;
	int stderr_rd;
	int active;
	char binpath[64];
} g_irun = {
	.pid = -1,
	.stdin_wr = -1,
	.stdout_rd = -1,
	.stderr_rd = -1
};

static void irun_cleanup(void){
	if(!g_irun.active)
		return;

	if(g_irun.pid > 0){
		kill(g_irun.pid, SIGKILL);
		waitpid(g_irun.pid, NULL, 0);
		g_irun.pid = -1;
	}
	if(g_irun.stdin_wr >= 0){
		close(g_irun.stdin_wr);
		g_irun.stdin_wr  = -1;
	}
	if(g_irun.stdout_rd >= 0){
		close(g_irun.stdout_rd);
		g_irun.stdout_rd = -1;
	}
	if(g_irun.stderr_rd >= 0){
		close(g_irun.stderr_rd);
		g_irun.stderr_rd = -1;
	}
	if(g_irun.binpath[0]){
		unlink(g_irun.binpath);
		g_irun.binpath[0] = '\0';
	}

	g_irun.active = 0;
}

// debug session state

static struct {
	Arena *code_arena;
	Arena *stack_arena;
	uint64_t *code_base;
	uint64_t *stack_base;
	uint64_t regs[64];
	uint64_t file_length;
	uint64_t extensions;
	uint64_t exit_code;
	int running;
	int active;
	int stdin_wr;
	int stdin_pipe_rd;
	int saved_stdin;
} g_debug;

// string helpers

static char *json_escape(const char *str){
	if(!str)
		str = "";

	size_t len = strlen(str);
	size_t cap = len * 2 + 8;
	char *out = malloc(cap);

	size_t j = 0;
	for(size_t i = 0; i < len; i++){
		if(j >= cap - 6){
			cap *= 2;
			char *tmp = realloc(out, cap);
			if(!tmp){ free(out); return strdup(""); }
			out = tmp;
		}

		unsigned char c = (unsigned char)str[i];
		switch(c){
			case '"':{
				out[j++] = '\\';
				out[j++] = '"';
				break;
			}
			case '\\':{
				out[j++] = '\\';
				out[j++] = '\\';
				break;
			}
			case '\n':{
				out[j++] = '\\';
				out[j++] = 'n';
				break;
			}
			case '\r':{
				out[j++] = '\\';
				out[j++] = 'r';
				break;
			}
			case '\t':{
				out[j++] = '\\';
				out[j++] = 't';
				break;
			}
			default:{
				if(c < 0x20){
					j += sprintf(out + j, "\\u%04x", c);
				}
				else{
					out[j++] = c;
				}
				break;
			}
		}
	}

	out[j] = '\0';
	return out;
}

static char *json_get_string(const char *json, const char *key){
	char pattern[256];
	snprintf(pattern, sizeof(pattern), "\"%s\":", key);

	const char *pos = strstr(json, pattern);
	if(!pos)
		return NULL;

	pos += strlen(pattern);
	while(*pos == ' ' || *pos == '\t')
		pos++;

	if(*pos != '"')
		return NULL;
	pos++;

	size_t cap = 4096, len = 0;
	char *out = malloc(cap);
	while(*pos && *pos != '"'){
		if(len >= cap - 2){
			cap *= 2;
			char *tmp = realloc(out, cap);

			if(!tmp){
				free(out);
				return NULL;
			}

			out = tmp;
		}

		if(*pos == '\\'){
			pos++;
			switch(*pos){
				case '"':{
					out[len++] = '"';
					break;
				}
				case '\\':{
					out[len++] = '\\';
					break;
				}
				case 'n':{
					out[len++] = '\n';
					break;
				}
				case 'r':{
					out[len++] = '\r';
					break;
				}
				case 't':{
					out[len++] = '\t';
					break;
				}
				case '/':{
					out[len++] = '/';
					break;
				}
				default:{
					out[len++] = '\\';
					out[len++] = *pos;
					break;
				}
			}
		}
		else{
			out[len++] = *pos;
		}

		pos++;
	}

	out[len] = '\0';
	return out;
}

// parse breakpoints array from json body: {"breakpoints":[0,5,10]}
static int parse_breakpoints(const char *body, uint64_t *bps, int max_bps){
	const char *pos = strstr(body, "\"breakpoints\":[");
	if(!pos)
		return 0;
	pos += 15;

	int count = 0;
	while(count < max_bps && *pos && *pos != ']'){
		while(*pos == ' ' || *pos == ',')
			pos++;

		if(*pos == ']' || !*pos)
			break;

		char *end;
		bps[count++] = (uint64_t)strtoull(pos, &end, 10);

		if(end == pos)
			break;
		pos = end;
	}

	return count;
}

// http i/o

static char *read_request(int fd, size_t *out_len){
	// poll briefly for the first byte. Browsers open speculative
	// connections that never send anything; without this we'd block here
	// for the full SO_RCVTIMEO on every one of them, serializing real
	// requests behind dead ones. 500ms is generous for localhost.
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int pr = poll(&pfd, 1, 500);
	if(pr <= 0)
		return NULL;	// idle speculative connection or error: drop it
	if(!(pfd.revents & POLLIN))
		return NULL;

	// once data is arriving, give the rest of the request a real timeout
	struct timeval tv = {
		.tv_sec = 2,
		.tv_usec = 0
	};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	size_t cap = 8192, len = 0;
	char *buf = calloc(1, cap);

	while(!strstr(buf, "\r\n\r\n")){
		if(len >= cap - 1){
			cap *= 2;
			char *tmp = realloc(buf, cap);

			if(!tmp){
				free(buf);
				return NULL;
			}

			buf = tmp;
		}

		ssize_t n = read(fd, buf + len, cap - len - 1);
		if(n <= 0)
			break;
		len += n;

		buf[len] = '\0';
	}

	char *cl_pos = strcasestr(buf, "content-length:");
	int clen = cl_pos ? atoi(cl_pos + 15) : 0;

	if(clen > 0){
		char *hdr_end = strstr(buf, "\r\n\r\n");

		size_t hdr_len = hdr_end ? (size_t)(hdr_end + 4 - buf) : len;
		size_t body_have = len - hdr_len;
		size_t need = hdr_len + (size_t)clen + 1;

		if(need > cap){
			char *tmp = realloc(buf, need);

			if(!tmp){
				free(buf);
				return NULL;
			}

			buf = tmp; cap = need;
		}

		while(body_have < (size_t)clen){
			ssize_t n = read(fd, buf + len, cap - len - 1);

			if(n <= 0)
				break;

			len += n; body_have += n;
		}

		buf[len] = '\0';
	}

	*out_len = len;
	return buf;
}

static void write_all(int fd, const char *buf, size_t len){
	while(len > 0){
		ssize_t n = write(fd, buf, len);
		if(n < 0){
			if(errno == EINTR)
				continue;	// retry on signal interruption
			break;	// real error (EPIPE, etc.)
		}
		if(n == 0)
			break;

		buf += n;
		len -= n;
	}
}

static void send_response(int fd, const char *status, const char *ct, const char *body, size_t blen){
	char hdr[256];
	int hlen = snprintf(hdr, sizeof(hdr), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", status, ct, blen);
	
	write_all(fd, hdr, (size_t)hlen);

	if(body && blen)
		write_all(fd, body, blen);
}

// pipe helpers (for fork+exec runs)

static void drain_pipes(int fd_out, int fd_err, char **out, char **err){
	size_t ocap = 4096, olen = 0;
	size_t ecap = 4096, elen = 0;
	*out = malloc(ocap);
	*err = malloc(ecap);

	while(fd_out >= 0 || fd_err >= 0){
		fd_set rfds;
		FD_ZERO(&rfds);

		if(fd_out >= 0)
			FD_SET(fd_out, &rfds);
		if(fd_err >= 0)
			FD_SET(fd_err, &rfds);

		int maxfd = fd_out > fd_err ? fd_out : fd_err;
		if(select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
			break;

		if(fd_out >= 0 && FD_ISSET(fd_out, &rfds)){
			if(olen >= ocap - 1){
				ocap *= 2;
				char *tmp = realloc(*out, ocap);

				if(!tmp){
					close(fd_out);
					fd_out = -1;
					goto next_err;
				}

				*out = tmp;
			}
			ssize_t n = read(fd_out, *out + olen, ocap - olen - 1);

			if(n <= 0){
				close(fd_out);
				fd_out = -1;
			}
			else
				olen += n;
		}
		next_err:
		if(fd_err >= 0 && FD_ISSET(fd_err, &rfds)){
			if(elen >= ecap - 1){
				ecap *= 2;
				char *tmp = realloc(*err, ecap);

				if(!tmp){
					close(fd_err);
					fd_err = -1;
					goto done;
				}

				*err = tmp;
			}
			ssize_t n = read(fd_err, *err + elen, ecap - elen - 1);
			if(n <= 0){
				close(fd_err);
				fd_err = -1;
			}
			else
				elen += n;
		}
		done:;
	}
	(*out)[olen] = '\0';
	(*err)[elen] = '\0';
}

static int parse_regs(char *err, uint64_t regs[64]){
	char *marker = strstr(err, "{\"regs\":[");
	if(!marker)
		return 0;

	char *p = marker + strlen("{\"regs\":[");
	for(int i = 0; i < 64; i++){
		regs[i] = (uint64_t)strtoull(p, &p, 10);
		if(*p == ',')
			p++;
	}

	while(marker > err && (*(marker-1) == '\n' || *(marker-1) == ' '))
		marker--;
	*marker = '\0';

	return 1;
}

static int run_vm_argv(const char **argv, const char *stdin_data, size_t stdin_len, char **out, char **err, uint64_t regs[64], int *exit_code){
	int out_fds[2], err_fds[2], in_fds[2];
	if(pipe(out_fds) != 0 || pipe(err_fds) != 0)
		return 0;

	// write stdin data into a pipe before forking to avoid deadlock
	int devnull = -1;
	if(stdin_data && stdin_len > 0){
		if(pipe(in_fds) != 0){
			close(out_fds[0]);
			close(out_fds[1]);
			close(err_fds[0]);
			close(err_fds[1]);
			return 0;
		}
		size_t written = 0;

		while(written < stdin_len){
			ssize_t n = write(in_fds[1], stdin_data + written, stdin_len - written);
			if(n <= 0)
				break;
			written += (size_t)n;
		}

		close(in_fds[1]);
	}
	else{
		devnull = open("/dev/null", O_RDONLY);
	}

	pid_t pid = fork();
	if(pid == 0){
		close(out_fds[0]);
		close(err_fds[0]);

		dup2(out_fds[1], STDOUT_FILENO);
		close(out_fds[1]);

		dup2(err_fds[1], STDERR_FILENO);
		close(err_fds[1]);

		if(stdin_data && stdin_len > 0){
			dup2(in_fds[0], STDIN_FILENO);
			close(in_fds[0]);
		}
		else{
			dup2(devnull, STDIN_FILENO);
			close(devnull);
		}

		execvp(g_argv0, (char *const *)argv);
		exit(127);
	}

	if(stdin_data && stdin_len > 0)
		close(in_fds[0]);
	if(devnull >= 0)
		close(devnull);

	close(out_fds[1]);
	close(err_fds[1]);

	drain_pipes(out_fds[0], err_fds[0], out, err);

	int status = 0;
	waitpid(pid, &status, 0);
	*exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

	int found = 0;
	if(regs)
		found = parse_regs(*err, regs);
	return found;
}

// write source to a temp .s file and assemble it to binpath
// returns exit code; caller must free *err_out
static int assemble_to_file(const char *source, const char *binpath, char **err_out){
	char tmppath[] = "/tmp/cortex-vm-XXXXXX.s";
	int tmpfd = mkstemps(tmppath, 2);

	write_all(tmpfd, source, strlen(source));
	close(tmpfd);

	const char *argv[] = {g_argv0, "-a", tmppath, "-o", binpath, NULL};
	char *out;
	int exit_code;
	run_vm_argv(argv, NULL, 0, &out, err_out, NULL, &exit_code);
	free(out);
	unlink(tmppath);

	return exit_code;
}

// json builders

static char *build_regs_json(const uint64_t regs[64]){
	size_t cap = 64 * 24; // 20 digits + quotes + comma + margin per register
	char *s = malloc(cap);
	size_t pos = 0;

	for(int i = 0; i < 64; i++)
		pos += (size_t)snprintf(s + pos, cap - pos, "\"%llu\"%s", (unsigned long long)regs[i], i < 63 ? "," : "");

	return s;
}

static char *build_debug_state_json(const char *stdout_str){
	char *esc_out = json_escape(stdout_str);
	char *regs_str = build_regs_json(g_debug.regs);
	char ec[24];
	snprintf(ec, sizeof(ec), "%llu", (unsigned long long)g_debug.exit_code);

	size_t resp_len = strlen(esc_out) + strlen(regs_str) + 128;
	char *resp = malloc(resp_len);
	strcpy(resp, "{\"ok\":true,\"stdout\":\"");
	strcat(resp, esc_out);
	strcat(resp, g_debug.running ? "\",\"running\":true,\"regs\":[" : "\",\"running\":false,\"regs\":[");
	strcat(resp, regs_str);
	strcat(resp, "],\"exit_code\":"); strcat(resp, ec); strcat(resp, "}");

	free(esc_out);
	free(regs_str);
	return resp;
}

// redirect stdout to a tmpfile, run some code, then restore and return the captured text
// caller must free the returned string
#define CAPTURE_STDOUT_START(tmpf, saved) \
	FILE *(tmpf) = tmpfile(); \
	int   (saved) = dup(STDOUT_FILENO); \
	dup2(fileno(tmpf), STDOUT_FILENO);

#define CAPTURE_STDOUT_END(tmpf, saved, out_ptr) \
	fflush(stdout); \
	dup2((saved), STDOUT_FILENO); \
	close(saved); \
	rewind(tmpf); \
	fseek(tmpf, 0, SEEK_END); \
	do { long _sz = ftell(tmpf); rewind(tmpf); \
	     *(out_ptr) = malloc((size_t)_sz + 1); \
	     _sz = (long)fread(*(out_ptr), 1, (size_t)_sz, tmpf); \
	     (*(out_ptr))[_sz] = '\0'; } while(0); \
	fclose(tmpf);

// debug session management

static void debug_teardown(void){
	if(!g_debug.active)
		return;
	
	// destroy VM memory
	heapDestroy();
	if(g_debug.code_arena){
		arenaLocalDestroy(g_debug.code_arena);
		g_debug.code_arena  = NULL;
	}
	if(g_debug.stack_arena){
		arenaLocalDestroy(g_debug.stack_arena);
		g_debug.stack_arena = NULL;
	}
	g_debug.code_base = g_debug.stack_base = NULL;

	// close I/O files
	if(g_debug.stdin_wr >= 0){
		close(g_debug.stdin_wr);
		g_debug.stdin_wr = -1;
	}
	if(g_debug.saved_stdin >= 0){
		dup2(g_debug.saved_stdin, STDIN_FILENO);
		close(g_debug.saved_stdin);
		g_debug.saved_stdin = -1;
	}
	if(g_debug.stdin_pipe_rd >= 0){
		close(g_debug.stdin_pipe_rd);
		g_debug.stdin_pipe_rd = -1;
	}
	g_debug.active = g_debug.running = 0;
}

// read a binary from binpath and load it into the debug session
// returns 1 on success; caller must free *err_out on failure
static int debug_load_binary(const char *binpath, char **err_out){
	size_t fileSize;
	uint64_t *binary = readFileWords(binpath, &fileSize);
	if(!binary || fileSize < HEADER_LEN){
		*err_out = strdup("Failed to read binary");
		free(binary);
		return 0;
	}

	uint64_t magic = 0, fileLength = 0, offset = 0, extensions = 0, dataOffset = 0;
	uint16_t version = 0;
	headerParse(&magic, &version, &fileLength, &offset, &extensions, &dataOffset, binary);
	headerValidate(&magic, &version, &fileSize, &fileLength, &offset, &extensions, &dataOffset);

	size_t code_words = fileLength - HEADER_LEN;
	g_debug.code_arena = arenaLocalInit();
	g_debug.stack_arena = arenaLocalInit();
	g_debug.code_base = arenaLocalAlloc(g_debug.code_arena,  sizeof(uint64_t) * code_words);
	g_debug.stack_base = arenaLocalAlloc(g_debug.stack_arena, STACKSIZE);

	for(size_t i = HEADER_LEN; i < fileLength; i++)
		g_debug.code_base[i - HEADER_LEN] = binary[i];
	free(binary);

	g_debug.file_length = code_words;
	g_debug.extensions = extensions;
	memset(g_debug.regs, 0, sizeof(g_debug.regs));
	g_debug.regs[PC] = offset - HEADER_LEN;
	g_debug.regs[SP] = (uint64_t)STACK_ADDR;
	g_debug.exit_code = 0;
	g_debug.running = 1;
	g_debug.active = 1;
	g_debug.stdin_wr = -1;
	g_debug.stdin_pipe_rd = -1;
	g_debug.saved_stdin = -1;
	*err_out = NULL;

	return 1;
}

// check if the current instruction is a READ syscall with no data in the stdin pipe
// returns 1 if step() would block waiting for user input
static int is_waiting_for_input(void){
	uint64_t pc = g_debug.regs[PC];
	if(pc >= g_debug.file_length)
		return 0;

	uint64_t instr = g_debug.code_base[pc];
	if(((instr >> 56) & 0xff) != OP_SYS)
		return 0;
	if(((instr >> 48) & 0xff) != FN_SYSCALL)
		return 0;

	uint64_t sysno = g_debug.regs[31]; // A13
	if(sysno != SYS_READ_INT  && sysno != SYS_READ_UINT && sysno != SYS_READ_CHAR && sysno != SYS_READ_FLOAT && sysno != SYS_READ_STR)
		return 0;

	fd_set rfds; FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	struct timeval tv = {0, 0};
	return select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) == 0;
}

// request handlers

static void handle_index(int fd){
	send_response(fd, "200 OK", "text/html; charset=utf-8", (const char *)UI_HTML, UI_HTML_LEN);
}

static void handle_favicon(int fd){
	send_response(fd, "200 OK", "image/png", (const char *)FAVICON_PNG, FAVICON_PNG_LEN);
}

static void handle_cm_css(int fd){
	send_response(fd, "200 OK", "text/css", (const char *)CM_CSS, CM_CSS_LEN);
}

static void handle_cm_theme_css(int fd){
	send_response(fd, "200 OK", "text/css", (const char *)CM_THEME_CSS, CM_THEME_CSS_LEN);
}

static void handle_cm_js(int fd){
	send_response(fd, "200 OK", "application/javascript", (const char *)CM_JS, CM_JS_LEN);
}

static void handle_cm_gas_js(int fd){
	send_response(fd, "200 OK", "application/javascript", (const char *)CM_GAS_JS, CM_GAS_JS_LEN);
}

static void handle_source(int fd){
	if(!g_source_path){
		send_response(fd, "200 OK", "application/json", "{\"source\":null}", 15);
		return;
	}

	FILE *f = fopen(g_source_path, "r");
	if(!f){
		send_response(fd, "200 OK", "application/json", "{\"source\":null}", 15);
		return;
	}

	fseek(f, 0, SEEK_END); long flen = ftell(f); fseek(f, 0, SEEK_SET);
	char *content = malloc((size_t)flen + 1);
	(void)fread(content, 1, (size_t)flen, f);
	content[flen] = '\0';
	fclose(f);

	char *esc = json_escape(content);
	free(content);

	size_t rlen = strlen(esc) + 16;
	char *resp = malloc(rlen);

	strcpy(resp, "{\"source\":\"");
	strcat(resp, esc);
	strcat(resp, "\"}");
	free(esc);

	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

static void handle_save(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}

	if(!g_source_path){
		free(source);
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"no_path\":true}", 27);
		return;
	}

	FILE *f = fopen(g_source_path, "w");
	if(!f){
		free(source);
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Cannot write file\"}", 40);
		return;
	}
	fputs(source, f);
	fclose(f);

	free(source);

	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

static void handle_assemble(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}

	char tmppath[] = "/tmp/cortex-vm-XXXXXX.s";
	int tmpfd = mkstemps(tmppath, 2);
	write_all(tmpfd, source, strlen(source)); close(tmpfd); free(source);

	const char *argv[] = {g_argv0, "-an", tmppath, NULL};
	char *out, *err;
	int exit_code;
	run_vm_argv(argv, NULL, 0, &out, &err, NULL, &exit_code);
	free(out);

	unlink(tmppath);

	char *esc_err = json_escape(err); free(err);
	char *resp;
	if(exit_code == 0){
		resp = strdup("{\"ok\":true}");
	}
	else{
		size_t rlen = strlen(esc_err) + 32;
		resp = malloc(rlen);

		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, esc_err);
		strcat(resp, "\"}");
	}
	free(esc_err);

	send_response(fd, "200 OK", "application/json", resp, strlen(resp)); free(resp);
}

static void handle_run(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}
	char *stdin_str = json_get_string(body, "stdin");
	size_t stdin_len = stdin_str ? strlen(stdin_str) : 0;

	char tmppath[] = "/tmp/cortex-vm-XXXXXX.s";
	int tmpfd = mkstemps(tmppath, 2);
	write_all(tmpfd, source, strlen(source));
	close(tmpfd);
	free(source);

	const char *argv[] = {g_argv0, "-arD", tmppath, NULL};
	char *out, *err;
	int exit_code;
	uint64_t regs[64] = {0};
	int ran = run_vm_argv(argv, stdin_str, stdin_len, &out, &err, regs, &exit_code);
	free(stdin_str);

	unlink(tmppath);

	char *esc_out = json_escape(out);
	char *esc_err = json_escape(err);
	free(out);
	free(err);

	if(!ran && exit_code != 0){
		size_t rlen = strlen(esc_err) + 32;
		char *resp = malloc(rlen);

		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, esc_err);
		strcat(resp, "\"}");

		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		free(esc_out);
		free(esc_err);
		return;
	}

	char *regs_str = build_regs_json(regs);
	size_t resp_len = strlen(esc_out) + strlen(esc_err) + strlen(regs_str) + 80;
	char *resp = malloc(resp_len);
	char ec[16]; snprintf(ec, sizeof(ec), "%d", exit_code);
	strcpy(resp, "{\"ok\":true,\"stdout\":\"");
	strcat(resp, esc_out);
	strcat(resp, "\",\"stderr\":\"");
	strcat(resp, esc_err);
	strcat(resp, "\",\"regs\":[");
	strcat(resp, regs_str);
	strcat(resp, "],\"exit_code\":");
	strcat(resp, ec);
	strcat(resp, "}");

	send_response(fd, "200 OK", "application/json", resp, strlen(resp));

	free(resp);
	free(esc_out);
	free(esc_err);
	free(regs_str);
}

// assemble source, then return each code word as a 16-char hex string
static void handle_bytecode(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}

	char binpath[] = "/tmp/cortex-vm-XXXXXX.b";
	int binfd = mkstemps(binpath, 2);
	close(binfd);

	char *err;
	int asm_exit = assemble_to_file(source, binpath, &err);
	free(source);

	if(asm_exit != 0){
		char *esc = json_escape(err);
		free(err);

		size_t rlen = strlen(esc) + 32;
		char *resp = malloc(rlen);

		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, esc);
		strcat(resp, "\"}");

		free(esc);
		unlink(binpath);
		
		send_response(fd, "200 OK", "application/json", resp, strlen(resp));
		
		free(resp);
		return;
	}
	free(err);

	size_t fileSize;
	uint64_t *binary = readFileWords(binpath, &fileSize);
	unlink(binpath);

	if(!binary || fileSize < HEADER_LEN){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Failed to read binary\"}", 43);

		free(binary);
		return;
	}

	uint64_t magic = 0, fileLength = 0, offset = 0, extensions = 0, dataOffset = 0;
	uint16_t version = 0;
	headerParse(&magic, &version, &fileLength, &offset, &extensions, &dataOffset, binary);

	size_t num_words = fileLength - HEADER_LEN;
	uint64_t entry = offset - HEADER_LEN;
	uint64_t data_off = (dataOffset > HEADER_LEN) ? dataOffset - HEADER_LEN : 0;

	size_t words_cap = num_words * 22 + 4;
	char *words_json = malloc(words_cap);
	size_t pos = 0;
	for(size_t i = 0; i < num_words; i++){
		pos += (size_t)snprintf(words_json + pos, words_cap - pos, "\"%016llX\"%s", (unsigned long long)binary[HEADER_LEN + i], i < num_words - 1 ? "," : "");
	}
	free(binary);

	char entry_s[24], doff_s[24];
	snprintf(entry_s, sizeof(entry_s), "%llu", (unsigned long long)entry);
	snprintf(doff_s,  sizeof(doff_s),  "%llu", (unsigned long long)data_off);

	size_t resp_len = strlen(words_json) + 80;
	char *resp = malloc(resp_len);
	strcpy(resp, "{\"ok\":true,\"entry\":");
	strcat(resp, entry_s);
	strcat(resp, ",\"data_offset\":");
	strcat(resp, doff_s);
	strcat(resp, ",\"words\":[");
	strcat(resp, words_json);
	strcat(resp, "]}");
	free(words_json);

	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

static void handle_debug_start(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}
	char *stdin_str = json_get_string(body, "stdin");
	size_t stdin_len = stdin_str ? strlen(stdin_str) : 0;

	debug_teardown();

	char binpath[] = "/tmp/cortex-vm-XXXXXX.b";
	int binfd = mkstemps(binpath, 2);
	close(binfd);

	char *err;
	int asm_exit = assemble_to_file(source, binpath, &err);
	free(source);

	if(asm_exit != 0){
		char *esc = json_escape(err);
		free(err);

		size_t rlen = strlen(esc) + 32;
		char *resp = malloc(rlen);
		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, esc);
		strcat(resp, "\"}");
		free(esc);
		
		unlink(binpath);

		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		return;
	}
	free(err);

	char *load_err;
	if(!debug_load_binary(binpath, &load_err)){
		unlink(binpath);
		free(stdin_str);

		size_t rlen = strlen(load_err) + 32;
		char *resp = malloc(rlen);
		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, load_err);
		strcat(resp, "\"}");
		free(load_err);

		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		return;
	}
	unlink(binpath);

	// always create a pipe so step() reads from it; write end stays open for /debug/input
	{
		int in_fds[2];
		if(pipe(in_fds) != 0){
			send_response(fd, "500 Internal Server Error", "application/json", "{\"ok\":false,\"error\":\"pipe failed\"}", 32);
			return;
		}
		if(stdin_str && stdin_len > 0){
			size_t written = 0;
			while(written < stdin_len){
				ssize_t n = write(in_fds[1], stdin_str + written, stdin_len - written);

				if(n <= 0)
					break;

				written += (size_t)n;
			}
		}
		g_debug.stdin_wr = in_fds[1];
		g_debug.saved_stdin = dup(STDIN_FILENO);
		g_debug.stdin_pipe_rd = in_fds[0];

		dup2(in_fds[0], STDIN_FILENO);
	}
	free(stdin_str);

	char *resp = build_debug_state_json("");
	send_response(fd, "200 OK", "application/json", resp, strlen(resp));

	free(resp);
}

// execute one instruction in-process, capturing stdout
// note: if the vm calls exit() on a fatal error the server will also exit
static void handle_debug_step(int fd){
	if(!g_debug.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"No active debug session\"}", 46);
		return;
	}
	if(!g_debug.running){
		char *resp = build_debug_state_json("");
		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		return;
	}

	if(is_waiting_for_input()){
		char *regs_str = build_regs_json(g_debug.regs);
		size_t rlen = strlen(regs_str) + 80;
		char *resp = malloc(rlen);

		strcpy(resp, "{\"ok\":true,\"waiting_for_input\":true,\"running\":true,\"regs\":[");
		strcat(resp, regs_str); strcat(resp, "]}");
		free(regs_str);

		send_response(fd, "200 OK", "application/json", resp, strlen(resp));
		free(resp); return;
	}

	CAPTURE_STDOUT_START(tmpf, saved_out)

	bool still_running = step(g_debug.regs, g_debug.code_base, g_debug.stack_base, g_debug.file_length, g_debug.extensions, &g_debug.exit_code);
	g_debug.running = still_running ? 1 : 0;

	char *out;
	CAPTURE_STDOUT_END(tmpf, saved_out, &out)

	char *resp = build_debug_state_json(out);
	free(out);
	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

// run until a breakpoint is hit or the program halts; caps at 10M steps
#define MAX_CONTINUE_STEPS 10000000

static void handle_debug_continue(int fd, const char *body){
	if(!g_debug.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"No active debug session\"}", 46);
		return;
	}
	if(!g_debug.running){
		char *resp = build_debug_state_json("");
		send_response(fd, "200 OK", "application/json", resp, strlen(resp));
		free(resp); return;
	}

	uint64_t bps[256];
	int num_bp = parse_breakpoints(body, bps, 256);

	CAPTURE_STDOUT_START(tmpf, saved_out)

	int steps = 0, hit_bp = 0, waiting = 0;
	while(g_debug.running && steps < MAX_CONTINUE_STEPS && !hit_bp){
		if(is_waiting_for_input()){
			waiting = 1;
			break;
		}

		bool still_running = step(g_debug.regs, g_debug.code_base, g_debug.stack_base, g_debug.file_length, g_debug.extensions, &g_debug.exit_code);
		g_debug.running = still_running ? 1 : 0;
		steps++;

		if(!g_debug.running)
			break;
		for(int i = 0; i < num_bp; i++){
			if(g_debug.regs[PC] == bps[i]){
				hit_bp = 1;
				break;
			}
		}
	}

	char *out;
	CAPTURE_STDOUT_END(tmpf, saved_out, &out)

	if(waiting){
		char *esc_out = json_escape(out); free(out);
		char *regs_str = build_regs_json(g_debug.regs);
		size_t rlen = strlen(esc_out) + strlen(regs_str) + 80;
		char *resp = malloc(rlen);
		strcpy(resp, "{\"ok\":true,\"waiting_for_input\":true,\"stdout\":\"");
		strcat(resp, esc_out);
		strcat(resp, "\",\"running\":true,\"regs\":[");
		strcat(resp, regs_str); strcat(resp, "]}");

		free(esc_out);
		free(regs_str);
		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		return;
	}

	char *resp = build_debug_state_json(out);
	free(out);
	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

static void handle_debug_stop(int fd){
	debug_teardown();
	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

// return a hex-word json array for a slice of memory
static char *words_to_json(const uint64_t *words, size_t count){
	if(count == 0)
		return strdup("[]");
	size_t cap = count * 22 + 4;
	char *buf = malloc(cap);

	size_t pos = 0;
	buf[pos++] = '[';
	for(size_t i = 0; i < count; i++){
		pos += (size_t)snprintf(buf + pos, cap - pos, "\"%016llX\"%s", (unsigned long long)words[i], i < count - 1 ? "," : "");
	}

	buf[pos++] = ']';
	buf[pos]   = '\0';

	return buf;
}

#define MEM_MAX_WORDS 8192

static void handle_debug_memory(int fd){
	if(!g_debug.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"No active debug session\"}", 46);
		return;
	}

	// code: all words up to file_length
	size_t code_count = g_debug.file_length < MEM_MAX_WORDS ? g_debug.file_length : MEM_MAX_WORDS;
	char *code_json = words_to_json(g_debug.code_base, code_count);

	// stack: words from base up to SP (stack grows up from STACK_ADDR)
	uint64_t sp = g_debug.regs[SP];
	uint64_t sp_word = (sp >= STACK_ADDR) ? (sp - STACK_ADDR) : 0;
	size_t stack_count = (size_t)(sp_word < MEM_MAX_WORDS ? sp_word : MEM_MAX_WORDS);
	char *stack_json = words_to_json(g_debug.stack_base, stack_count);

	// heap: all allocated words
	uint64_t heap_used = 0;
	uint64_t *heap_data = heapSnapshot(&heap_used);
	size_t heap_count = (size_t)(heap_used < MEM_MAX_WORDS ? heap_used : MEM_MAX_WORDS);
	char *heap_json = words_to_json(heap_data, heap_count);

	char pc_s[24], sp_s[24];
	snprintf(pc_s, sizeof(pc_s), "%llu", (unsigned long long)g_debug.regs[PC]);
	snprintf(sp_s, sizeof(sp_s), "%llu", (unsigned long long)sp_word);

	size_t resp_len = strlen(code_json) + strlen(stack_json) + strlen(heap_json) + 128;
	char *resp = malloc(resp_len);
	strcpy(resp, "{\"ok\":true,\"pc_word\":");
	strcat(resp, pc_s);
	strcat(resp, ",\"sp_word\":");
	strcat(resp, sp_s);
	strcat(resp, ",\"code\":");
	strcat(resp, code_json);
	strcat(resp, ",\"stack\":");
	strcat(resp, stack_json);
	strcat(resp, ",\"heap\":");
	strcat(resp, heap_json);
	strcat(resp, "}");

	free(code_json);
	free(stack_json);
	free(heap_json);
	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

// send a line of input to the active debug session's stdin pipe
static void handle_debug_input(int fd, const char *body){
	if(!g_debug.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"No active debug session\"}", 46);
		return;
	}

	char *line = json_get_string(body, "line");
	if(line && g_debug.stdin_wr >= 0){
		size_t len = strlen(line);
		write_all(g_debug.stdin_wr, line, len);
		write_all(g_debug.stdin_wr, "\n", 1);
	}
	free(line);

	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

// interactive run handlers

// assemble source then fork a child process with live stdin/stdout/stderr pipes
static void handle_irun_start(int fd, const char *body){
	char *source = json_get_string(body, "source");
	if(!source){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"Missing source\"}", 36);
		return;
	}

	irun_cleanup();
	debug_teardown();

	char binpath[] = "/tmp/cortex-vm-XXXXXX.b";
	int binfd = mkstemps(binpath, 2); close(binfd);

	char *err;
	int asm_exit = assemble_to_file(source, binpath, &err);
	free(source);

	if(asm_exit != 0){
		char *esc = json_escape(err); free(err);
		size_t rlen = strlen(esc) + 32;
		char *resp = malloc(rlen);

		strcpy(resp, "{\"ok\":false,\"error\":\"");
		strcat(resp, esc);
		strcat(resp, "\"}");

		free(esc);
		unlink(binpath);

		send_response(fd, "200 OK", "application/json", resp, strlen(resp));

		free(resp);
		return;
	}
	free(err);

	int in_fds[2], out_fds[2], err_fds[2];
	if(pipe(in_fds) != 0 || pipe(out_fds) != 0 || pipe(err_fds) != 0){
		send_response(fd, "500 Internal Server Error", "application/json", "{\"ok\":false,\"error\":\"pipe failed\"}", 32);
		return;
	}

	pid_t pid = fork();
	if(pid == 0){
		dup2(in_fds[0],  STDIN_FILENO);
		close(in_fds[0]);
		close(in_fds[1]);

		dup2(out_fds[1], STDOUT_FILENO);
		close(out_fds[0]);
		close(out_fds[1]);

		dup2(err_fds[1], STDERR_FILENO);
		close(err_fds[0]);
		close(err_fds[1]);

		const char *argv[] = {g_argv0, binpath, NULL};
		execvp(g_argv0, (char *const *)argv);

		exit(127);
	}

	close(in_fds[0]);
	close(out_fds[1]);
	close(err_fds[1]);

	fcntl(out_fds[0], F_SETFL, O_NONBLOCK);
	fcntl(err_fds[0], F_SETFL, O_NONBLOCK);

	g_irun.pid = pid;
	g_irun.stdin_wr = in_fds[1];
	g_irun.stdout_rd = out_fds[0];
	g_irun.stderr_rd = err_fds[0];
	g_irun.active = 1;
	strncpy(g_irun.binpath, binpath, sizeof(g_irun.binpath) - 1);

	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

// write a line of input to the child's stdin
static void handle_irun_input(int fd, const char *body){
	if(!g_irun.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"error\":\"No active session\"}", 40);
		return;
	}

	char *line = json_get_string(body, "line");
	if(line){
		size_t len = strlen(line);
		write_all(g_irun.stdin_wr, line, len);
		write_all(g_irun.stdin_wr, "\n", 1);
		free(line);
	}

	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

// drain whatever output the child has produced since the last poll; detect eof to signal exit
static void handle_irun_poll(int fd){
	if(!g_irun.active){
		send_response(fd, "200 OK", "application/json", "{\"ok\":false,\"running\":false,\"stdout\":\"\",\"stderr\":\"\",\"exit_code\":0}", 65);
		return;
	}

	// drain stdout (non-blocking)
	size_t out_cap = 4096, out_len = 0;
	char *out_buf = malloc(out_cap);
	int stdout_eof = 0;
	while(1){
		if(out_len >= out_cap - 1){
			out_cap *= 2;
			out_buf = realloc(out_buf, out_cap);
		}

		ssize_t n = read(g_irun.stdout_rd, out_buf + out_len, out_cap - out_len - 1);

		if(n > 0){
			out_len += (size_t)n;
			continue;
		}
		if(n == 0){
			stdout_eof = 1;
		}
		break;
	}
	out_buf[out_len] = '\0';

	// drain stderr (non-blocking)
	size_t err_cap = 4096, err_len = 0;
	char *err_buf = malloc(err_cap);
	while(1){
		if(err_len >= err_cap - 1){
			err_cap *= 2;
			err_buf = realloc(err_buf, err_cap);
		}

		ssize_t n = read(g_irun.stderr_rd, err_buf + err_len, err_cap - err_len - 1);
		if(n > 0){
			err_len += (size_t)n;
			continue;
		}
		break;
	}
	err_buf[err_len] = '\0';

	int running = 1, exit_code = 0;
	if(stdout_eof){
		int status = 0;
		waitpid(g_irun.pid, &status, 0);
		exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
		g_irun.pid = -1;
		irun_cleanup();
		running = 0;
	}

	char *esc_out = json_escape(out_buf);
	char *esc_err = json_escape(err_buf);
	free(out_buf);
	free(err_buf);

	char ec[16];
	snprintf(ec, sizeof(ec), "%d", exit_code);

	size_t resp_len = strlen(esc_out) + strlen(esc_err) + 80;
	char *resp = malloc(resp_len);
	strcpy(resp, "{\"ok\":true,\"running\":");
	strcat(resp, running ? "true" : "false");
	strcat(resp, ",\"stdout\":\"");
	strcat(resp, esc_out);
	strcat(resp, "\",\"stderr\":\"");
	strcat(resp, esc_err);
	strcat(resp, "\",\"exit_code\":");
	strcat(resp, ec); strcat(resp, "}");

	free(esc_out);
	free(esc_err);
	send_response(fd, "200 OK", "application/json", resp, strlen(resp));
	free(resp);
}

static void handle_irun_stop(int fd){
	irun_cleanup();
	send_response(fd, "200 OK", "application/json", "{\"ok\":true}", 11);
}

// probe whether a port is available (returns 1 if free, 0 if taken)
static int port_free(int port){
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
		return 0;

	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons((uint16_t)port);

	int ok = (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
	
	close(fd);
	return ok;
}

// scan upward from `start` for the first free TCP port; returns -1 if none found
int serverFindPort(int start){
	for(int p = start; p <= 65535; p++){
		if(port_free(p)) return p;
	}
	return -1;
}

// Routes that read or write process-global state (g_irun, g_debug). These
// must run in the parent process — forking would put any state mutations
// into a child that the parent never sees, breaking subsequent polls.
static int path_is_stateful(const char *path){
	return strncmp(path, "/irun",  5) == 0
	    || strncmp(path, "/debug", 6) == 0;
}

// server loop

void serverStart(int port, char *argv0, const char *sourcePath){
	g_argv0 = argv0;
	g_source_path = sourcePath;
	signal(SIGPIPE, SIG_IGN);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	int yes = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons((uint16_t)port),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	listen(sock, 8);

	printf("Cortex-VM IDE -> http://127.0.0.1:%d\n", port);
	printf("Press Ctrl+C to stop.\n");
	fflush(stdout);

	char cmd[128];
#ifdef __APPLE__
	snprintf(cmd, sizeof(cmd), "open http://127.0.0.1:%d", port);
#else
	snprintf(cmd, sizeof(cmd), "xdg-open http://127.0.0.1:%d >/dev/null 2>&1 &", port);
#endif
	(void)system(cmd);

	for(;;){
		// reap any finished worker children non-blockingly so they
		// don't pile up as zombies. The handful of waitpid()s in irun/debug
		// teardown only cover those specific PIDs; we need a sweep here.
		while(waitpid(-1, NULL, WNOHANG) > 0)
			;

		int cfd = accept(sock, NULL, NULL);
		if(cfd < 0){
			if(errno == EINTR)
				continue;
			break;
		}

		size_t req_len;
		char *req = read_request(cfd, &req_len);
		if(!req){
			close(cfd);
			continue;
		}

		char method[8] = {0}, path[256] = {0};
		sscanf(req, "%7s %255s", method, path);
		char *body = strstr(req, "\r\n\r\n");
		if(body)
			body += 4;
		else
			body = req + req_len;

		// For stateless routes (page assets, /source, /assemble,
		// /run, /save, /bytecode), fork a worker so the accept loop can
		// keep up with browser-parallel asset requests. Stateful routes
		// (g_irun, g_debug) must run in the parent so their state changes
		// persist across requests.
		int in_child = 0;
		if(!path_is_stateful(path)){
			pid_t pid = fork();
			if(pid == 0){
				// child: drop the listening socket, handle, exit
				close(sock);
				signal(SIGPIPE, SIG_IGN);
				in_child = 1;
			}
			else if(pid > 0){
				// parent: close the client fd and move on
				free(req);
				close(cfd);
				continue;
			}
			// fork() < 0: fall through and handle in parent (degraded but correct)
		}

		if(strcmp(method,"GET")  == 0 && strcmp(path,"/") == 0){
			handle_index(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/favicon.png") == 0){
			handle_favicon(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/vendor/codemirror.min.css") == 0){
			handle_cm_css(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/vendor/material-darker.min.css") == 0){
			handle_cm_theme_css(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/vendor/codemirror.min.js") == 0){
			handle_cm_js(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/vendor/gas.js") == 0){
			handle_cm_gas_js(cfd);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/source") == 0){
			handle_source(cfd);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/save") == 0){
			handle_save(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/assemble") == 0){
			handle_assemble(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/run") == 0){
			handle_run(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/bytecode") == 0){
			handle_bytecode(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/debug/start") == 0){
			handle_debug_start(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/debug/step") == 0){
			handle_debug_step(cfd);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/debug/continue") == 0){
			handle_debug_continue(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/debug/stop") == 0){
			handle_debug_stop(cfd);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/debug/input") == 0){
			handle_debug_input(cfd, body);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/debug/memory") == 0){
			handle_debug_memory(cfd);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/irun/start") == 0){
			handle_irun_start(cfd, body);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/irun/input") == 0){
			handle_irun_input(cfd, body);
		}
		else if(strcmp(method,"GET")  == 0 && strcmp(path,"/irun/poll") == 0){
			handle_irun_poll(cfd);
		}
		else if(strcmp(method,"POST") == 0 && strcmp(path,"/irun/stop") == 0){
			handle_irun_stop(cfd);
		}
		else{
			send_response(cfd, "404 Not Found", "text/plain", "Not Found", 9);
		}

		free(req);
		close(cfd);

		// if we're a forked worker, exit now — don't loop back
		// to accept(). The parent (in_child == 0) keeps running.
		if(in_child)
			_exit(0);
	}
	close(sock);
}
