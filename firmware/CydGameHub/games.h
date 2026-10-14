// games.h - the game registry. To add a game: write game_x.h (see hub.h), include it, add a row.
// Order here = order in the menu. High scores are stored per game index AND per difficulty.
#pragma once
#include "hub.h"
#include "game_snake.h"
#include "game_flap.h"
#include "game_breakout.h"
#include "game_pong.h"
#include "game_invaders.h"
#include "game_asteroids.h"
#include "game_tetris.h"
#include "game_2048.h"
#include "game_simon.h"
#include "game_whack.h"
#include "game_runner.h"
#include "game_tictactoe.h"
#include "game_connect4.h"
#include "game_reversi.h"
#include "game_memory.h"
#include "game_mines.h"
#include "game_lightsout.h"
#include "game_sudoku.h"
#include "game_slide.h"

//                 name          tile colour  ms  lower  init            tick            icon
static const GameDef GAMES[] = {
  {"Snake",       C_GREEN,      33, false, Snake::init,      Snake::tick,      Snake::icon},
  {"Flap",        C_YELLOW,     33, false, Flap::init,       Flap::tick,       Flap::icon},
  {"Breakout",    C_ORANGE,     33, false, Breakout::init,   Breakout::tick,   Breakout::icon},
  {"Pong",        C_CYAN,       33, false, Pong::init,       Pong::tick,       Pong::icon},
  {"Invaders",    C_MAGENTA,    33, false, Invaders::init,   Invaders::tick,   Invaders::icon},
  {"Asteroids",   C_LGREY,      33, false, Asteroids::init,  Asteroids::tick,  Asteroids::icon},
  {"Tetris",      C_BLUE,       33, false, Tetris::init,     Tetris::tick,     Tetris::icon},
  {"2048",        C_GOLD,       33, false, G2048::init,      G2048::tick,      G2048::icon},
  {"Simon",       C_RED,        33, false, Simon::init,      Simon::tick,      Simon::icon},
  {"Whack",       C_PINK,       33, false, Whack::init,      Whack::tick,      Whack::icon},
  {"Runner",      C_TEAL,       33, false, Runner::init,     Runner::tick,     Runner::icon},
  {"Tic-Tac-Toe", C_LGREY,      33, false, TicTacToe::init,  TicTacToe::tick,  TicTacToe::icon},
  {"Connect 4",   C_RED,        33, false, Connect4::init,   Connect4::tick,   Connect4::icon},
  {"Reversi",     C_GREEN,      33, false, Reversi::init,    Reversi::tick,    Reversi::icon},
  {"Memory",      C_PURPLE,     33, true,  Memory::init,     Memory::tick,     Memory::icon},
  {"Minesweeper", C_GREY,       33, true,  Mines::init,      Mines::tick,      Mines::icon},
  {"Lights Out",  C_YELLOW,     33, true,  LightsOut::init,  LightsOut::tick,  LightsOut::icon},
  {"Sudoku",      C_CYAN,       33, true,  Sudoku::init,     Sudoku::tick,     Sudoku::icon},
  {"Slide",       C_ORANGE,     33, true,  Slide15::init,    Slide15::tick,    Slide15::icon},
};
static const int NUM_GAMES = (int)(sizeof(GAMES) / sizeof(GAMES[0]));
