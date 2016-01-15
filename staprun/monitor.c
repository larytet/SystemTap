#include "staprun.h"
#include <json-c/json.h>
#include <curses.h>
#include <time.h>

#define COMP_FNS 7
#define MAX_INDEX_LEN 5
#define MAX_COLS 256
#define MAX_DATA 8192
#define MAX_HISTORY 8192

#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))

#define HIGHLIGHT(NAME,IDX,CUR) (((IDX == CUR)) ? (NAME "*") : (NAME))

typedef struct History_Queue
{
  char *buf[MAX_HISTORY];
  size_t allocated[MAX_HISTORY];
  int front;
  int back;
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
  insert,
  help
} monitor_state; 

static History_Queue h_queue;
static char probe[MAX_INDEX_LEN];
static WINDOW *monitor_status = NULL;
static WINDOW *monitor_output = NULL;
static int comp_fn_index = 0;
static int probe_scroll = 0;
static int output_scroll = 0;
static time_t elapsed_time = 0;
static time_t start_time = 0;
static int resized = 0;
static int input = 0;
static int rendered = 0;

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

static void write_command(const char *msg, const unsigned len)
{
  char path[PATH_MAX];
  FILE* fp;

  if (sprintf_chk(path, "/proc/systemtap/%s/monitor_control", modname))
    cleanup_and_exit (0, 1);
  if (!(fp = fopen(path, "w")))
    cleanup_and_exit (0, 1);
  if (fwrite(msg, 1, len, fp) != len)
    cleanup_and_exit (0, 1);
  fclose(fp);
}

static void handle_resize()
{
  usleep (500*1000); /* prevent too many allocations */
  endwin();
  refresh();
  if (monitor_status)
    {
      delwin(monitor_status);
      monitor_status = newwin(LINES/2,COLS,0,0);
    }
  if (monitor_output)
    {
      delwin(monitor_output);
      monitor_output = newwin(LINES/2,COLS,LINES/2,0);
      scrollok(monitor_output, TRUE);
    }
  resized = 0;
}

void monitor_winch(__attribute__((unused)) int signum)
{
  resized = 1;
}

void monitor_setup(void)
{
  probe[0] = '\0';
  start_time = time(NULL);
  initscr();
  curs_set(0);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  monitor_status = newwin(LINES/2,COLS,0,0);
  monitor_output = newwin(LINES/2,COLS,LINES/2,0);
  scrollok(monitor_output, TRUE);
}

void monitor_cleanup(void)
{
  monitor_end = 1;
  if (monitor_status) delwin(monitor_status);
  if (monitor_output) delwin(monitor_output);
  endwin();
}

void monitor_render(void)
{
  FILE *monitor_fp;
  FILE *output_fp;
  char path[PATH_MAX];
  time_t current_time = time(NULL);
  int monitor_x, monitor_y, max_cols, max_rows, cur_y, cur_x;

  if (resized)
    handle_resize();

  output_fp = fdopen(monitor_pfd[0], "r");

  /* Render normal systemtap output */
  if (output_fp && monitor_set)
    {
      int i;
      int bytes;

      while ((bytes = getline(&h_queue.buf[h_queue.back],
              &h_queue.allocated[h_queue.back], output_fp)) != -1)
        {
          h_queue.back = (h_queue.back+1) % MAX_HISTORY;
          if (h_queue.count < MAX_HISTORY)
            h_queue.count++;
          else
            h_queue.front = (h_queue.front+1) % MAX_HISTORY;
        }

      wclear(monitor_output);
      for (i = 0; i < h_queue.count-output_scroll; i++)
        wprintw(monitor_output, "%s", h_queue.buf[(h_queue.front+i) % MAX_HISTORY]);

      wrefresh(monitor_output);
    }

  if (!input && rendered && (elapsed_time = current_time - start_time) < monitor_interval)
    return;

  start_time = current_time;
  input = 0;

  getmaxyx(monitor_status, monitor_y, monitor_x);
  max_rows = monitor_y;
  max_cols = MIN(MAX_COLS, monitor_x);

  if (monitor_state != help)
    {
      if (sprintf_chk(path, "/proc/systemtap/%s/monitor_status", modname))
        cleanup_and_exit (0, 1);
      monitor_fp = fopen(path, "r");

      /* Render monitor mode statistics */
      if (monitor_fp)
        {
          char json[MAX_DATA];
          char monitor_str[MAX_COLS];
          char monitor_out[MAX_COLS];
          int printed;
          int col;
          int i;
          size_t bytes;
          size_t width[num_attributes] = {0};
          json_object *jso, *jso_uptime, *jso_uid, *jso_mem,
                      *jso_name, *jso_globals, *jso_probes;
          struct json_object_iterator it, it_end;

          rendered = 1;

          bytes = fread(json, sizeof(char), MAX_DATA, monitor_fp);
          if (!bytes)
            cleanup_and_exit (0, 1);
          fclose(monitor_fp);

          jso = json_tokener_parse(json);
          if (!jso)
            cleanup_and_exit(0, 1);
          json_object_object_get_ex(jso, "uptime", &jso_uptime);
          json_object_object_get_ex(jso, "uid", &jso_uid);
          json_object_object_get_ex(jso, "memory", &jso_mem);
          json_object_object_get_ex(jso, "module_name", &jso_name);
          json_object_object_get_ex(jso, "globals", &jso_globals);
          json_object_object_get_ex(jso, "probes", &jso_probes);

          wclear(monitor_status);

          if (monitor_state == insert)
            mvwprintw(monitor_status, max_rows-1, 0, "enter probe index: %s\n", probe);
          else
            mvwprintw(monitor_status, max_rows-1, 0,
                      "press h for help, q to quit\n");
          wmove(monitor_status, 0, 0);

          wprintw(monitor_status, "uptime: %s uid: %s memory: %s\n",
                  json_object_get_string(jso_uptime),
                  json_object_get_string(jso_uid),
                  json_object_get_string(jso_mem));

          wprintw(monitor_status, "module_name: %s\n",
                  json_object_get_string(jso_name));

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
                  wprintw(monitor_status, "%s\n", monitor_out);
                  col = 0;
                }
            }
          if (col != 0)
            wprintw(monitor_status, "%s\n", monitor_out);

          /* Find max width of each field for alignment uses */
          for (i = 0; i < json_object_array_length(jso_probes); i++)
            {
              json_object *probe, *field;
              probe = json_object_array_get_idx(jso_probes, i);

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

          json_object_array_sort(jso_probes, comp_fn[comp_fn_index]);

          wprintw(monitor_status, "\n%*s\t%*s\t%*s\t%*s\t%*s\t%*s\t%s\n",
                  width[p_index], HIGHLIGHT("index", p_index, comp_fn_index),
                  width[p_state], HIGHLIGHT("state", p_state, comp_fn_index),
                  width[p_hits], HIGHLIGHT("hits", p_hits, comp_fn_index),
                  width[p_min], HIGHLIGHT("min", p_min, comp_fn_index),
                  width[p_avg], HIGHLIGHT("avg", p_avg, comp_fn_index),
                  width[p_max], HIGHLIGHT("max", p_max, comp_fn_index),
                  HIGHLIGHT("name", p_name, comp_fn_index));
          getyx(monitor_status, cur_y, cur_x);
          for (i = probe_scroll; i < MIN(json_object_array_length(jso_probes),
                probe_scroll+max_rows-cur_y-2); i++)
            {
              json_object *probe, *field;
              probe = json_object_array_get_idx(jso_probes, i);
              json_object_object_get_ex(probe, "index", &field);
              wprintw(monitor_status, "%*s\t", width[p_index], json_object_get_string(field));
              json_object_object_get_ex(probe, "state", &field);
              wprintw(monitor_status, "%*s\t", width[p_state], json_object_get_string(field));
              json_object_object_get_ex(probe, "hits", &field);
              wprintw(monitor_status, "%*s\t", width[p_hits], json_object_get_string(field));
              json_object_object_get_ex(probe, "min", &field);
              wprintw(monitor_status, "%*s\t", width[p_min], json_object_get_string(field));
              json_object_object_get_ex(probe, "avg", &field);
              wprintw(monitor_status, "%*s\t", width[p_avg], json_object_get_string(field));
              json_object_object_get_ex(probe, "max", &field);
              wprintw(monitor_status, "%*s\t", width[p_max], json_object_get_string(field));
              getyx(monitor_status, cur_y, cur_x);
              json_object_object_get_ex(probe, "name", &field);
              wprintw(monitor_status, "%.*s", max_cols-cur_x-1, json_object_get_string(field));
              wprintw(monitor_status, "\n");
            }

          wrefresh(monitor_status);

          /* Free allocated memory */
          json_object_put(jso);
        }
    } else {
      /* Render help page */
      rendered = 0;
      wclear(monitor_status);
      wprintw(monitor_status, "MONITOR MODE COMMANDS\n");
      wprintw(monitor_status, "h - Display help page.\n");
      wprintw(monitor_status, "r - Reset global variables to initial state, zeroes if unset.\n");
      wprintw(monitor_status, "s - Rotate sort columns for probes.\n");
      wprintw(monitor_status, "t - Open prompt to enter a probe index to toggle.\n");
      wprintw(monitor_status, "q - Quit monitor mode/go back to status from help page.\n");
      wprintw(monitor_status, "j/DownArrow - Scroll down the probe list.\n");
      wprintw(monitor_status, "k/UpArrow - Scroll up the probe list.\n");
      wprintw(monitor_status, "d/PageDown - Scroll down the output by one page.\n");
      wprintw(monitor_status, "u/PageUp - Scroll up the probe list by one page.\n");
      mvwprintw(monitor_status, max_rows-1, 0, "press q to go back\n");
      wrefresh(monitor_status);
    }
}

void monitor_input(void)
{
  static int i = 0;
  int ch;
  int max_rows, max_cols;

  switch (monitor_state)
    {
      case normal:
        ch = getch();
        switch (ch)
          {
            case 'j': /* Fallthrough */
            case KEY_DOWN:
              probe_scroll++;
              break;
            case 'k': /* Fallthrough */
            case KEY_UP:
              probe_scroll--;
              probe_scroll = MAX(0, probe_scroll);
              break;
            case 'd': /* Fallthrough */
            case KEY_NPAGE:
              getmaxyx(monitor_status, max_rows, max_cols);
              (void) max_cols; /* Unused */
              output_scroll -= max_rows-1;
              output_scroll = MAX(0, output_scroll);
              break;
            case 'u': /* Fallthrough */
            case KEY_PPAGE:
              getmaxyx(monitor_status, max_rows, max_cols);
              (void) max_cols; /* Unused */
              output_scroll += max_rows-1;
              output_scroll = MIN(MAX(0, h_queue.count-max_rows+1), output_scroll);
              break;
            case 'q':
              cleanup_and_exit (0, 0);
              break;
            case 's':
              comp_fn_index++;
              if (comp_fn_index == COMP_FNS)
                comp_fn_index = 0;
              break;
            case 'r':
              write_command("reset", 5);
              break;
            case 't':
              monitor_state = insert;
              break;
            case 'h':
              monitor_state = help;
              break;
          }
        if (ch != ERR)
          input = 1;
        break;
      case insert:
        ch = getch();
        if (ch == '\n' || i == MAX_INDEX_LEN)
          {
            write_command(probe, i);
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
      case help:
        ch = getch();
        if(ch == 'q')
          monitor_state = normal;
        break;
    }
}
