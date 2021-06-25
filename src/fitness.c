// Brian Chrzanowski
// 2021-06-08 20:04:01
//
// my pastebin, with a small, static, instructional webpage
//
// TODO (Brian)
// 1. fix exercise form (style)
// 2. handle exercise form submit
// 3. make exercise list
// 4. make exercise list sortable
// 5. make exercise list exportable
// 6. make a function to read form body data
// 7. make a function to fetch query parameters
// 8. unencode the percent encoded things

#define COMMON_IMPLEMENTATION
#include "common.h"

#define HTTPSERVER_IMPL
#include "httpserver.h"

#include "sqlite3.h"

#include <magic.h>

#define PORT (5000)

struct kvpair {
	char *k;
	char *v;
};

struct kvpairs {
	struct kvpair *kvpair;
	size_t kvpair_len, kvpair_cap;
};

static magic_t MAGIC_COOKIE;

static sqlite3 *db;

// init: initializes the program
void init(char *db_file_name, char *sql_file_name);
// cleanup: cleans up resources open so we can elegantly close the program
void cleanup(void);

// create_tables: execs create * statements on the database
int create_tables(sqlite3 *db, char *fname);

// is_uuid: returns true if the input string is a uuid
int is_uuid(char *id);

// request_handler: the http request handler
void request_handler(struct http_request_s *req);

// rcheck: returns true if the route in req matches the path and method
int rcheck(struct http_request_s *req, char *target, char *method);

// parse_query: parses the query out from the http_string_s
struct kvpairs parse_url_encoded(struct http_string_s parseme);

// free_kvpairs: frees a kvpairs array
void free_kvpairs(struct kvpairs pairs);

// getv: gets the value from a kvpair(s) given the key
char *getv(struct kvpairs pairs, char *k);

// send_file: sends the file in the request, coalescing to '/index.html' from "html/"
int send_file(struct http_request_s *req, struct http_response_s *res, char *path);

// send_error: sends an error
int send_error(struct http_request_s *req, struct http_response_s *res, int errcode);

// SERVER FUNCTIONS
// exercise_post: handles the POSTing of an exercise record
int exercise_post(struct http_request_s *req, struct http_response_s *res);

#define SQLITE_ERRMSG(x) (fprintf(stderr, "Error: %s\n", sqlite3_errstr(rc)))

#define USAGE ("USAGE: %s <dbname>\n")

int main(int argc, char **argv)
{
	struct http_server_s *server;

	if (argc < 2) {
		fprintf(stderr, USAGE, argv[0]);
		return 1;
	}

	init(argv[1], "schema.sql");

	server = http_server_init(PORT, request_handler);

	printf("listening on http://localhost:%d\n", PORT);

	http_server_listen(server);

	cleanup();

	return 0;
}

// rcheck: returns true if the route in req matches the path and method
int rcheck(struct http_request_s *req, char *target, char *method)
{
	struct http_string_s m, t;
	int i;

	if (req == NULL)
		return 0;
	if (target == NULL)
		return 0;
	if (method == NULL)
		return 0;

	m = http_request_method(req);

	for (i = 0; i < m.len && i < strlen(method); i++) {
		if (tolower(m.buf[i]) != tolower(method[i]))
			return 0;
	}

	t = http_request_target(req);

	if (t.len == 1 && t.buf[0] == '/' && strlen(target) > 1)
		return 0;

	// stop checking the target before query parameters
	for (i = 0; i < t.len && i < strlen(target) && target[i] != '?'; i++) {
		if (tolower(t.buf[i]) != tolower(target[i]))
			return 0;
	}

	return 1;
}

// request_handler: the http request handler
void request_handler(struct http_request_s *req)
{
	struct http_response_s *res;
	int rc;

#define CHKERR(E) do { if ((rc) < 0) { send_error(req, res, (E)); } } while (0)

	res = http_response_init();

	if (rcheck(req, "/exercise/form", "GET")) { // send exercise form
		rc = send_file(req, res, "html/exercise.html");
		CHKERR(503);
	} else if (rcheck(req, "/exercise", "POST")) {
		rc = exercise_post(req, res);
		CHKERR(503);
	} else {
		send_error(req, res, 404);
	}
}

// exercise_post: handles the POSTing of an exercise record
int exercise_post(struct http_request_s *req, struct http_response_s *res)
{
	sqlite3_stmt *stmt;
	struct kvpairs data;
	struct http_string_s body;
	int rc;

	body = http_request_body(req);

	data = parse_url_encoded(body);

	// let's check that we have all of the data first
	if (getv(data, "kind") == NULL) {
		return -1;
	}

	if (getv(data, "duration") == NULL) {
		return -1;
	}

#define EXERCISE_POST_SQL ("insert into exercise (kind, duration) values (?,?);")

	rc = sqlite3_prepare_v2(db, EXERCISE_POST_SQL, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		SQLITE_ERRMSG(rc);
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, getv(data, "kind"), -1, NULL);
	sqlite3_bind_int(stmt, 2, atoi(getv(data, "duration")));

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);

	free_kvpairs(data);

	// send a simple success page
	send_file(req, res, "html/exercise_success.html");

	return 0;
}

// send_file: sends the file in the request, coalescing to '/index.html' from "html/"
int send_file(struct http_request_s *req, struct http_response_s *res, char *path)
{
	char *file_data;
	char *mime_type;
	size_t len;

	if (path == NULL) {
		return -1;
	}

	if (access(path, F_OK) == 0) {
		file_data = sys_readfile(path, &len);

		mime_type = (char *)magic_buffer(MAGIC_COOKIE, file_data, len);

		http_response_status(res, 200);
		http_response_header(res, "Content-Type", mime_type);
		http_response_body(res, file_data, len);

		http_respond(req, res);

		free(file_data);
	} else {
		send_error(req, res, 404);
	}

	return 0;
}

// send_error: sends an error
int send_error(struct http_request_s *req, struct http_response_s *res, int errcode)
{
	http_response_status(res, errcode);
	http_respond(req, res);

	return 0;
}

// is_uuid: returns true if the input string is a uuid
int is_uuid(char *id)
{
	int i;

	if (id == NULL) {
		return 0;
	}

	if (strlen(id) < 36) {
		return 0;
	}

	for (i = 0; i < 36; i++) { // ensure our chars are right
		switch (id[i]) {
		case '0': case '1': case '2': case '3': case '4': case '5': case '6':
		case '7': case '8': case '9': case 'a': case 'b': case 'c': case 'd':
		case 'e': case 'f': case 'A': case 'B': case 'C': case 'D': case 'E':
		case 'F': case '-':
				break;
		default:
			return 0;
		}
	}

	// index 13 should always be '4'
	return id[14] == '4';
}

// getv: gets the value from a kvpair(s) given the key
char *getv(struct kvpairs pairs, char *k)
{
	size_t i;

	for (i = 0; i < pairs.kvpair_len; i++) {
		if (streq(pairs.kvpair[i].k, k))
			return pairs.kvpair[i].v;
	}

	return NULL;
}

// parse_query: parses the query out from the http_string_s
struct kvpairs parse_url_encoded(struct http_string_s parseme)
{
	struct kvpairs pairs;
	char *content;
	char *ss, *s;
	int i;

	memset(&pairs, 0, sizeof pairs);

	content = strndup(parseme.buf, parseme.len);

	ss = strchr(content, '?');
	if (ss == NULL) {
		ss = content;
	}

	for (i = 0, s = strtok(ss, "=&"); s; i++, s = strtok(NULL, "=&")) {
		C_RESIZE(&pairs.kvpair);
		printf("%s - %s\n", i % 2 ? "v" : "k", s);

		if (i % 2 == 0) {
			pairs.kvpair[pairs.kvpair_len].k = strdup(s);
		} else {
			pairs.kvpair[pairs.kvpair_len].v = strdup(s);
			pairs.kvpair_len++;
		}
	}

	free(content);

	// debugging
	for (i = 0; i < pairs.kvpair_len; i++) {
		printf("dbg k %s\n", pairs.kvpair[i].k);
		printf("dbg v %s\n", pairs.kvpair[i].v);
	}

	return pairs;
}

// free_kvpairs: frees a kvpairs array
void free_kvpairs(struct kvpairs pairs)
{
	size_t i;

	for (i = 0; i < pairs.kvpair_len; i++) {
		free(pairs.kvpair[i].k);
		free(pairs.kvpair[i].v);
	}

	free(pairs.kvpair);
}

// kvpair: constructor for a kvpair
struct kvpair kvpair(char *k, char *v)
{
	struct kvpair pair;

	pair.k = k;
	pair.v = v;

	return pair;
}

// init: initializes the program
void init(char *db_file_name, char *sql_file_name)
{
	int rc;

	// seed the rng machine if it hasn't been
	pcg_seed(&localrand, time(NULL) ^ (long)printf, (unsigned long)init);

	// setup libmagic
	MAGIC_COOKIE = magic_open(MAGIC_MIME);
	if (MAGIC_COOKIE == NULL) {
		fprintf(stderr, "%s", magic_error(MAGIC_COOKIE));
		exit(1);
	}

	if (magic_load(MAGIC_COOKIE, NULL) != 0) {
		fprintf(stderr, "cannot load magic database - %s\n", magic_error(MAGIC_COOKIE));
		magic_close(MAGIC_COOKIE);
	}

	rc = sqlite3_open(db_file_name, &db);
	if (rc != SQLITE_OK) {
		SQLITE_ERRMSG(rc);
		exit(1);
	}

	sqlite3_db_config(db,SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
	if (rc != SQLITE_OK) {
		SQLITE_ERRMSG(rc);
		exit(1);
	}

	rc = sqlite3_load_extension(db, "./ext_uuid.so", "sqlite3_uuid_init", NULL);
	if (rc != SQLITE_OK) {
		SQLITE_ERRMSG(rc);
		exit(1);
	}

	rc = create_tables(db, sql_file_name);
	if (rc < 0) {
		ERR("Critical error in creating sql tables!!\n");
		exit(1);
	}

#if 0
	char *err;
#define SQL_WAL_ENABLE ("PRAGMA journal_mode=WAL;")
	char *err;
	rc = sqlite3_exec(db, SQL_WAL_ENABLE, NULL, NULL, (char **)&err);
	if (rc != SQLITE_OK) {
		SQLITE_ERRMSG(rc);
		exit(1);
	}
#endif
}

// cleanup: cleans up for a shutdown (probably doesn't ever happen)
void cleanup(void)
{
	sqlite3_close(db);
}

// create_tables: bootstraps the database (and the rest of the app)
int create_tables(sqlite3 *db, char *fname)
{
	sqlite3_stmt *stmt;
	char *sql, *s;
	int rc;

	sql = sys_readfile(fname, NULL);
	if (sql == NULL) {
		return -1;
	}

	for (s = sql; *s;) {
		rc = sqlite3_prepare_v2(db, s, -1, &stmt, (const char **)&s);
		if (rc != SQLITE_OK) {
			ERR("Couldn't Prepare STMT : %s\n", sqlite3_errmsg(db));
			return -1;
		}

		rc = sqlite3_step(stmt);

		//  TODO (brian): goofy hack, ensure that this can really just run
		//  all of our bootstrapping things we'd ever need
		if (rc == SQLITE_MISUSE) {
			continue;
		}

		if (rc != SQLITE_DONE) {
			ERR("Couldn't Execute STMT : %s\n", sqlite3_errmsg(db));
			return -1;
		}

		rc = sqlite3_finalize(stmt);
		if (rc != SQLITE_OK) {
			ERR("Couldn't Finalize STMT : %s\n", sqlite3_errmsg(db));
			return -1;
		}
	}

	return 0;
}

