#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xA7, 0xE2, 0x79, 0x8A, 0x1D, 0x2A, 0x4C, 0xD6, 0xAE, 0x30, 0x03, 0xCB, 0x1A, 0xA3, 0x46, 0x1A }
#define ABOUT_CONTENT "By: Niels Vleeming\
\nLevels: ..\
\nFor info and source code, visit:\
\npebloban.vlemert.com"

PBL_APP_INFO(MY_UUID,
             "Pebloban", "Vlemert",
             0, 1, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);


/* Helper code */
/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
*/
char* itoa(int value, char* result, int base) {
  // check that the base if valid
  if (base < 2 || base > 36) { *result = '\0'; return result; }
	
  char* ptr = result, *ptr1 = result, tmp_char;
  int tmp_value;
	
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
  } while ( value );
	
  // Apply negative sign
  if (tmp_value < 0) *ptr++ = '-';
  *ptr-- = '\0';
  while(ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr--= *ptr1;
    *ptr1++ = tmp_char;
  }
  return result;
}

// Normal square size
const char Square_Width = 0x0F; // 15
const char Square_Height = 0x0F;

// 'minimap' square size
const char Square_Small_Width = 0x05;
const char Square_Small_Height = 0x05;

// Images
BmpContainer IMG_BOX;
BmpContainer IMG_WALL;
BmpContainer IMG_TARGET;
BmpContainer IMG_PLAYER_UP;
BmpContainer IMG_PLAYER_RIGHT;
BmpContainer IMG_PLAYER_DOWN;
BmpContainer IMG_PLAYER_LEFT;
BmpContainer IMG_ARROW_UP;
BmpContainer IMG_ARROW_RIGHT;
BmpContainer IMG_ARROW_DOWN;
BmpContainer IMG_ARROW_LEFT;
			 
			 
/* Levels
    Note: the definition is levels[levelNr][x][y]. To visualize the array to a real map, flip the matrix, so that the top right corner is bottom left and vice versa.
    Note2: bits 1,2,4,8 are the bottom bits, 16,32,64,128 are the top bits!
    We can store all info for one position in 4 bits. Thus 2 blocks per byte. This means we can store two vertically adjacent blocks in one byte. This means we need half as much 'y' positions

(byte & 1): bottom position contains target
(byte & 2): bottom position contains wall
(byte & 4): bottom position contains box
(byte & 8): bottom position contains player_start
(byte & 16,32,64,128): same for top position

This is a test level, with walls all around
Test size is 6x6
*/
const unsigned char levels[1][6][3] = {
 {
  { 0x22, 0x22, 0x22 },
  { 0x20, 0x00, 0x02 },
  { 0x20, 0x00, 0x02 },
  { 0x20, 0x00, 0x02 },
  { 0x20, 0x00, 0x02 },
  { 0x22, 0x22, 0x22 }
 }
};


/* Menu structs */
typedef struct MenuItem {
  Layer mainLayer;
  TextLayer pointerLayer;
  TextLayer pointerRightLayer;
  TextLayer textLayer;
  char *text;
  bool selected;
  void (*menuAction)(void);
} MenuItem;

void menuitem_pointer_update_proc(MenuItem *menuItem, GContext* ctx) {
}

void menuitem_init_text_action(MenuItem *menuItem, char *text, void (*menuAction)(void)) {
  menuItem->text = text;
  menuItem->menuAction = menuAction;

  //menuItem->pointerLayer.layer.update_proc = (LayerUpdateProc)menuitem_pointer_update_proc;
}

void menuitem_set_selected(MenuItem *menuItem, bool selected) {
  menuItem->selected = selected;

  if (menuItem->selected == true) {
      text_layer_set_text(&menuItem->pointerLayer, ">");
	  text_layer_set_text(&menuItem->pointerRightLayer, "<");
  } else {
      text_layer_set_text(&menuItem->pointerLayer, "");
	  text_layer_set_text(&menuItem->pointerRightLayer, "");
  }
}

void menuitem_init(MenuItem *menuItem, GRect frame) {
	layer_init(&menuItem->mainLayer, frame);

	text_layer_init(&menuItem->textLayer, GRect(15, 0, 144, 20));
	text_layer_set_text(&menuItem->textLayer, menuItem->text);
	text_layer_set_text_color(&menuItem->textLayer, GColorWhite);
	text_layer_set_background_color(&menuItem->textLayer, GColorClear);
	text_layer_set_text_alignment(&menuItem->textLayer, GTextAlignmentLeft);
	layer_add_child(&menuItem->mainLayer, &menuItem->textLayer.layer);

	text_layer_init(&menuItem->pointerLayer, GRect(5, 0, 10, 20));
	text_layer_set_text_color(&menuItem->pointerLayer, GColorWhite);
	text_layer_set_background_color(&menuItem->pointerLayer, GColorClear);
	layer_add_child(&menuItem->mainLayer, &menuItem->pointerLayer.layer);

	text_layer_init(&menuItem->pointerRightLayer, GRect(128, 0, 20, 20));
	//layer_add_child(&menuItem->mainLayer, &menuItem->pointerRightLayer.layer);

	//  text_layer_init(&menuItem->pointerLayer, windowBoot_Menu.frame);
	//text_layer_set_text(&windowBoot_Menu_Start, "> Start game");
	//text_layer_set_text_alignment(&windowBoot_Menu_Start, GTextAlignmentLeft);
	//layer_add_child(&windowBoot_Menu, &windowBoot_Menu_Start.layer);
}



/* Enums */
typedef enum {START, INGAME} MenuType;

/* Assisting structs */

/* Window structs */
typedef struct MenuWindow {
	Window window;
	TextLayer debugLayer;
	int shownMenu[10];
	MenuItem allMenuItems[10];
	int selectedItem;
} MenuWindow;

typedef struct GameWindow {
	Window window;
	Layer backgroundLayer;
	Layer buttonActionBar;
	TextLayer lblLevel;
	TextLayer lblLevelNr;
	Layer gameScreen;
	Layer tileContainer;
	Layer staticTiles;
	Layer dynamicTiles;
	
	unsigned char level[14][8];
	signed char levelNr;
	char levelWidth;
	char levelHeight;
	
	signed char levelX;
	signed char levelY;
	
	signed char playerX;
	signed char playerY;
	
	signed char vX;
	signed char vY;
	
	char targetsLeft;
	
	bool invertButtons;
} GameWindow;

typedef struct PauseWindow {
	Window window;
} PauseWindow;

typedef struct MinimapWindow {
	Window window;
} MinimapWindow;

typedef struct AboutWindow {
	Window window;
	TextLayer textLayer;
} AboutWindow;

// Declaration of those window structs
MenuWindow menuWindow;
GameWindow gameWindow;
PauseWindow pauseWindow;
MinimapWindow minimapWindow;
AboutWindow aboutWindow;


// Draws the game logo on the top 1/3 of the screen
void windowlayer_logo_draw(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);

    // Black background
    graphics_fill_rect(ctx, GRect(0, 0, layer->bounds.size.w, 51), 0, GCornerNone);
	
	graphics_fill_rect(ctx, GRect(0, 52, layer->bounds.size.w, layer->bounds.size.h-52), 0, GCornerNone);
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	
	graphics_fill_rect(ctx, GRect(0, 51, 144, 1), 0, GCornerNone);
	
	
	GRect destination = layer_get_frame(&IMG_BOX.layer.layer);
	
	destination.origin.x = 35;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	destination.origin.x = 65;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	destination.origin.x = 65;
	destination.origin.y = 18;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	destination.origin.x = 50;
	destination.origin.y = 33;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	destination.origin.x = 95;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	destination.origin.x = 110;
	destination.origin.y = 18;
	graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, destination);
	
	destination.origin.x = 5;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	destination.origin.x = 5;
	destination.origin.y = 18;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	destination.origin.x = 5;
	destination.origin.y = 33;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	destination.origin.x = 125;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	destination.origin.x = 125;
	destination.origin.y = 18;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	destination.origin.x = 125;
	destination.origin.y = 33;
	graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, destination);
	
	destination.origin.x = 50;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_TARGET.bmp, destination);

	destination.origin.x = 20;
	destination.origin.y = 3;
	graphics_draw_bitmap_in_rect(ctx, &IMG_PLAYER_RIGHT.bmp, destination);
}


void aboutwindow_init(AboutWindow *aboutWindow) {
	window_init(&aboutWindow->window, "About window");
	
	GRect textPosition = layer_get_frame(&aboutWindow->window.layer);
	textPosition.origin.x = 3;
	textPosition.origin.y = 55;
	textPosition.size.w = textPosition.size.w - 6; // make everyting symmetrical
	
	text_layer_init(&aboutWindow->textLayer, textPosition);
	text_layer_set_text(&aboutWindow->textLayer, ABOUT_CONTENT);
	text_layer_set_text_color(&aboutWindow->textLayer, GColorWhite);
	text_layer_set_background_color(&aboutWindow->textLayer, GColorClear);
	layer_add_child(&aboutWindow->window.layer, &aboutWindow->textLayer.layer);
	
	aboutWindow->window.layer.update_proc = windowlayer_logo_draw;
}

void minimapwindow_init(MinimapWindow *minimapWindow) {
	window_init(&minimapWindow->window, "Minimap window");
}




void pausewindow_init(PauseWindow *pauseWindow) {
	window_init(&pauseWindow->window, "Pause window");
	
	pauseWindow->window.layer.update_proc = windowlayer_logo_draw;
}



// Draws the top, right and bottom line in the game window
// There seems to be a bug, the outer bounds of the layer fall of the bottom of the screen (16px, the size of the top bar)
void windowGame_Bars_draw(Layer *layer, GContext *ctx) {
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(0, 0, layer->bounds.size.w, layer->bounds.size.h), 0, GCornerNone);

    graphics_context_set_fill_color(ctx, GColorWhite);
    const int16_t offset_right = layer->bounds.size.w - 17;
	
	// top line
	graphics_fill_rect(ctx, GRect(0, 0, offset_right, 1), 0, GCornerNone);
    // Right bar
    graphics_fill_rect(ctx, GRect(offset_right, 0, 1, layer->bounds.size.h - 33), 0, GCornerNone);
    const int16_t offset_bottom = layer->bounds.size.h - 33; // Top bar offsets the outer bounds by 16px, so we need to go up 33
    // Bottom bar
    graphics_fill_rect(ctx, GRect(0, offset_bottom, layer->bounds.size.w - 17, 1), 0, GCornerNone); // Width - 17, so the corner doesn't get drawn twice
}

void windowgame_buttonactionbar_draw(Layer *layer, GContext *ctx) {
	if (!gameWindow.invertButtons) {
		graphics_draw_bitmap_in_rect(ctx, &IMG_ARROW_UP.bmp, GRect(0, 0,9,9));
		graphics_draw_bitmap_in_rect(ctx, &IMG_ARROW_DOWN.bmp, GRect(0, layer->bounds.size.h-9,9,9));
	} else {
		graphics_draw_bitmap_in_rect(ctx, &IMG_ARROW_LEFT.bmp, GRect(0, 0,9,9));
		graphics_draw_bitmap_in_rect(ctx, &IMG_ARROW_RIGHT.bmp, GRect(0, layer->bounds.size.h-9,9,9));
	}
}

void windowGame_container_draw(Layer *layer, GContext *ctx) {
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, layer->bounds, 0, GCornerNone);
	
	GRect destination = layer->bounds;
	
	for (int x = 0; x < destination.size.w; x+=15) {
		for (int y = 0; y < destination.size.h; y+=15) {
			graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, GRect(x, y,15,15));
		}
	}
	//graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, GRect(15, 15,destination.size.w-15,destination.size.h-15));
	//graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, GRect(0, 0,15,15));
	//graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, GRect(15, 0,destination.size.w-15,15));
	//graphics_draw_bitmap_in_rect(ctx, &IMG_WALL.bmp, GRect(0, 15,15,destination.size.h-15));
	
	//graphics_draw_bitmap_in_rect(ctx, &IMG_WALL_INV.bmp, GRect(-15,-15,15,30));
}

//(byte & 1): bottom position contains target
//(byte & 2): bottom position contains wall
//(byte & 4): bottom position contains box
//(byte & 8): bottom position contains player_start
//(byte & 16,32,64,128): same for top position

void windowgame_statictiles_draw(Layer *layer, GContext *ctx) {
	if (gameWindow.levelNr > -1) {
		graphics_context_set_fill_color(ctx, GColorBlack);
		//graphics_fill_rect(ctx, GRect(0,0,20,20), 0, GCornerNone);
		for (unsigned char i = 0; i < gameWindow.levelWidth; i++) {
			for (unsigned char j = 0; j < gameWindow.levelHeight/2; j++) {
				unsigned char block = gameWindow.level[i][j];
				//char topblock = block >> 4;
				
				// no wall on top block
				if (!(block & 32)) {
					graphics_fill_rect(ctx, GRect((i*15), ((j*2)*15), 15, 15), 0, GCornerNone);
				}
				
				// target on top block
				if (block & 16) {
					graphics_draw_bitmap_in_rect(ctx, &IMG_TARGET.bmp, GRect((i*15), ((j*2)*15), 15, 15));
				}
				
				// no wall on bottom block
				if (!(block & 2)) {
					graphics_fill_rect(ctx, GRect((i*15), ((j*2)*15)+15, 15, 15), 0, GCornerNone);
				}
				
				// target on bottom block
				if (block & 1) {
					graphics_draw_bitmap_in_rect(ctx, &IMG_TARGET.bmp, GRect((i*15), ((j*2)*15)+15, 15, 15));
				}
			}
		}
	}
}

void windowgame_dynamictiles_draw(Layer *layer, GContext *ctx) {
	if (gameWindow.levelNr > -1 && gameWindow.playerX > -1 && gameWindow.playerY > -1) {
		if (gameWindow.vX == -1) {
			graphics_draw_bitmap_in_rect(ctx, &IMG_PLAYER_LEFT.bmp, GRect((gameWindow.playerX*15), (gameWindow.playerY*15), 15, 15));
		} else if (gameWindow.vX == 1) {
			graphics_draw_bitmap_in_rect(ctx, &IMG_PLAYER_RIGHT.bmp, GRect((gameWindow.playerX*15), (gameWindow.playerY*15), 15, 15));
		} else if (gameWindow.vY == -1) {
			graphics_draw_bitmap_in_rect(ctx, &IMG_PLAYER_UP.bmp, GRect((gameWindow.playerX*15), (gameWindow.playerY*15), 15, 15));
		} else if (gameWindow.vY == 1) {
			graphics_draw_bitmap_in_rect(ctx, &IMG_PLAYER_DOWN.bmp, GRect((gameWindow.playerX*15), (gameWindow.playerY*15), 15, 15));
		}
		
		for (unsigned char i = 0; i < gameWindow.levelWidth; i++) {
			for (unsigned char j = 0; j < gameWindow.levelHeight/2; j++) {
				unsigned char block = gameWindow.level[i][j];

				// box on top block
				if (block & 64) {
					graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, GRect((i*15), ((j*2)*15), 15, 15));
				}

				// box on bottom block
				if (block & 4) {
					graphics_draw_bitmap_in_rect(ctx, &IMG_BOX.bmp, GRect((i*15), ((j*2)*15)+15, 15, 15));
				}
			}
		}
	}
}

bool block_is_wall(unsigned char x, unsigned char y) {
	unsigned char block = gameWindow.level[x][y/2];
	
	if (y % 2 > 0) { // bottom block
		return block & 2;
	} else { // top block
		return block & 32;
	}
}

bool move_block_if_needed(unsigned char x, unsigned char y, signed char vX, signed char vY) {
	x = x + vX;
	y = y + vY;
	
	unsigned char nextX = x + vX;
	unsigned char nextY = y + vY;
	
	unsigned char block = gameWindow.level[x][y/2];
	
	bool blockIsBox = false;
	bool blockIsTarget = false;
	
	// check next block
	if (y % 2 > 0) { // bottom block
		blockIsBox = block & 4;
		blockIsTarget = block & 1;
	} else { // top block
		blockIsBox = block & 64;
		blockIsTarget = block & 16;
	}
	
	// block is not a box, return true
	if (!blockIsBox) {
		return true;
	}
	
	// check if we hit the outer wall
	if (((x == 13 || x == 0) && vX != 0) || ((y == 15 || y == 0) && vY != 0)) {
		return false;
	}
	
	// this is the next block
	unsigned char nextBlock = gameWindow.level[nextX][nextY/2];
	
	bool blockIsClear = false;
	bool nextBlockIsTarget = false;
	
	// block is a box, check if the next one is clear (neither box, nor block)
	if (nextY % 2 > 0) {
		blockIsClear = !(nextBlock & 2 || nextBlock & 4);
		nextBlockIsTarget = nextBlock & 1;
	} else {
		blockIsClear = !(nextBlock & 32 || nextBlock & 64);
		nextBlockIsTarget = nextBlock & 16;
	}
	
	// next one is not clear, return
	if (!blockIsClear) {
		return false;
	}
	
	// next block is clear, move the box
	if (y % 2 > 0) {
		gameWindow.level[x][y/2] &= ~4;
	} else {
		gameWindow.level[x][y/2] &= ~64;
	}
	if (nextY % 2 > 0) {
		gameWindow.level[nextX][nextY/2] |= 4;
	} else {
		gameWindow.level[nextX][nextY/2] |= 64;
	}
	
	// if the block moved from a target, one target more left
	if (blockIsTarget) {
		gameWindow.targetsLeft++;
	}
	
	// if the block moved ontop of a target, one target less left
	if (nextBlockIsTarget) {
		gameWindow.targetsLeft--;
	}
	
	return true;
}

void gamewindow_start_game(GameWindow *gameWindow, char levelNr) {
	gameWindow->levelNr = levelNr;
	
	char buffer[1024];
    itoa(levelNr+1, buffer, 10);
	text_layer_set_text(&gameWindow->lblLevelNr, buffer);
	
	gameWindow->levelWidth = 14;
	gameWindow->levelHeight = 16;
	
	unsigned char tempGame[14][8] = {
		{ 0x00, 0x22, 0x01, 0x20, 0x00, 0x00, 0x00, 0x00 },
		{ 0x04, 0x80, 0x40, 0x20, 0x00, 0x00, 0x00, 0x00 },
		{ 0x10, 0x22, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00 },
		{ 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	};
	
	gameWindow->targetsLeft = 0;
	for (unsigned char i = 0; i < gameWindow->levelWidth; i++) {
		for (unsigned char j = 0; j < gameWindow->levelHeight/2; j++) {
			gameWindow->level[i][j] = tempGame[i][j];
			
			// Count the nr of open targets
			if (tempGame[i][j] & 16 && !(tempGame[i][j] & 64)) {
				gameWindow->targetsLeft++;
			}
			if (tempGame[i][j] & 1 && !(tempGame[i][j] & 4)) {
				gameWindow->targetsLeft++;
			}
		}
	}
	
	// Must be set based on current level later on..
	gameWindow->levelX = 0;
	gameWindow->levelY = 0;
	gameWindow->playerX = 1;
	gameWindow->playerY = 2;
	gameWindow->vX = 0;
	gameWindow->vY = -1;
	
	layer_mark_dirty(&(gameWindow->staticTiles));
	layer_mark_dirty(&(gameWindow->dynamicTiles));
}

void player_move(signed char vX, signed char vY) {
	signed char vXold = gameWindow.vX;
	signed char vYold = gameWindow.vY;
	gameWindow.vX = vX;
	gameWindow.vY = vY;
	
	// Player rotation is cause for a redraw, even without moving
	if (vX != vXold || vY != vYold) {
		layer_mark_dirty(&(gameWindow.dynamicTiles));
	}
	
	// Check if we are within the maximum game bounds
	if (gameWindow.playerX + vX >=	0 && gameWindow.playerX + vX <= 14 && gameWindow.playerY + vY >= 0 && gameWindow.playerY + vY <= 15)
	{
		// Walking into a wall?
		if (!block_is_wall(gameWindow.playerX + vX, gameWindow.playerY + vY)) {
			// Pushing a box maybe?
			if (move_block_if_needed(gameWindow.playerX, gameWindow.playerY, vX, vY)) {
				gameWindow.playerX += vX;
				gameWindow.playerY += vY;
				// This is cause for a redraw:
				layer_mark_dirty(&(gameWindow.dynamicTiles));
			}
		}
	}
	
	//gameWindow.tileContainer.bounds = GRect(-(105 * ((gameWindow.playerX+gameWindow.vX)/7)), -(120 * ((gameWindow.playerY+gameWindow.vY)/8)), 210+30, 240+30);
	gameWindow.tileContainer.bounds = GRect(-(105 * ((gameWindow.playerX)/7)), -(120 * ((gameWindow.playerY)/8)), 210+30, 240+30);
	
	if (gameWindow.targetsLeft == 0) {
		gamewindow_start_game(&gameWindow, 0);
	}
}

void game_window_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
	
	if (!gameWindow.invertButtons) {
		player_move(0, -1);
	} else {
		player_move(-1, 0);
	}
}

void game_window_up_multi_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
}

void game_window_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
	(void)recognizer;
	(void)window;
  
	if (!gameWindow.invertButtons) {
		player_move(0, 1);
	} else {
		player_move(1, 0);
	}
}

void game_window_down_multi_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
}

void game_window_select_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

  // 'Pause' game. Show a nice clock and '<press back to continue>' text
  window_stack_push(&pauseWindow.window, true);
}

void game_window_select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

  // Switch button modes (single/[multi/long] click)
  gameWindow.invertButtons = !gameWindow.invertButtons;
  layer_mark_dirty(&(gameWindow.buttonActionBar));
}

void game_window_select_multi_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

	// Show map overview
	window_stack_push(&minimapWindow.window, true);
}

void game_window_click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  //config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) game_window_select_long_click_handler;
  
  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) game_window_select_single_click_handler;
  
  //config[BUTTON_ID_SELECT]->multi_click.handler = (ClickHandler) game_window_select_multi_click_handler;
  //config[BUTTON_ID_SELECT]->multi_click.min = 2;
  //config[BUTTON_ID_SELECT]->multi_click.max = 2;
  //config[BUTTON_ID_SELECT]->multi_click.last_click_only = true;
  //config[BUTTON_ID_SELECT]->multi_click.timeout = 0x64;

  //uint8_t min;
    //uint8_t max;
    //bool last_click_only;
    //ClickHandler handler;
    //uint16_t timeout;
  
  //config[BUTTON_ID_UP]->click.handler = (ClickHandler) game_window_up_single_click_handler;
  //config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
  
  config[BUTTON_ID_UP]->click.handler = (ClickHandler) game_window_up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 200;
  //config[BUTTON_ID_UP]->multi_click.handler = (ClickHandler) game_window_up_multi_click_handler;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) game_window_down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 200;
  
  //config[BUTTON_ID_DOWN]->multi_click.handler = (ClickHandler) game_window_down_multi_click_handler;
}

void gamewindow_init(GameWindow *gameWindow) {
	window_init(&gameWindow->window, "Game window");
	
	gameWindow->levelNr = -1;
	
	gameWindow->levelX = -1;
	gameWindow->levelY = -1;
	
	gameWindow->playerX = -1;
	gameWindow->playerY = -1;
	
	layer_init(&gameWindow->backgroundLayer, gameWindow->window.layer.frame);
	layer_add_child(&gameWindow->window.layer, &gameWindow->backgroundLayer);
	gameWindow->backgroundLayer.update_proc = windowGame_Bars_draw;
	
	const int16_t offset_right = gameWindow->window.layer.bounds.size.w - 17;
	const int16_t offset_bottom = gameWindow->window.layer.bounds.size.h - 34;
	
	layer_init(&gameWindow->buttonActionBar, GRect(offset_right+3, 0, 15, offset_bottom));
	layer_add_child(&gameWindow->window.layer, &gameWindow->buttonActionBar);
	gameWindow->buttonActionBar.update_proc = windowgame_buttonactionbar_draw;
	
	text_layer_init(&gameWindow->lblLevel, GRect(3, offset_bottom + 2, 30, 20));
	text_layer_set_text_color(&gameWindow->lblLevel, GColorWhite);
	text_layer_set_background_color(&gameWindow->lblLevel, GColorClear);
	text_layer_set_text(&gameWindow->lblLevel, "LVL:");
	layer_add_child(&gameWindow->window.layer, &gameWindow->lblLevel.layer);
	
	text_layer_init(&gameWindow->lblLevelNr, GRect(30, offset_bottom + 2, 30, 20));
	text_layer_set_text_color(&gameWindow->lblLevelNr, GColorWhite);
	text_layer_set_background_color(&gameWindow->lblLevelNr, GColorClear);
	text_layer_set_text(&gameWindow->lblLevelNr, "0");
	layer_add_child(&gameWindow->window.layer, &gameWindow->lblLevelNr.layer);
	
	layer_init(&gameWindow->gameScreen, GRect(0,1,offset_right,offset_bottom));
	layer_set_clips(&gameWindow->gameScreen, true);
	layer_add_child(&gameWindow->window.layer, &gameWindow->gameScreen);
	
	// Init of tileContainer layer must be done based on the starting position of the player later on
	layer_init(&gameWindow->tileContainer, GRect(11-15, 7-15, 210+30, 240+30));
	layer_add_child(&gameWindow->gameScreen, &gameWindow->tileContainer);
	gameWindow->tileContainer.update_proc = windowGame_container_draw;
	
	
	GRect destination = gameWindow->tileContainer.bounds;
	destination.origin.x = destination.origin.x + 15;
	destination.origin.y = destination.origin.y + 15;
	destination.size.w = destination.size.w - 30;
	destination.size.h = destination.size.h - 30;
	
	layer_init(&gameWindow->staticTiles, destination);
	layer_add_child(&gameWindow->tileContainer, &gameWindow->staticTiles);
	gameWindow->staticTiles.update_proc = windowgame_statictiles_draw;
	
	layer_init(&gameWindow->dynamicTiles, destination);
	layer_add_child(&gameWindow->tileContainer, &gameWindow->dynamicTiles);
	gameWindow->dynamicTiles.update_proc = windowgame_dynamictiles_draw;
	
	// click handlers
	window_set_click_config_provider(&gameWindow->window, (ClickConfigProvider) game_window_click_config_provider);
}



void menuwindow_set_shownmenu(MenuWindow *menuWindow, int shownMenu[]) {
  for (unsigned int i = 0; i < 10; i++) {
    menuWindow->shownMenu[i] = shownMenu[i];
  }
}

void menuwindow_set_menutype(MenuWindow *menuWindow, MenuType menuType) {
  for (int i = 0; i < 10; i++) {
    int currentItem = menuWindow->shownMenu[i];
    if (currentItem >= 0 && currentItem < 10) { 
      layer_remove_from_parent(&menuWindow->allMenuItems[currentItem].mainLayer);
    }
  }
  
  if (menuType == START) {
    int shownMenu[10] = { 2, 3, 4, 5, -1, -1, -1, -1, -1, -1};
    menuwindow_set_shownmenu(menuWindow, shownMenu);
  } else if (menuType == INGAME) {
    int shownMenu[10] = { 0, 1, 3, 4, 5, -1, -1, -1, -1, -1};
    menuwindow_set_shownmenu(menuWindow, shownMenu);
  }
  
  for (int i = 0; i < 10; i++) {
    int currentItem = menuWindow->shownMenu[i];
    if (currentItem >= 0 && currentItem < 10) {
      int offsetTop = 55 + (17 * i);
      //char buffer[1024];
      //itoa(currentItem, buffer, 10);
      //text_layer_set_text(&menuWindow->debugLayer, buffer);
      
      menuitem_init(&menuWindow->allMenuItems[currentItem], GRect(0, offsetTop, 144, 20));
      layer_add_child(&menuWindow->window.layer, &menuWindow->allMenuItems[currentItem].mainLayer);
      menuitem_set_selected(&menuWindow->allMenuItems[currentItem], (menuWindow->selectedItem == i));
    }
  }
}

void menuwindow_click_up(MenuWindow *menuWindow) {
  int newItem = menuWindow->selectedItem - 1;
  
  if (newItem < 0) {
    newItem = 9;
  }

  while (menuWindow->shownMenu[newItem] == -1) {
    newItem--;

    if (newItem < 0) {
      newItem = 9;
    }
  }

  // we found our new index, set the old one deselected
  menuitem_set_selected(&menuWindow->allMenuItems[menuWindow->shownMenu[menuWindow->selectedItem]], false);

  // and new one selected
  menuitem_set_selected(&menuWindow->allMenuItems[menuWindow->shownMenu[newItem]], true);

  menuWindow->selectedItem = newItem;
}

void menuwindow_click_down(MenuWindow *menuWindow) {
  int newItem = menuWindow->selectedItem + 1;
  
  if (newItem >= 10) {
    newItem = 0;
  }

  // This has the potential to become an eternal loop if there are no valid menuitems...
  while (menuWindow->shownMenu[newItem] == -1) {
    newItem++;

    if (newItem >= 10) {
      newItem = 0;
    }
  }

  // we found our new index, set the old one deselected
  menuitem_set_selected(&menuWindow->allMenuItems[menuWindow->shownMenu[menuWindow->selectedItem]], false);

  // and new one selected
  menuitem_set_selected(&menuWindow->allMenuItems[menuWindow->shownMenu[newItem]], true);

  menuWindow->selectedItem = newItem;
}

void boot_menu_up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

  menuwindow_click_up(&menuWindow);
}

void boot_menu_down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
  menuwindow_click_down(&menuWindow);
}

void boot_menu_select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

  (&menuWindow)->allMenuItems[(&menuWindow)->shownMenu[(&menuWindow)->selectedItem]].menuAction();
}

void boot_menu_click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) boot_menu_select_single_click_handler;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) boot_menu_up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) boot_menu_down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

/* Menu actions */
void menuaction_continue_game() {
	// Switch to game screen
	window_stack_push(&gameWindow.window, true);
}
void menuaction_restart_level() {
	// TODO: reload level code here..
	gamewindow_start_game(&gameWindow, 0);
	
	// Switch to game screen
	window_stack_push(&gameWindow.window, true);
}
void menuaction_new_game() {
	// TODO: load level 1
	gamewindow_start_game(&gameWindow, 0);
	
	// Switch to game screen
	window_stack_push(&gameWindow.window, true);
	
	// Set menu type to INGAME
	menuwindow_set_menutype(&menuWindow, INGAME);
}
void menuaction_select_level() {
	// TODO: something to provide level select
	
}
void menuaction_options() {
	// TODO: open options menu or something
}
void menuaction_about() {
	// Switch to about screen
	window_stack_push(&aboutWindow.window, true);
}

void menuwindow_init_menuitems(MenuWindow *menuWindow) {
  menuitem_init_text_action(&menuWindow->allMenuItems[0], "Continue game", menuaction_continue_game);
  menuitem_init_text_action(&menuWindow->allMenuItems[1], "Restart level", menuaction_restart_level);
  menuitem_init_text_action(&menuWindow->allMenuItems[2], "New game", menuaction_new_game);
  menuitem_init_text_action(&menuWindow->allMenuItems[3], "Select level", menuaction_select_level);
  menuitem_init_text_action(&menuWindow->allMenuItems[4], "Options", menuaction_options);
  menuitem_init_text_action(&menuWindow->allMenuItems[5], "About", menuaction_about);
}

void menuwindow_init(MenuWindow *menuWindow) {
  window_init(&menuWindow->window, "Main Menu");

  // background / logo
  menuWindow->window.layer.update_proc = windowlayer_logo_draw;
  
  // select first item
  menuWindow->selectedItem = 0;
  // init base menu items
  menuwindow_init_menuitems(menuWindow);
  // Set an empty list as shown menu items
  int shownMenu[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  menuwindow_set_shownmenu(menuWindow, shownMenu);  
  // init means app started, show start menu
  menuwindow_set_menutype(menuWindow, START);

  // click handlers
  window_set_click_config_provider(&menuWindow->window, (ClickConfigProvider) boot_menu_click_config_provider);

  // debug
  /*text_layer_init(&menuWindow->debugLayer, menuWindow->window.layer.frame);
  text_layer_set_text(&menuWindow->debugLayer, "");
  text_layer_set_text_alignment(&menuWindow->debugLayer, GTextAlignmentLeft);
  layer_add_child(&menuWindow->window.layer, &menuWindow->debugLayer.layer);*/
}



void handle_init(AppContextRef ctx) {
  (void)ctx;

  // Resources
  resource_init_current_app(&APP_RESOURCES);
  
  bmp_init_container(RESOURCE_ID_BOX, &IMG_BOX);
  bmp_init_container(RESOURCE_ID_WALL, &IMG_WALL);
  bmp_init_container(RESOURCE_ID_TARGET, &IMG_TARGET);
  bmp_init_container(RESOURCE_ID_PLAYER_UP, &IMG_PLAYER_UP);
  bmp_init_container(RESOURCE_ID_PLAYER_RIGHT, &IMG_PLAYER_RIGHT);
  bmp_init_container(RESOURCE_ID_PLAYER_DOWN, &IMG_PLAYER_DOWN);
  bmp_init_container(RESOURCE_ID_PLAYER_LEFT, &IMG_PLAYER_LEFT);
  bmp_init_container(RESOURCE_ID_ARROW_UP, &IMG_ARROW_UP);
  bmp_init_container(RESOURCE_ID_ARROW_RIGHT, &IMG_ARROW_RIGHT);
  bmp_init_container(RESOURCE_ID_ARROW_DOWN, &IMG_ARROW_DOWN);
  bmp_init_container(RESOURCE_ID_ARROW_LEFT, &IMG_ARROW_LEFT);
  
  
  // Initialize windows
  menuwindow_init(&menuWindow);
  gamewindow_init(&gameWindow);
  pausewindow_init(&pauseWindow);
  minimapwindow_init(&minimapWindow);
  aboutwindow_init(&aboutWindow);
  
  // Push menu window to the stack
  window_stack_push(&menuWindow.window, true);
}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  // Note: Failure to de-init this here will result in instability and
  //       unable to allocate memory errors.
  //	   All bmpcontainers must have a de-init here
  bmp_deinit_container(&IMG_BOX);
  bmp_deinit_container(&IMG_WALL);
  bmp_deinit_container(&IMG_TARGET);
  bmp_deinit_container(&IMG_PLAYER_UP);
  bmp_deinit_container(&IMG_PLAYER_RIGHT);
  bmp_deinit_container(&IMG_PLAYER_DOWN);
  bmp_deinit_container(&IMG_PLAYER_LEFT);
  bmp_deinit_container(&IMG_ARROW_UP);
  bmp_deinit_container(&IMG_ARROW_RIGHT);
  bmp_deinit_container(&IMG_ARROW_DOWN);
  bmp_deinit_container(&IMG_ARROW_LEFT);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
	.init_handler = &handle_init,
	.deinit_handler = &handle_deinit
	};
	app_event_loop(params, &handlers);
}