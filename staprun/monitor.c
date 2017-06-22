#include "staprun.h"
#include <termios.h>
#include <unistd.h>

#ifdef HAVE_MONITOR_LIBS

#include <json-c/json.h>
#include <panel.h>
#include <curses.h>
#include <time.h>
#include <string.h>

#define COMP_FNS 7
#define MAX_INDEX_LEN 5
#define MAX_COLS 256
#define MAX_DATA 262144 /* XXX: pass procfs.read().maxsize(NNN) from stap */
#define MAX_HISTORY 8192

#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))

#define HIGHLIGHT(NAME,IDX,CUR) (((IDX == CUR)) ? (NAME "*") : (NAME))

typedef struct History_Queue
{
  char *lines[MAX_HISTORY]; /* each malloc/strdup'd(). */
  int oldest;
  int newest;
  int count;
} History_Queue;

enum probe_attributes
{
  p_index,
  p_state,
  p_hits,
  p_min,
  p_avg,
  p_max,
  p_name,
  num_attributes
};

static enum state
{
  normal,
  exited,
  insert,
} monitor_state;

static History_Queue h_queue;
static pthread_mutex_t mutex;
static char probe[MAX_INDEX_LEN];
static WINDOW *status = NULL;
static WINDOW *output = NULL;
static WINDOW *help = NULL;
static WINDOW *status_border = NULL;
static WINDOW *help_border = NULL;
static PANEL *status_panel = NULL;
static PANEL *output_panel = NULL;
static PANEL *help_panel = NULL;
static PANEL *status_border_panel = NULL;
static PANEL *help_border_panel = NULL;
static int comp_fn_index = 0;
static int num_probes = 0;
static int probe_scroll = 0;
static int output_scroll = 0;
static time_t elapsed_time = 0;
static time_t start_time = 0;
static int resized = 0;
static int input = 0; /* fresh input received in monitor_input() */
static int rendered = 0;
static int status_hidden = 0;
static int active_window = 1; /* 0 for status window, 1 for output window */

/* Forward declarations */
static int comp_index(const void *p1, const void *p2);
static int comp_state(const void *p1, const void *p2);
static int comp_hits(const void *p1, const void *p2);
static int comp_min(const void *p1, const void *p2);
static int comp_avg(const void *p1, const void *p2);
static int comp_max(const void *p1, const void *p2);
static int comp_name(const void *p1, const void *p2);

static int (*comp_fn[COMP_FNS])(const void *p1, const void *p2) =
{
  comp_index,
  comp_state,
  comp_hits,
  comp_min,
  comp_avg,
  comp_max,
  comp_name
};

static int comp_index(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *index1, *index2;
  json_object_object_get_ex(j1, "index", &index1);
  json_object_object_get_ex(j2, "index", &index2);
  return json_object_get_int(index1) - json_object_get_int(index2);
}

static int comp_state(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *state1, *state2;
  json_object_object_get_ex(j1, "state", &state1);
  json_object_object_get_ex(j2, "state", &state2);
  return strcmp(json_object_get_string(state1), json_object_get_string(state2));
}

static int comp_hits(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *hits1, *hits2;
  json_object_object_get_ex(j1, "hits", &hits1);
  json_object_object_get_ex(j2, "hits", &hits2);
  return json_object_get_int(hits2) - json_object_get_int(hits1);
}

static int comp_min(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *min1, *min2;
  json_object_object_get_ex(j1, "min", &min1);
  json_object_object_get_ex(j2, "min", &min2);
  return json_object_get_int(min2) - json_object_get_int(min1);
}

static int comp_avg(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *avg1, *avg2;
  json_object_object_get_ex(j1, "avg", &avg1);
  json_object_object_get_ex(j2, "avg", &avg2);
  return json_object_get_int(avg2) - json_object_get_int(avg1);
}

static int comp_max(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *max1, *max2;
  json_object_object_get_ex(j1, "max", &max1);
  json_object_object_get_ex(j2, "max", &max2);
  return json_object_get_int(max2) - json_object_get_int(max1);
}

static int comp_name(const void *p1, const void *p2)
{
  json_object *j1 = *(json_object**)p1;
  json_object *j2 = *(json_object**)p2;
  json_object *name1, *name2;
  json_object_object_get_ex(j1, "name", &name1);
  json_object_object_get_ex(j2, "name", &name2);
  return strcmp(json_object_get_string(name1), json_object_get_string(name2));
}

static void write_command(const char *msg)
{
  char path[PATH_MAX];
  FILE* fp;
  size_t len = strlen(msg);

  if (sprintf_chk(path, "/proc/systemtap/%s/monitor_control", modname))
    return;
  if (!(fp = fopen(path, "w")))
    return;
  if (fwrite(msg, 1, len, fp) != len)
    {
      fclose(fp);
      return;
    }
  fclose(fp);
}

static void clear_screen()
{
  if (status)
    {
      del_panel(status_panel);
      del_panel(status_border_panel);
      delwin(status);
      delwin(status_border);
      status = NULL;
    }
  if (output)
    {
      del_panel(output_panel);
      delwin(output);
      output = NULL;
    }
  if (help)
    {
      del_panel(help_border_panel);
      del_panel(help_panel);
      delwin(help_border);
      delwin(help);
      help = NULL;
    }
}

static void setup_status_window()
{
  /* Status window not needed when hidden */
  if (!status_hidden)
    {
      status_border = newwin(LINES/2, COLS, 0, 0);
      status_border_panel = new_panel(status_border);
      status = newwin(LINES/2-2, COLS-2, 1, 1);
      status_panel = new_panel(status);
      box(status_border, 0 ,0);
    }
}

static void setup_output_window()
{
  /* Use the entire screen if not displaying status */
  if (status_hidden)
    output = newwin(LINES,COLS,0,0);
  else
    output = newwin(LINES/2,COLS,LINES/2,0);
  output_panel = new_panel(output);
  scrollok(output, TRUE);
}

static void setup_help_window()
{
  help_border = newwin((3.0/4)*LINES+2, (3.0/4)*COLS+2, LINES/8-1, COLS/8-1);
  help = newwin((3.0/4)*LINES, (3.0/4)*COLS, LINES/8, COLS/8);
  help_border_panel = new_panel(help_border);
  help_panel = new_panel(help);
  hide_panel(help_border_panel);
  hide_panel(help_panel);
  box(help_border, 0 ,0);
  wattron(help, A_BOLD);
  wprintw(help, "MONITOR MODE COMMANDS\n");
  wattroff(help, A_BOLD);
  wprintw(help, "h - Show/hide help page\n");
  wprintw(help, "c - Reset all global variables to their initial states\n");
  wprintw(help, "s - Rotate sort column for probes\n");
  wprintw(help, "t - Open a prompt to enter the index of a probe to toggle\n");
  wprintw(help, "p/r - Pause/resume script by toggling off/on all probes\n");
  wprintw(help, "x - Hide/show the status window\n");
  wprintw(help, "q - Quit script\n");
  wprintw(help, "j,k/DownArrow,UpArrow - Scroll down/up by one entry\n");
  wprintw(help, "PgDown/PgUp - Scroll down/up by one page\n");
  wprintw(help, "Home/End - Scroll to beginning/end\n");
  wprintw(help, "Tab - Toggle scroll window\n");
}

static void handle_resize()
{
  usleep (500*1000); /* prevent too many allocations */
  endwin();
  refresh();
  clear_screen();
  setup_status_window();
  setup_output_window();
  setup_help_window();
  resized = 0;
}

void monitor_winch(__attribute__((unused)) int signum)
{
  resized = 1;
}

void monitor_setup(void)
{
  pthread_mutex_init(&mutex, NULL);
  probe[0] = '\0';
  start_time = time(NULL);
  initscr();
  curs_set(0);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  setup_status_window();
  setup_output_window();
  setup_help_window();
}

void monitor_cleanup(void)
{
  pthread_mutex_destroy(&mutex);
  monitor_end = 1;
  endwin();
}

void monitor_render(void)
{
  FILE *monitor_fp;
  char path[PATH_MAX];
  time_t current_time = time(NULL);
  int monitor_x, monitor_y, max_cols, max_rows, cur_y, cur_x;
  int i;
  int discard;

  if (resized)
    handle_resize();

  /* Bound scrolling by window height and entry count */
  getmaxyx(output, max_rows, max_cols);
  output_scroll = MIN(MAX(0, h_queue.count-max_rows+1), output_scroll);

  /* Render previously recorded output */
  wclear(output);
  pthread_mutex_lock(&mutex);
  for (i = 0; i < h_queue.count-output_scroll; i++)
    wprintw(output, "%s", h_queue.lines[(h_queue.oldest+i) % MAX_HISTORY]);
  pthread_mutex_unlock(&mutex);
  update_panels();
  doupdate();

  if (status_hidden)
    return;

  if (!input && rendered && (elapsed_time = current_time - start_time) < monitor_interval)
    return;

  start_time = current_time;
  input = 0;

  getmaxyx(status, monitor_y, monitor_x);
  max_rows = monitor_y;
  max_cols = MIN(MAX_COLS, monitor_x);

  static json_object *jso = NULL; /* survives across refresh calls */
  char json[MAX_DATA];
  size_t bytes = 0;

  /* Render monitor mode statistics */
  if (sprintf_chk(path, "/proc/systemtap/%s/monitor_status", modname))
    return;
  monitor_fp = fopen(path, "r");
  if (monitor_fp)
    {
      bytes = fread(json, sizeof(char), sizeof(json), monitor_fp);
      fclose(monitor_fp);
    }
  if (bytes >= 1)
    {
      /* Free allocated memory */
      if (jso)
        json_object_put(jso);
      jso = json_tokener_parse(json);
    }

  wclear(status);

  if (jso)
    {
      char monitor_str[MAX_COLS];
      char monitor_out[MAX_COLS];
      int printed;
      int col;
      int i;
      size_t width[num_attributes] = {0};
      json_object *jso_uptime, *jso_uid, *jso_mem,
                  *jso_name, *jso_globals, *jso_probe_list;
      struct json_object_iterator it, it_end;

      rendered = 1;

      json_object_object_get_ex(jso, "uptime", &jso_uptime);
      json_object_object_get_ex(jso, "uid", &jso_uid);
      json_object_object_get_ex(jso, "memory", &jso_mem);
      json_object_object_get_ex(jso, "module_name", &jso_name);
      json_object_object_get_ex(jso, "globals", &jso_globals);
      json_object_object_get_ex(jso, "probe_list", &jso_probe_list);
      num_probes = json_object_array_length(jso_probe_list);

      wmove(status, 0, 0);

      wprintw(status, "uptime: %s uid: %s memory: %s\n",
              json_object_get_string(jso_uptime),
              json_object_get_string(jso_uid),
              json_object_get_string(jso_mem));

      wprintw(status, "module_name: %s probes: %d \n",
              json_object_get_string(jso_name),
              num_probes);

      col = 0;
      col += snprintf(monitor_out, max_cols, "globals: ");
      it = json_object_iter_begin(jso_globals);
      it_end = json_object_iter_end(jso_globals);
      while (!json_object_iter_equal(&it, &it_end))
        {
          printed = snprintf(monitor_str, max_cols, "%s: %s ",
                             json_object_iter_peek_name(&it),
                             json_object_get_string(json_object_iter_peek_value(&it))) + 1;

          /* Prevent line folding for globals */
          if ((max_cols - (col + printed)) >= 0)
            {
              col += snprintf(monitor_out+col, printed, "%s", monitor_str);
              json_object_iter_next(&it);
            }
          else if (printed > max_cols)
            {
              /* Skip globals that do not fit on one line */
              json_object_iter_next(&it);
            }
          else
            {
              wprintw(status, "%s\n", monitor_out);
              col = 0;
            }
        }
      if (col != 0)
        wprintw(status, "%s\n", monitor_out);


      /* Find max width of each field for alignment uses */
      for (i = 0; i < num_probes; i++)
        {
          json_object *probe, *field;
          probe = json_object_array_get_idx(jso_probe_list, i);

          json_object_object_get_ex(probe, "index", &field);
          width[p_index] = MAX(width[p_index], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "state", &field);
          width[p_state] = MAX(width[p_state], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "hits", &field);
          width[p_hits] = MAX(width[p_hits], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "min", &field);
          width[p_min] = MAX(width[p_min], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "avg", &field);
          width[p_avg] = MAX(width[p_avg], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "max", &field);
          width[p_max] = MAX(width[p_max], strlen(json_object_get_string(field)));
          json_object_object_get_ex(probe, "name", &field);
          width[p_name] = MAX(width[p_name], strlen(json_object_get_string(field)));
        }

      json_object_array_sort(jso_probe_list, comp_fn[comp_fn_index]);

      if (active_window == 0)
        wattron(status, A_BOLD);
      wprintw(status, "\n%*s\t%*s\t%*s\t%*s\t%*s\t%*s\t%s\n",
              width[p_index], HIGHLIGHT("index", p_index, comp_fn_index),
              width[p_state], HIGHLIGHT("state", p_state, comp_fn_index),
              width[p_hits], HIGHLIGHT("hits", p_hits, comp_fn_index),
              width[p_min], HIGHLIGHT("min", p_min, comp_fn_index),
              width[p_avg], HIGHLIGHT("avg", p_avg, comp_fn_index),
              width[p_max], HIGHLIGHT("max", p_max, comp_fn_index),
              HIGHLIGHT("name", p_name, comp_fn_index));
      if (active_window == 0)
        wattroff(status, A_BOLD);

      getyx(status, cur_y, discard);
      if (probe_scroll >= num_probes)
        probe_scroll = num_probes-1;
      for (i = probe_scroll; i < MIN(num_probes, probe_scroll+max_rows-cur_y); i++)
        {
          json_object *probe, *field;
          probe = json_object_array_get_idx(jso_probe_list, i);
          json_object_object_get_ex(probe, "index", &field);
          wprintw(status, "%*s\t", width[p_index], json_object_get_string(field));
          json_object_object_get_ex(probe, "state", &field);
          wprintw(status, "%*s\t", width[p_state], json_object_get_string(field));
          json_object_object_get_ex(probe, "hits", &field);
          wprintw(status, "%*s\t", width[p_hits], json_object_get_string(field));
          json_object_object_get_ex(probe, "min", &field);
          wprintw(status, "%*s\t", width[p_min], json_object_get_string(field));
          json_object_object_get_ex(probe, "avg", &field);
          wprintw(status, "%*s\t", width[p_avg], json_object_get_string(field));
          json_object_object_get_ex(probe, "max", &field);
          wprintw(status, "%*s\t", width[p_max], json_object_get_string(field));
          getyx(status, discard, cur_x);
          json_object_object_get_ex(probe, "name", &field);
          wprintw(status, "%.*s", max_cols-cur_x-1, json_object_get_string(field));
          wprintw(status, "\n");
        }
      mvwprintw(status, max_rows-1, 0,
                "press h for help\n");
    }
  else /* ! jso */
    {
      wmove(status, 0, 0);
      wprintw(status, "(data not available)");
    }

  if (monitor_state == insert)
    mvwprintw(status, max_rows-1, 0, "enter probe index: %s\n", probe);
  else if (monitor_state == exited)
    mvwprintw(status, max_rows-1, 0,
              "EXITED: press q again to quit\n");

  update_panels();
  doupdate();
  (void) discard; /* Unused */
}

/* Called in staprun/relay.c */
void monitor_remember_output_line(const char* buf, const size_t bytes)
{
  pthread_mutex_lock(&mutex);
  free (h_queue.lines[h_queue.newest]);
  h_queue.lines[h_queue.newest] = strndup(buf, bytes);
  h_queue.newest = (h_queue.newest+1) % MAX_HISTORY;

  if (h_queue.count < MAX_HISTORY)
    h_queue.count++; /* and h_queue.oldest stays at 0 */
  else
    h_queue.oldest = (h_queue.oldest+1) % MAX_HISTORY;
  pthread_mutex_unlock(&mutex);
}

void monitor_exited(void)
{
  monitor_state = exited;
  input = 1;
}

void monitor_input(void)
{
  static int i = 0;
  int ch;
  int max_rows;
  int cur_y;
  int discard;

  getmaxyx(output, max_rows, discard);
  getyx(status, cur_y, discard);

  switch (monitor_state)
    {
      case normal:
      case exited:
        ch = getch();
        switch (ch)
          {
            case '\t':
              active_window ^= 1;
              break;
            case 'j':
            case KEY_DOWN:
              if (active_window == 0)
                probe_scroll++;
              else
                output_scroll--;
              break;
            case 'k':
            case KEY_UP:
              if (active_window == 0)
                probe_scroll--;
              else
                output_scroll++;
              break;
            case KEY_NPAGE:
              if (active_window == 0)
                probe_scroll += max_rows-cur_y;
              else
                output_scroll -= max_rows-1;
              break;
            case KEY_PPAGE:
              if (active_window == 0)
                probe_scroll -= max_rows-cur_y;
              else
                output_scroll += max_rows-1;
              break;
            case KEY_HOME:
              if (active_window == 0)
                probe_scroll = 0;
              else
                output_scroll = h_queue.count-max_rows+1;
              break;
            case KEY_END:
              if (active_window == 0)
                probe_scroll = num_probes-1;
              else
                output_scroll = 0;
              break;
            case 's':
              comp_fn_index++;
              if (comp_fn_index == COMP_FNS)
                comp_fn_index = 0;
              break;
            case 'c':
              write_command("clear");
              break;
            case 'r':
              write_command("resume");
              break;
            case 'p':
              write_command("pause");
              break;
            case 'q':
              if (monitor_state == exited)
                cleanup_and_exit(0, 0 /* error_detected unavailable here */ );
              else
                write_command("quit");
              break;
            case 't':
              monitor_state = insert;
              break;
            case 'h':
              if (panel_hidden(help_panel))
                {
                  show_panel(help_border_panel);
                  show_panel(help_panel);
                }
              else
                {
                  hide_panel(help_border_panel);
                  hide_panel(help_panel);
                }
              break;
            case 'x':
              status_hidden ^= 1;
              handle_resize();
              break;
          }
        probe_scroll = MAX(0, probe_scroll);
        output_scroll = MAX(0, output_scroll);
        if (ch != ERR)
          input = 1;
        break;
      case insert:
        ch = getch();
        if (ch == '\n' || i == MAX_INDEX_LEN)
          {
            write_command(probe);
            monitor_state = normal;
            i = 0;
            probe[0] = '\0';
            rendered = 0;
          }
        else if (ch == KEY_BACKSPACE && i > 0)
          {
            probe[--i] = '\0';
            input = 1;
          }
        else if (ch != ERR && ch != KEY_BACKSPACE)
          {
            probe[i++] = ch;
            probe[i] = '\0';
            input = 1;
          }
        break;
    }
  (void) discard; /* Unused */
}

#else /* ! HAVE_MONITOR_LIBS */

void monitor_winch(int signum) { (void) signum; }
void monitor_setup(void) {}
void monitor_cleanup(void) {}
void monitor_render(void) {}
void monitor_input(void) {}
void monitor_exited(void) {}
void monitor_remember_output_line(const char* buf, const size_t bytes) { (void)buf; (void) bytes; }

#endif

void *redirect_stdin(void *arg)
{
        char path[PATH_MAX];
        int c;
        int fd;

        snprintf(path, PATH_MAX - 25, "/proc/systemtap/%s/__stdin", modname);
        /* destination file may not exist yet */
        while ((fd = open(path, O_WRONLY)) == -1) {
                if (errno != ENOENT) {
                        _perr("Unexpected failure during file open.\n");
                        exit(1);
                }
                usleep(2000);
        }
        while ((c = getchar()) != EOF) {
	        char ch = (char)c;
                if (! write(fd, &ch, sizeof(ch)) && errno != ENOENT) {
                        _perr("Unexpected failure during write.\n");
                        exit(1);
                }
        }
        read_stdin_cleanup();
        return arg;
}

void read_stdin_setup(void)
{
        pthread_t t;

        if (isatty(STDIN_FILENO)) {
                /* enter non-canonical mode */
                struct termios oldterm, newterm;
                if (tcgetattr(STDIN_FILENO, &oldterm) != 0) {
                        _perr("Failed to reconfigure terminal.\n");
                        exit(1);
                }
                newterm = oldterm;
                newterm.c_lflag &= ~ICANON & ~ECHO;
                if (tcsetattr(STDIN_FILENO, TCSANOW, &newterm) != 0) {
                        _perr("Failed to reconfigure terminal.\n");
                        exit(1);
                }
        }
        if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
                _perr("Failed to reconfigure input stream.\n");
                exit(1);
        }
        if (pthread_create(&t, NULL, redirect_stdin, NULL) != 0) {
                _perr("Failed to create thread.\n");
                exit(1);
        }
        if (pthread_detach(t) != 0) {
                _perr("Failed to detach thread.\n");
                exit(1);
        }
        return;
}

void read_stdin_cleanup(void)
{
        if (isatty(STDIN_FILENO)) {
            struct termios newterm;

            if (tcgetattr(STDIN_FILENO, &newterm) != 0) {
                    _perr("Failed to reconfigure terminal.\n");
                    return;
            }
            newterm.c_lflag |= ICANON | ECHO;
            if (tcsetattr(STDIN_FILENO, TCSANOW, &newterm) != 0) {
                   _perr("Failed to reconfigure terminal.\n");
            }
        }
        return;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
