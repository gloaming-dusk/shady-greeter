/* Authentication and cancellation against a scripted greetd, without PAM. */
#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <shady/shell_plugin.h>

const struct shady_shell_plugin *shady_shell_plugin_entry_v1(uint32_t host_abi,
    const struct shady_shell_api_v1 *host_api, shady_shell_host host_handle);

static struct { char key[64]; char value[1024]; } values[32];
static size_t value_count;
static struct { char name[64]; shady_shell_action_fn fn; void *data; } actions[8];
static size_t action_count;
static shady_shell_watch_fn watch_fn;
static void *watch_data;
static int watch_fd = -1;
static int server = -1;

static void api_log(shady_shell_host h, const char *message) {
    (void)h;
    (void)message;
}

static const char *api_value(shady_shell_host h, const char *key) {
    (void)h;
    for (size_t i = 0; i < value_count; i++)
        if (!strcmp(values[i].key, key)) return values[i].value;
    return NULL;
}

static bool api_set_value(shady_shell_host h, const char *key, const char *value) {
    (void)h;
    size_t i = 0;
    while (i < value_count && strcmp(values[i].key, key)) i++;
    assert(i < sizeof(values) / sizeof(values[0]));
    if (i == value_count) value_count++;
    snprintf(values[i].key, sizeof(values[i].key), "%s", key);
    snprintf(values[i].value, sizeof(values[i].value), "%s", value);
    return true;
}

static bool api_add_action(shady_shell_host h, const char *name,
        shady_shell_action_fn fn, void *data) {
    (void)h;
    snprintf(actions[action_count].name, sizeof(actions[0].name), "%s", name);
    actions[action_count].fn = fn;
    actions[action_count++].data = data;
    return true;
}

static shady_shell_watch api_watch_add(shady_shell_host h, int fd, short events,
        shady_shell_watch_fn fn, void *data) {
    (void)h;
    (void)events;
    watch_fd = fd;
    watch_fn = fn;
    watch_data = data;
    return (shady_shell_watch)&watch_fd;
}

static void api_watch_remove(shady_shell_host h, shady_shell_watch watch) {
    (void)h;
    (void)watch;
    watch_fd = -1;
}

static bool api_theme_color(shady_shell_host h, const char *name, double rgba[4]) {
    (void)h;
    (void)name;
    (void)rgba;
    return false;
}

static const struct shady_shell_api_v1 api = {
    .abi_version = SHADY_SHELL_PLUGIN_ABI_V1,
    .struct_size = sizeof(struct shady_shell_api_v1),
    .log = api_log,
    .set_value = api_set_value,
    .value = api_value,
    .add_action = api_add_action,
    .watch_add = api_watch_add,
    .watch_remove = api_watch_remove,
    .theme_color = api_theme_color,
};

static void action(const char *name, const char *argument) {
    for (size_t i = 0; i < action_count; i++) {
        if (strcmp(actions[i].name, name)) continue;
        actions[i].fn(argument, actions[i].data);
        return;
    }
    assert(!"no such action");
}

static void expect_request(const char *type) {
    struct pollfd pfd = { .fd = server, .events = POLLIN };
    assert(poll(&pfd, 1, 1000) == 1);
    uint32_t len = 0;
    assert(recv(server, &len, sizeof(len), MSG_WAITALL) == (ssize_t)sizeof(len));
    char *body = calloc(1, (size_t)len + 1);
    assert(body && recv(server, body, len, MSG_WAITALL) == (ssize_t)len);
    if (!strstr(body, type)) {
        fprintf(stderr, "expected %s, got %s\n", type, body);
        abort();
    }
    free(body);
}

/* Send greetd's answer and let the plugin read it. */
static void reply(const char *json) {
    uint32_t len = (uint32_t)strlen(json);
    assert(send(server, &len, sizeof(len), 0) == (ssize_t)sizeof(len));
    assert(send(server, json, len, 0) == (ssize_t)len);
    watch_fn(watch_fd, POLLIN, watch_data);
}

static void expect_value(const char *key, const char *want) {
    const char *got = api_value(NULL, key);
    if (!got || strcmp(got, want)) {
        fprintf(stderr, "%s: wanted '%s', got '%s'\n", key, want, got ? got : "(unset)");
        abort();
    }
}

static const char visible[] =
    "{\"type\":\"auth_message\",\"auth_message_type\":\"visible\",\"auth_message\":\"Code:\"}";
static const char success[] = "{\"type\":\"success\"}";


static const char secret[] =
    "{\"type\":\"auth_message\",\"auth_message_type\":\"secret\",\"auth_message\":\"Password:\"}";
static const char auth_error[] =
    "{\"type\":\"error\",\"error_type\":\"auth_error\",\"description\":\"no\"}";

static void expect_no_request(void) {
    char byte;
    assert(recv(server, &byte, 1, MSG_DONTWAIT) == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
}

static void expect_hidden(void) {
    expect_value("greetd.secret", "1");
    expect_value("greetd.prompt", "");
}

static void login(const char *user) {
    action("greetd.login", user);
    expect_request("create_session");
}

static void cancel(void) {
    action("greetd.cancel", "");
    expect_hidden();
    expect_value("greetd.state", "busy");
    expect_request("cancel_session");
    reply(success);
    expect_value("greetd.state", "idle");
    expect_no_request();
}

static void visible_cancel(void) {
    login("alice");
    reply(visible);
    expect_value("greetd.secret", "0");
    cancel();
}

static void visible_error(void) {
    login("alice");
    reply(visible);
    action("greetd.respond", "123456");
    expect_request("post_auth_message_response");
    expect_hidden();
    reply(auth_error);
    expect_hidden();
    expect_request("cancel_session");
    reply(success);
    expect_value("greetd.state", "error");
}

static void switch_at_prompt(void) {
    login("alice");
    reply(visible);
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    expect_hidden();
    expect_request("cancel_session");
    reply(success);
    expect_request("\"username\":\"bob\"");
    reply(secret);
    expect_request("\"response\":\"bob-password\"");
    reply(visible);
    expect_value("greetd.state", "prompt");
    expect_no_request();
    cancel();
}

static void cancel_during_create(void) {
    login("alice");
    action("greetd.respond", "alice-password");
    action("greetd.cancel", "");
    expect_value("greetd.state", "busy");
    expect_no_request();
    action("greetd.respond", "discard-me");
    reply(secret);
    expect_request("cancel_session");
    reply(success);
    expect_value("greetd.state", "idle");
    expect_no_request();
    login("bob");
    reply(secret);
    expect_value("greetd.state", "prompt");
    expect_no_request();
    cancel();
}

static void cancel_during_response(void) {
    login("alice");
    reply(secret);
    action("greetd.respond", "alice-password");
    expect_request("post_auth_message_response");
    action("greetd.cancel", "");
    expect_value("greetd.state", "busy");
    expect_no_request();
    /* Even successful authentication of the abandoned user must cancel. */
    reply(success);
    expect_request("cancel_session");
    reply(success);
    expect_value("greetd.state", "idle");
    expect_no_request();
}

static void switch_during_create(void) {
    login("alice");
    action("greetd.respond", "alice-password");
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    expect_no_request();
    reply(visible);
    expect_request("cancel_session");
    reply(success);
    expect_request("\"username\":\"bob\"");
    reply(secret);
    expect_request("\"response\":\"bob-password\"");
    reply(secret);
    expect_no_request();
    cancel();
}

static void switch_during_response(void) {
    login("alice");
    reply(secret);
    action("greetd.respond", "alice-password");
    expect_request("post_auth_message_response");
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    reply(success);
    expect_request("cancel_session");
    reply(success);
    expect_request("\"username\":\"bob\"");
    reply(secret);
    expect_request("\"response\":\"bob-password\"");
    reply(secret);
    cancel();
}

static void cancel_queued_switch(void) {
    login("alice");
    reply(secret);
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    expect_request("cancel_session");
    action("greetd.cancel", "");
    expect_value("greetd.state", "busy");
    expect_no_request();
    reply(success);
    expect_value("greetd.state", "idle");
    expect_no_request();
    login("charlie");
    reply(secret);
    expect_value("greetd.state", "prompt");
    expect_no_request();
    cancel();
}

static void failed_cancel(void) {
    login("alice");
    reply(secret);
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    expect_request("cancel_session");
    reply(auth_error);
    expect_hidden();
    expect_value("greetd.state", "error");
    assert(watch_fd == -1);
    char byte;
    assert(recv(server, &byte, 1, 0) == 0);
}

static void password_at_visible_prompt(void) {
    login("alice");
    action("greetd.respond", "private-password");
    reply(visible);
    expect_value("greetd.state", "prompt");
    expect_value("greetd.secret", "0");
    expect_no_request();
    action("greetd.respond", "public-code");
    expect_request("\"response\":\"public-code\"");
    reply(secret);
    expect_value("greetd.state", "prompt");
    expect_no_request(); /* The discarded password must not reappear. */
    cancel();
}

static void password_after_info(void) {
    login("alice");
    action("greetd.respond", "private-password");
    reply("{\"type\":\"auth_message\",\"auth_message_type\":\"info\",\"auth_message\":\"Welcome\"}");
    expect_request("{\"type\":\"post_auth_message_response\"}");
    reply(secret);
    expect_request("\"response\":\"private-password\"");
    /* Ignore duplicate answers while an earlier response is outstanding. */
    action("greetd.respond", "do-not-queue");
    reply(secret);
    expect_value("greetd.state", "prompt");
    expect_no_request();
    cancel();
}

static void disconnect_at_visible_prompt(void) {
    login("alice");
    reply(visible);
    close(server);
    server = -1;
    watch_fn(watch_fd, POLLIN | POLLHUP, watch_data);
    expect_hidden();
    expect_value("greetd.state", "error");
    assert(watch_fd == -1);
    action("greetd.respond", "do-not-queue");
    action("greetd.login", "bob");
    expect_hidden();
    expect_value("greetd.state", "error");
}

static void send_failure_at_visible_prompt(void) {
    login("alice");
    reply(visible);
    close(server);
    server = -1;
    action("greetd.respond", "private-password");
    expect_hidden();
    expect_value("greetd.state", "error");
    assert(watch_fd == -1);
}

static void handoff(void) {
    login("alice");
    action("greetd.respond", "private-password");
    reply(secret);
    expect_request("post_auth_message_response");
    reply(success);
    expect_request("start_session");
    action("greetd.cancel", "");
    action("greetd.login", "bob");
    action("greetd.respond", "bob-password");
    expect_value("greetd.state", "busy");
    expect_no_request();
    reply(success);
    expect_value("greetd.state", "done");
    expect_hidden();
    action("greetd.cancel", "");
    action("greetd.login", "bob");
    expect_value("greetd.state", "done");
    expect_no_request();
    FILE *f = fopen(getenv("SHADY_GREETER_STATE"), "r");
    char user[64];
    assert(f && fgets(user, sizeof(user), f) && !strcmp(user, "alice\n"));
    fclose(f);
}

static void passwordless(void) {
    login("alice");
    reply(success);
    expect_request("start_session");
    reply(success);
    expect_value("greetd.state", "done");
    expect_hidden();
}

static void unsolicited_response(void) {
    reply(success);
    expect_value("greetd.state", "idle");
    expect_no_request();
}

static void demo(void) {
    action("greetd.login", "alice");
    expect_value("greetd.state", "prompt");
    action("greetd.respond", "");
    expect_value("greetd.state", "error");
    action("greetd.login", "alice");
    action("greetd.respond", "demo-password");
    expect_value("greetd.state", "done");
    expect_hidden();
}

static void run_test(void (*fn)(void)) {
    char dir[] = "/tmp/shady-greetd-test-XXXXXX";
    assert(mkdtemp(dir));
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/sock", dir);
    char state[sizeof(dir) + 16], session[sizeof(dir) + 16];
    snprintf(state, sizeof(state), "%s/last", dir);
    snprintf(session, sizeof(session), "%s/test.desktop", dir);
    FILE *f = fopen(session, "w");
    assert(f);
    fputs("[Desktop Entry]\nName=Test\nExec=/bin/true\n", f);
    fclose(f);

    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(listener >= 0 && bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listener, 1) == 0);
    setenv("GREETD_SOCK", addr.sun_path, 1);
    setenv("SHADY_GREETER_SESSIONS", dir, 1);
    setenv("SHADY_GREETER_STATE", state, 1);
    if (fn == demo) setenv("SHADY_GREETER_DEMO", "1", 1);
    else unsetenv("SHADY_GREETER_DEMO");
    /* Successful tests must never contact the running desktop. */
    unsetenv("SHADY_SOCKET");
    unsetenv("XDG_RUNTIME_DIR");

    const struct shady_shell_plugin *plugin =
        shady_shell_plugin_entry_v1(SHADY_SHELL_PLUGIN_ABI_V1, &api, NULL);
    assert(plugin && plugin->init(NULL, NULL));
    if (fn != demo) {
        server = accept(listener, NULL, NULL);
        assert(server >= 0);
    }
    action("greetd.session", "test");
    expect_hidden();
    fn();
    plugin->destroy(NULL);
    if (server >= 0) close(server);
    close(listener);
    unlink(addr.sun_path);
    unlink(state);
    unlink(session);
    assert(rmdir(dir) == 0);
}

int main(void) {
    const struct { const char *name; void (*fn)(void); } tests[] = {
#define CASE(name) { #name, name }
        CASE(visible_cancel), CASE(visible_error), CASE(switch_at_prompt),
        CASE(cancel_during_create), CASE(cancel_during_response),
        CASE(switch_during_create), CASE(switch_during_response),
        CASE(cancel_queued_switch), CASE(failed_cancel),
        CASE(password_at_visible_prompt), CASE(password_after_info),
        CASE(disconnect_at_visible_prompt), CASE(send_failure_at_visible_prompt),
        CASE(handoff), CASE(passwordless), CASE(unsolicited_response), CASE(demo),
#undef CASE
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        /* Isolate the plugin's process-global state between scenarios. */
        pid_t pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            alarm(5);
            run_test(tests[i].fn);
            _exit(0);
        }
        int status;
        assert(waitpid(pid, &status, 0) == pid);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "greetd-plugin: FAIL %s (status %d)\n", tests[i].name, status);
            return 1;
        }
        printf("greetd-plugin: PASS %s\n", tests[i].name);
    }
    return 0;
}
