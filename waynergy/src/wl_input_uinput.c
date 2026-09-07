/* uinput-based input handling as a last resort */


#include "wayland.h"
#include "log.h"
#include "fdio_full.h"

#if defined(__linux__)
#include <linux/uinput.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/uinput.h>
#endif


#if !defined(UINPUT_VERSION) || (UINPUT_VERSION < 5)
bool wlInputInitUinput(struct wlContext *ctx)
{
	logDbg("uinput unavailable or too old on this platform");
	return false;
}
#else

struct state_uinput {
	int key_fd;
	int mouse_fd;
	int mouse_x;
	int mouse_y;
	bool have_mouse_pos;
};

#define UINPUT_KEY_MAX 256

static void emit(int fd, int type, int code, int val)
{
	struct input_event ie = {
		.type = type,
		.code = code,
		.value = val,
	};
	if (!write_full(fd, &ie, sizeof(ie), 0)) {
		logPErr("could not send uinput event");
	}
}

static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct state_uinput *ui = input->state;
	emit(ui->mouse_fd, EV_REL, REL_X, dx);
	emit(ui->mouse_fd, EV_REL, REL_Y, dy);
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_uinput *ui = input->state;

	/* Deskflow sends absolute positions.  Sailfish's evdev mouse plugin
	 * expects a relative mouse, so turn consecutive positions into deltas. */
	if (!ui->have_mouse_pos) {
		/* QEvdevMouseHandler starts a newly-created mouse at 0,0. */
		mouse_rel_motion(input, x, y);
		ui->mouse_x = x;
		ui->mouse_y = y;
		ui->have_mouse_pos = true;
		return;
	}

	mouse_rel_motion(input, x - ui->mouse_x, y - ui->mouse_y);
	ui->mouse_x = x;
	ui->mouse_y = y;
}

static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_uinput *ui = input->state;

	emit(ui->mouse_fd, EV_KEY, button, state);
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
	struct state_uinput *ui = input->state;

	if (dx < 0) {
		emit(ui->mouse_fd, EV_REL, REL_HWHEEL, -1);
	} else if (dx > 0) {
		emit(ui->mouse_fd, EV_REL, REL_HWHEEL, 1);
	}
	if (dy < 0) {
		emit(ui->mouse_fd, EV_REL, REL_WHEEL, -1);
	} else if (dy > 0) {
		emit(ui->mouse_fd, EV_REL, REL_WHEEL, 1);
	}
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void key(struct wlInput *input, int code, int state)
{
	struct state_uinput *ui = input->state;

	code -= 8;
	if (code > UINPUT_KEY_MAX) {
		logErr("Keycode %d is unsupported by uinput (max %d), dropping", code, UINPUT_KEY_MAX);
		return;
	}

	emit(ui->key_fd, EV_KEY, code, state);
	emit(ui->key_fd, EV_SYN, SYN_REPORT, 0);
}
static bool key_map(struct wlInput *input, char *map)
{
	logWarn("uinput does not support xkb keymaps -- use raw-keymap instead");
	return true;
}

#define TRY_IOCTL(fd, req, ...) \
       	do { \
		if (ioctl(fd, req, __VA_ARGS__) == -1) { \
			logPDbg("ioctl " #req " failed"); \
			return false; \
		} \
	} while (0)

#define TRY_IOCTL0(fd, req) \
       	do { \
		if (ioctl(fd, req) == -1) { \
			logPDbg("ioctl " #req " failed"); \
			return false; \
		} \
	} while (0)

static bool legacy_dev_setup(int fd, const char *name, int max_x, int max_y)
{
	struct uinput_user_dev uidev;

	memset(&uidev, 0, sizeof(uidev));
	strncpy(uidev.name, name, sizeof(uidev.name) - 1);
	uidev.id.bustype = BUS_VIRTUAL;
	if (max_x >= 0) {
		uidev.absmax[ABS_X] = max_x;
		uidev.absmax[ABS_Y] = max_y;
	}
	if (!write_full(fd, &uidev, sizeof(uidev), 0)) {
		logPErr("could not write legacy uinput device setup");
		return false;
	}
	return true;
}

static bool init_key(struct state_uinput *ui)
{
	int i;

	struct uinput_setup usetup = {
		.id = {
			.bustype = BUS_VIRTUAL,
		},
		.name = "waynergy keyboard",
	};

	TRY_IOCTL(ui->key_fd, UI_SET_EVBIT, EV_SYN);
	TRY_IOCTL(ui->key_fd, UI_SET_EVBIT, EV_KEY);
	for (i = 0; i <= UINPUT_KEY_MAX; ++i) {
		TRY_IOCTL(ui->key_fd, UI_SET_KEYBIT, i);
	}

	if (ioctl(ui->key_fd, UI_DEV_SETUP, &usetup) == -1) {
		logPDbg("UI_DEV_SETUP failed; using legacy uinput setup");
		if (!legacy_dev_setup(ui->key_fd, "waynergy keyboard", -1, -1))
			return false;
	}
	TRY_IOCTL0(ui->key_fd, UI_DEV_CREATE);
	return true;
}

static bool init_mouse(struct wlContext *ctx, struct state_uinput *ui, int max_x, int max_y)
{
	int i;
	(void)max_x;
	(void)max_y;

	struct uinput_setup usetup = {
		.id = {
			.bustype = BUS_VIRTUAL,
		},
		.name = "waynergy mouse",
	};
	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_SYN);
	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_KEY);
	for (i = 0; i < WL_INPUT_BUTTON_COUNT; ++i) {
		TRY_IOCTL(ui->mouse_fd, UI_SET_KEYBIT, ctx->input.button_map[i]);
	}

	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_REL);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_X);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_Y);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_WHEEL);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_HWHEEL);

	if (ioctl(ui->mouse_fd, UI_DEV_SETUP, &usetup) == -1) {
		logPDbg("UI_DEV_SETUP failed; using legacy uinput setup");
		if (!legacy_dev_setup(ui->mouse_fd, "waynergy mouse", -1, -1))
			return false;
	}

	TRY_IOCTL0(ui->mouse_fd, UI_DEV_CREATE);

	return true;
}

static bool reinit_mouse(struct wlInput *input)
{
	struct state_uinput *ui = input->state;
	TRY_IOCTL0(ui->mouse_fd, UI_DEV_DESTROY);
	ui->have_mouse_pos = false;
	return init_mouse(input->wl_ctx, ui, input->wl_ctx->width, input->wl_ctx->height);
}

static void update_geom(struct wlInput *input)
{
	logDbg("uinput: updating geometry for mouse");
	if (!reinit_mouse(input)) {
		logErr("Could not reinitialize uinput for mouse");
	}
}

bool wlInputInitUinput(struct wlContext *ctx)
{
	struct state_uinput *ui;

	if (ctx->uinput_fd[0] == -1 || ctx->uinput_fd[1] == -1) {
		logDbg("Invalid uinput fds");
		return false;
	}

	ui = xmalloc(sizeof(*ui));
	ui->key_fd = ctx->uinput_fd[0];
	ui->mouse_fd = ctx->uinput_fd[1];
	ui->have_mouse_pos = false;
	/* we've consumed these */
	ctx->uinput_fd[0] = -1;
	ctx->uinput_fd[1] = -1;

	/* because we need to know the button map ahead of time, we need
	 * to initialize this first */
	ctx->input = (struct wlInput) {
		.state = ui,
		.wl_ctx = ctx,
		.mouse_motion = mouse_motion,
		.mouse_rel_motion = mouse_rel_motion,
		.mouse_button = mouse_button,
		.mouse_wheel = mouse_wheel,
		.key = key,
		.key_map = key_map,
		.update_geom = update_geom,
	};
	wlLoadButtonMap(ctx);

	if (!init_key(ui))
		goto error;
	if (!init_mouse(ctx, ui, ctx->width, ctx->height))
		goto error;

	logInfo("Using uinput");
	return true;
error:
	if (ui->key_fd != -1)
		close(ui->key_fd);
	if (ui->mouse_fd != -1)
		close(ui->mouse_fd);
	free(ui);
	return false;
}

#endif /* !defined(__linux__) */
