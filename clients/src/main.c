
/**
 * @brief   Lobby program for #FujiNet
 * @author  Thomas Cherryhomes, Eric Carr
 * @email   thom dot cherryhomes at gmail dot com,  eric dot carr at gmail dot com
 * @license gpl v. 3
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <conio.h>
#include <string.h>

#include <fujinet-fuji.h>
#include <fujinet-network.h>

#ifdef __ADAM__
#include <eos.h>
#include <smartkeys.h>
#include <video/tms99x8.h>
#endif

#include "platform.h"
#include "io.h"

#define CREATOR_ID 0x0001 /* FUJINET  */
#define APP_ID     0x01   /* LOBBY    */
#define KEY_ID     0x00   /* USERNAME */

#define LOBBY_ENDPOINT "N:https://lobby.fujinet.online/games"
#define LOBBY_QA_ENDPOINT "N:https://qalobby.fujinet.online/games"

//#define LOBBY_ENDPOINT "N:http://localhost:8080/games"

// Common defines. Undef and redefine for specific platform below
#define LIST_Y 4 // First row of the list, below its heading and divider
#define MAX_PAGE_SIZE  (BOTTOM_PANEL_Y-1-LIST_Y)
#define BACK_KEY "ESC"
#define SCREEN_WIDTH 40
#define BOTTOM_PANEL_ROWS 3
#define PANEL_SPACER "--------"
#define FUJI_HOST_SLOT_COUNT 8
#define FUJI_DEVICE_SLOT_COUNT 8
#define BOTTOM_PANEL_Y (screen_height-BOTTOM_PANEL_ROWS)
#define BOTTOM_PANEL_LEN SCREEN_WIDTH*BOTTOM_PANEL_ROWS
#define MOUNTING "Mounting"
#define COLD_BOOT reboot()


HostSlot host_slots[FUJI_HOST_SLOT_COUNT];
DeviceSlot device_slots[FUJI_DEVICE_SLOT_COUNT];

// Set the platform send to the lobby server based on the platform specified in the makefile

#ifdef __ATARI__
  #define PLATFORM "atari"
  #define ACTION_VERB ", press"
  #define BOOT_KEY "RETURN"
  #define BOOT_WORD "play"
  #define BACKGROUND_COLOR 0x90
  #define FOREGROUND_COLOR 0xff
#endif

#ifdef __APPLE2__
  #define PLATFORM "apple2"
  #define ACTION_VERB ", press"
  #define BOOT_KEY "RETURN"
  #define BOOT_WORD "boot"
#endif

#ifdef _CMOC_VERSION_
#define ACTION_VERB ", press"
#define BOOT_KEY "ENTER"
#define PLATFORM "coco"
#define BOOT_WORD "play"
#undef BACK_KEY
#define BACK_KEY "BREAK"
#undef SCREEN_WIDTH
#define SCREEN_WIDTH 42
#define FRAMED 1
#undef BOTTOM_PANEL_ROWS
#define BOTTOM_PANEL_ROWS 2
#define CH_ESC 0x03
#undef COLD_BOOT
#define COLD_BOOT coldStart()

#endif

#ifdef __C64__
  #define PLATFORM "c64"
  #define BACKGROUND_COLOR 6
  #define FOREGROUND_COLOR 1
  #define ACTION_VERB ", press"
  #define BOOT_KEY "RETURN"
  #define BOOT_WORD "play"
#endif

#ifdef __ADAM__
  #define PLATFORM "adam"
  #define ACTION_VERB ", press"
  #define BOOT_KEY "RETURN"
  #define BOOT_WORD "play"
  #undef SCREEN_WIDTH
  #define SCREEN_WIDTH 32
  #define CH_ESC 0x1B
  #undef COLD_BOOT
  #define COLD_BOOT eos_init()
  // GRAPHICS II attribute bytes, (fg << 4) | bg
  #define HEADER_ATTR   0xFD   /* white on magenta */
  #define BACKDROP_ATTR 0x17   /* black on cyan, CONFIG's backdrop */
#endif

#ifdef __TRS80M2__
  #define PLATFORM "model2"
  #define ACTION_VERB ", press"
  #define BOOT_KEY "ENTER"
  #define BOOT_WORD "play"
  #undef SCREEN_WIDTH
  #define SCREEN_WIDTH 80
  #define FRAMED 1
  #define CH_ESC 0x1B
  // fuji_set_boot_mode(0) would mount the MS-DOS CONFIG: one RS-232 image
  #define NO_CONFIG_BOOT
  #undef COLD_BOOT
  #define COLD_BOOT quit()
#endif

#ifdef BUILD_MSDOS
  #define PLATFORM "msdos"
  #define ACTION_VERB ", press"
  #define BOOT_KEY "ENTER"
  #define BOOT_WORD "play"
  #define CH_ESC 0x1B
  #define LIST_X 1
#elif defined(FRAMED)
  #define LIST_X 1
#else
  #define LIST_X 0
#endif
#define LIST_W (SCREEN_WIDTH - 2 * LIST_X)

#ifdef FRAMED
  #define FOOTER_X LIST_X
  #define ELLIPSIS_WIDTH LIST_W
  #define CLEAR_BOTTOM_PANEL do { uint8_t _r; \
      for (_r = BOTTOM_PANEL_Y; _r < screen_height; _r++) cclearxy(LIST_X, _r, LIST_W); \
    } while (0)
#else
  #define FOOTER_X 0
  #define ELLIPSIS_WIDTH SCREEN_WIDTH
  #define CLEAR_BOTTOM_PANEL cclearxy(0, BOTTOM_PANEL_Y, BOTTOM_PANEL_LEN)
#endif

#ifdef __VIC20__
  #define PLATFORM "vic20"
  #undef SCREEN_WIDTH
  #define SCREEN_WIDTH 22
#endif


// gotoxy + c* saves a little space
#define cputsxy(x,y,s) do { gotoxy(x,y); cputs(s); } while (0)
#define cputcxy(x,y,c) do { gotoxy(x,y); cputc(c); } while (0)
#define cclearxy(x,y,c) do { gotoxy(x,y); cclear(c); } while (0)

// CONFIG draws the Adam's title row white on dark blue over black-on-white content
#ifdef __ADAM__
  #define TITLE_COLOR(on) adam_set_normal((on) ? WHITE : BLACK, (on) ? BLUE : WHITE)
#else
  #define TITLE_COLOR(on)
#endif

char username[66];

char buf[128];         // Temporary buffer use
uint8_t page_size;     // Active page size
uint8_t qa_mode=0;     // Toggles the lobby endpoint (Prod vs QA)
uint8_t page=0;        // Current page
bool more_pages;       // True if should check for more pages
int8_t selected = 0;   // Currently selected game or server

bool in_game;          // False when listing games, true when listing one game's servers
uint8_t game_type;     // Appkey of the game whose servers are listed
char game_name[17];    // Name of the game whose servers are listed
uint8_t game_page;     // Games list page and selection to return to
int8_t game_selected;

uint8_t screen_height;

#ifdef _CMOC_VERSION_
char loading_name[17];   // Set by mount(), consumed by reboot() in coco/platform.c
#endif

#ifdef __WATCOMC__
#pragma pack(push, 1)
#endif
typedef struct { // 19 bytes, /games
  uint8_t game_type;
  char game[17];
  uint8_t players;
} GameDetails;

typedef struct { // 36 bytes, /games/{game}
  uint8_t id;
  char server[33];
  uint8_t max_players;
  uint8_t players;
} ServerDetails;

typedef struct { // 130 bytes, /games/{game}/{server}
  char url[65];
  char client_url[65];
} InstanceDetails;
#ifdef __WATCOMC__
#pragma pack(pop)
#endif

typedef struct {
  uint8_t count;
  uint8_t reserved_for_future_1;
  uint8_t reserved_for_future_2;
  uint8_t data[23*sizeof(ServerDetails)];
} LobbyResponse;

LobbyResponse lobby;

// The records in lobby, depending on which list was read. The join urls have
// no header, so are read over the whole response.
#define GAMES    ((GameDetails*)lobby.data)
#define SERVERS  ((ServerDetails*)lobby.data)
#define INSTANCE (*(InstanceDetails*)&lobby)

void pause(void) {
  cputs("\r\nPress ");
  revers(1);
  cputs("RETURN");
  revers(0);
  cputs(" to continue.");
  cgetc();
}

#ifdef __ADAM__
// The Adam's bottom panel is the SmartKeys strip. smartkeys_status() redraws
// the whole status area from one string, so the text so far is kept here.
static char panel_buf[96];

static void panel_add(const char *msg)
{
  char *d = panel_buf + strlen(panel_buf);

  // smartkeys_puts() takes \n but would draw \r as a glyph
  while (*msg && (d - panel_buf) < (int)sizeof(panel_buf) - 1) {
    if (*msg != '\r')
      *d++ = *msg;
    msg++;
  }
  *d = 0;
  smartkeys_status(panel_buf);
}
#endif

void panel_clear(void)
{
#ifdef __ADAM__
  panel_buf[0] = 0;
  // smartkeys_status() never erases; redisplaying with no keycaps does
  smartkeys_display(NULL,NULL,NULL,NULL,NULL,NULL);
#else
  CLEAR_BOTTOM_PANEL;
#endif
}

/// @brief Clear the panel and write msg on its row (the Adam's status area has no rows)
void panel_status(uint8_t row, const char *msg)
{
  panel_clear();
#ifdef __ADAM__
  (void)row;
  panel_add("  ");
  panel_add(msg);
#else
  cputsxy(FOOTER_X,BOTTOM_PANEL_Y+row,msg);
#endif
}

/// @brief Write msg on another panel row without clearing
void panel_line(uint8_t row, const char *msg)
{
#ifdef __ADAM__
  (void)row;
  panel_add("\n  ");
  panel_add(msg);
#else
  cputsxy(FOOTER_X,BOTTOM_PANEL_Y+row,msg);
#endif
}

void panel_append(const char *msg)
{
#ifdef __ADAM__
  panel_add(msg);
#else
  cputs(msg);
#endif
}

/**
 * @brief The initial banner
 */
void banner(void) {
  uint8_t j;
  clrscr();

#if defined(BUILD_MSDOS)
  draw_box(0, 0, SCREEN_WIDTH, BOTTOM_PANEL_Y);
#elif defined(__ADAM__)
  TITLE_COLOR(1);
  cclearxy(0,0,SCREEN_WIDTH);
#elif defined(FRAMED)
  draw_frame(qa_mode ? "## QA MODE ##" : "#FUJINET GAME LOBBY", username, BOTTOM_PANEL_Y-1);
#endif
  
#ifndef FRAMED
  gotoxy(LIST_X, 0);
  if (qa_mode)
    cputs("## QA MODE ##");
  else
    cputs("#FUJINET GAME LOBBY");
  TITLE_COLOR(0);
#endif
}

/**
 * @brief Put the lobby url for the current level in buf: the games list, a game's
 * servers, or (with server set) the urls to join that server
 */
void lobby_url(bool server) {
  strcpy(buf, qa_mode ? LOBBY_QA_ENDPOINT : LOBBY_ENDPOINT);
  if (in_game) {
    strcat(buf, "/");
    itoa(game_type, buf+strlen(buf), 10);
  }
  if (server) {
    strcat(buf, "/");
    itoa(SERVERS[selected].id, buf+strlen(buf), 10);
  }
  strcat(buf, "?bin=1&platform=" PLATFORM);
}

void display_list(int8_t old_selected) {
  unsigned char j;
  char *name;

  if (old_selected<0)
  {
    name = in_game ? game_name : "GAME";
#ifdef FRAMED
    setBoldMode(TRUE);
#endif
    cputsxy(LIST_X,LIST_Y-2,name);
    if (in_game) {
      cclear(LIST_W-7-strlen(name));
      cputs("PLAYERS");
    } else {
      cclear(LIST_W-14-strlen(name));
      cputs("PLAYERS ONLINE");
    }
    gotoxy(LIST_X,LIST_Y-1);
    for(j=0;j<LIST_W;j++)
      cputc('-');
#ifdef FRAMED
    setBoldMode(FALSE);
#endif
#ifdef __ADAM__
    vdp_vfill(MODE2_ATTR + ((unsigned int)(LIST_Y-2) << 8), HEADER_ATTR, 256);
#endif
  }

  for (j=0;j<lobby.count;j++)
  {
    // If just moving the selection, only redraw the old and new item
    if (old_selected>=0 && j != old_selected && j != selected)
      continue;

    if (in_game) {
      name = SERVERS[j].server;
      itoa(SERVERS[j].players, buf, 10);
      strcat(buf, "/");
      itoa(SERVERS[j].max_players, buf+strlen(buf), 10);
    } else {
      name = GAMES[j].game;
      itoa(GAMES[j].players, buf, 10);
    }

    // Show the selected item in reverse
    // Printing full space to overwrite the existing item, a bit convoluted but
    // prevents flickering.
    revers(j == selected ? 1 : 0);
    gotoxy(LIST_X,LIST_Y+j);
    // Servers are indented under their game's heading
    if (in_game)
      cputc(' ');
    cputs(name);
    cclear(LIST_W-in_game-strlen(name)-strlen(buf));
    cputs((char *)buf);
  }

  // Reset cursor and reverse
  revers(0);

  if (old_selected>=0)
    return;

  // Clear rows left over from a longer list
  for (;j<page_size;j++)
    cclearxy(LIST_X,LIST_Y+j,LIST_W);

#ifdef __ADAM__
  // Rows under the list go back to the backdrop, so the white ends with the list
  vdp_vfill(MODE2_ATTR + ((unsigned int)(LIST_Y+lobby.count) << 8), BACKDROP_ATTR,
            (unsigned int)(BOTTOM_PANEL_Y-LIST_Y-lobby.count) << 8);

  // The status area beside keycaps IV-VI is about 128px of proportional font
  panel_clear();
  smartkeys_display(NULL,NULL,NULL," REFRESH"," QA MODE",
                    lobby.count>0 ? (in_game ? "  PLAY" : "  VIEW") : NULL);
  panel_add("  [R]efresh list  [Q]A\n");
  panel_add(in_game ? "  [ESC] back  [C]hange name" : "  [C]hange name");
#else
  CLEAR_BOTTOM_PANEL;
#if !defined(BUILD_MSDOS) && !defined(FRAMED)
  gotoxy(0,BOTTOM_PANEL_Y-1);
  for(j=0;j<SCREEN_WIDTH/8;j++)
    cputs(PANEL_SPACER);
#endif

  if (lobby.count>0)
  {
    gotoxy(FOOTER_X,BOTTOM_PANEL_Y);
    if (in_game) {
      cputs("Select table" ACTION_VERB " ");
      revers(1); cputs(BOOT_KEY); revers(0);
      cputs(" to " BOOT_WORD);
    } else {
      cputs("Select game" ACTION_VERB " ");
      revers(1); cputs(BOOT_KEY); revers(0);
      cputs(" to view");
    }
  }

#if BOTTOM_PANEL_ROWS > 2
  if (in_game) {
    gotoxy(FOOTER_X,BOTTOM_PANEL_Y+1);
    revers(1); cputs(BACK_KEY); revers(0);
    cputs(" back to games");
  }
#endif

  gotoxy(FOOTER_X,screen_height-1);
  revers(1); cputs("R"); revers(0);
  cputs("efresh list   ");

  gotoxy(SCREEN_WIDTH/2-1,screen_height-1);
  revers(1); cputs("Q"); revers(0);
  cputs("A");

  gotoxy(SCREEN_WIDTH-11-LIST_X-FOOTER_X,screen_height-1);
  revers(1); cputs("C"); revers(0);
  cputs("hange name");
#endif
}

void refresh_list(bool clearScreen) {
  int16_t api_read_result;
  uint8_t attempt;

  for(attempt=0;attempt<2;attempt++) {

    lobby.count=0;
    page_size = MAX_PAGE_SIZE - (qa_mode ? 3 : 0);

    panel_status(1,in_game ? "Retrieving Tables.." : "Retrieving Games..");

    lobby_url(false);
    strcat(buf, "&pagesize=");
    itoa(page_size, buf+strlen(buf), 10);
    strcat(buf, "&page=");
    itoa(page, buf+strlen(buf), 10);

    network_open(buf, OPEN_MODE_HTTP_GET_H, OPEN_TRANS_NONE);
    api_read_result = network_read(buf, (uint8_t*)&lobby, sizeof(lobby));
    network_close(buf);

    if (clearScreen && (attempt==1 || api_read_result>0)) {
      banner();
      if (qa_mode) {
        cputsxy(LIST_X,BOTTOM_PANEL_Y-3,"QA mode is for testing new games");
        cputsxy(LIST_X,BOTTOM_PANEL_Y-2,"Visit: LOBBY.FUJINET.ONLINE/DOCS");
      }
    }

#ifndef FRAMED
    TITLE_COLOR(1);
    cputsxy(SCREEN_WIDTH-LIST_X-strlen(username),0, username);
    TITLE_COLOR(0);
#endif

    if (api_read_result<0) {
      if (attempt) {
        panel_status(1,"Could not query Lobby! Error: ");
        itoa(api_read_result, buf, 10);
        panel_append(buf);
      } else {
         // Possibly reached end of list and didn't read due to 404, reset page to 0
          page=0;
      }
    } else if (api_read_result < 3 + (in_game ? sizeof(ServerDetails) : sizeof(GameDetails)) || lobby.count == 0 || lobby.count > page_size) {
      if (attempt) {
        panel_status(1,in_game ? "No tables available." : "No games are online.");
      }
      lobby.count = 0;
    } else {
      if (selected >= lobby.count) {
        selected = 0;
      }
      break;
    }

  }

  // Check for more pages if we received a full page size reponse
  more_pages = lobby.count == page_size;
  display_list(-1);
}



/**
 * @brief Get username and set key
 */
void get_username() {

  cputsxy(LIST_X,8,"Enter your username:");
  cputsxy(LIST_X,10,"[         ]");

  while (1) {
    inputField(LIST_X+1,10,8,username);
    if (strlen(username)>=2 && strlen(username)<=8)
      break;
  }

  write_appkey(CREATOR_ID,APP_ID,KEY_ID,strlen(username), username);  
}


/**
 * @brief Set user name, either from appkey or via input
 */
void register_user(void) {  
  read_appkey(CREATOR_ID,APP_ID,KEY_ID,username);
  
  if (strlen(username)==0)
    get_username();

  cputsxy(LIST_X,2,"Welcome, ");
  cputs(username);
  cputs(".");
}



/**
 * @brief Get the selected server's urls, then mount its client and reboot
*/
void mount() {
  static char *client_path, *host, *filename; 
  static uint8_t i, slot;
  static int16_t api_read_result;

  // Sanity check - a valid server index was selected, and the game type is > 0
  if (lobby.count==0 || selected >= lobby.count || game_type == 0) {
    return;
  } 

  panel_status(1,"Retrieving Server..");

  lobby_url(true);
  network_open(buf, OPEN_MODE_HTTP_GET_H, OPEN_TRANS_NONE);
  api_read_result = network_read(buf, (uint8_t*)&INSTANCE, sizeof(InstanceDetails));
  network_close(buf);

  if (api_read_result < 0 || api_read_result < sizeof(InstanceDetails)) {
    panel_status(0,"ERROR: Could not get server: ");
    itoa(api_read_result, buf, 10);
    panel_append(buf);
    pause();
    refresh_list(false);
    return;
  }
  
  // Remove the protocol for now, assume TNFS://
  if (client_path = strstr(INSTANCE.client_url, "://"))
    client_path+=3;
  else
    client_path = INSTANCE.client_url;

  strcpy(buf,client_path);
  if (strlen(client_path)>ELLIPSIS_WIDTH) {
    buf[ELLIPSIS_WIDTH/2-1]=buf[ELLIPSIS_WIDTH/2]='.';
    strcpy(buf+ELLIPSIS_WIDTH/2+1,client_path+strlen(client_path)-ELLIPSIS_WIDTH/2+1);
  }
#ifdef _CMOC_VERSION_
  // reboot() prints this once it has left graphics mode
  for (i = 0; i < sizeof(loading_name) - 1 && game_name[i]; i++) {
    char c = game_name[i];
    loading_name[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
  }
  loading_name[i] = 0;
#endif

  panel_status(0, MOUNTING);
  panel_line(1, buf);

  // Get the host and filename
  if (filename = strstr(client_path,"/")) {
    filename+=1;
  }

  // Get the host
  host = strtok(client_path,"/");

  if (filename == NULL || host == NULL) {
    panel_status(0,"ERROR: Invalid client file");
    pause();
    refresh_list(false);
    return;
  }

  // Read current list of hosts from FujiNet
  fuji_get_host_slots((unsigned char*) host_slots, FUJI_HOST_SLOT_COUNT);
  
  // Pick the host slot to use. Default to the last, but choose an existing slot
  // if it already has the same host
  slot = FUJI_HOST_SLOT_COUNT;
  for(i=0;i<FUJI_HOST_SLOT_COUNT;i++) {
    if (stricmp(host, (char*)host_slots[i]) == 0) {
      slot = i;
      break;
    }
  }

  // Update the bottom host slot if needed
  if (slot == FUJI_HOST_SLOT_COUNT) {
    slot=FUJI_HOST_SLOT_COUNT-1;
    strcpy((char*)host_slots[slot], host);
    fuji_put_host_slots((unsigned char*) host_slots, FUJI_HOST_SLOT_COUNT);
  }
  

  // Mount the file to the device/host slot
  fuji_mount_host_slot(slot);

  // Mount the file to device slot 0
  device_slots[0].hostSlot = slot;
  device_slots[0].mode = 0;
  strcpy((char*)device_slots[0].file, filename);
  fuji_put_device_slots(device_slots, FUJI_DEVICE_SLOT_COUNT);
  fuji_set_device_filename(0, slot, 0, filename);
  fuji_mount_disk_image(0, 1);

  // Set the server url in this game type's app key:
  write_appkey(CREATOR_ID,APP_ID,game_type, strlen(INSTANCE.url), INSTANCE.url);  
  
  // Reboot / run the game
  reboot();

}

/**
 * @brief Open the selected game's list of servers
 */
void open_game() {
  if (lobby.count==0 || selected >= lobby.count || GAMES[selected].game_type == 0)
    return;

  game_type = GAMES[selected].game_type;
  strcpy(game_name, GAMES[selected].game);
  game_page = page;
  game_selected = selected;

  in_game = true;
  page = 0;
  selected = 0;
  refresh_list(true);
}

/**
 * @brief Return from a game's servers to the games list
 */
void close_game() {
  in_game = false;
  page = game_page;
  selected = game_selected;
  refresh_list(true);
}

void change_selection(int8_t delta) {
    int8_t old_selected = selected;

    selected += delta;

    if (selected <0) {
      selected=0;

      if (page>0) {
        // Go back a page
        page--;
        refresh_list(true);
        return;
      }  else if (!more_pages) {
        selected=lobby.count-1;
      }
    }
    else if (selected >= lobby.count) {
      selected=0;
      
      if (more_pages) {
        // Move to next page
        page++;
        refresh_list(true);
        return;
      } else if (page>0) {
        // Last page, go back to page 0
        page=0;
        refresh_list(true);
        return;
      } 
    }

   
  // Not switching pages, just update the selection
  display_list(old_selected);
        
    
}

/**
 * @brief Open the selected game, or join the selected server
 */
void select_item() {
  if (in_game)
    mount();
  else
    open_game();
}
 
void event_loop() {
  while (true) { 
    readCommonInput();

#ifdef __ADAM__
    // SmartKeys IV-VI do what display_list() labels them
    if (input.key == KEY_SMART_IV)
      input.key = 'r';
    else if (input.key == KEY_SMART_V)
      input.key = 'q';
    else if (input.key == KEY_SMART_VI)
      input.trigger = true;
#endif

    switch (input.key) {
#ifdef _CMOC_VERSION_
      case 's':
      case 'S':
        cycle_colorset();
        banner();
        display_list(-1);
        break;
#endif
      case 'c':
      case 'C':
        banner();  
        get_username();
        refresh_list(true);
        break;
      case 'q':
      case 'Q':
        qa_mode = !qa_mode;
        in_game = false;
        page = 0;
        selected = 0;
        refresh_list(true);
        break;
      case 'r':
      case 'R':
        refresh_list(false);
        break;
    case CH_ESC: // Escape / Break - back to games, or quit to config
        if (in_game) {
          close_game();
          break;
        }
#ifndef NO_CONFIG_BOOT
        fuji_set_boot_mode(0);
#endif
        COLD_BOOT;
    }
    
    waitvsync();
    // Up/down arrows select the game or server, left goes back to games
    if (input.dirY) {
      change_selection(input.dirY);
    } else if (input.dirX<0 && in_game) {
      close_game();
    }
    
    
    // ifdef hack for Atari, for now
    #ifdef __ATARI__
    // Pressing Option selects the game or server
    if (CONSOL_OPTION(GTIA_READ.consol))
    {
      select_item();
        // Wait until OPTION is released
      while (CONSOL_OPTION(GTIA_READ.consol));
    }
    #endif  

    // Select if return/enter is pressed
    if (input.trigger) {
      select_item();
    }        
  }
}

void main(void)
{
  uint8_t i;

  initialize();
  
  screensize(&i, &screen_height);

  #ifdef BACKGROUND_COLOR
  bordercolor(BACKGROUND_COLOR);
  bgcolor(BACKGROUND_COLOR);
  #endif

  #ifdef FOREGROUND_COLOR
  textcolor(FOREGROUND_COLOR);
  #endif

  banner();
  
  register_user();
#ifdef FRAMED
  /* redraw the border in place so the username appears without clearing */
  draw_frame(qa_mode ? "## QA MODE ##" : "#FUJINET GAME LOBBY", username, BOTTOM_PANEL_Y-1);
#endif
  for(i=0;i<45;i++) waitvsync();

  refresh_list(true);
  event_loop();
}
