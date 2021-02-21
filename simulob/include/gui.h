#pragma once
#include "controller.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <ncurses.h>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
class PrintOut;
class SubWindow {
public:
  SubWindow(int h = 0, int w = COLS - 2, int y = 0, int x = 1);
  ~SubWindow();
  SubWindow(const SubWindow &);
  SubWindow &operator=(const SubWindow &);
  SubWindow &operator=(SubWindow &&);
  void refresh();
  void erase();
  int height() const { return _height; }
  int width() const { return _width; }
  int y() const { return _y; }
  int x() const { return _x; }
  void print_one();
  void print_n(int);
  void write(const char *, ...);
  bool scribendum() const;
  void push_scribenda(std::string);
  std::string pop_scribenda();
  WINDOW *handle() const { return _window; }

protected:
  WINDOW *_window;
  int _height{0};
  int _width{0};
  int _y{0};
  int _x{0};
  std::deque<std::string> _scribenda;
  std::mutex _mtx;
};
struct OrderCommand;
class Order;
class User;
class OrderBook;
class OrderSubmission;
class UserWindow : public SubWindow {
public:
  UserWindow(int h = 0, int w = COLS - 2, int y = 0, int x = 1);
  UserWindow(std::shared_ptr<User> user, int h = 0, int w = COLS - 2, int y = 0,
             int x = 1);
  ~UserWindow();
  void set_query(std::string);
  void set_usermtx(std::mutex *usermtx) { _usermtx = usermtx; }
  void set_usercond(std::condition_variable *usercond) { _usercond = usercond; }
  void set_user(std::mutex *usermtx, std::condition_variable *usercond);
  void set_user(std::shared_ptr<User>);
  void query();
  void first_query();
  void getchar();
  std::string response() { return _response; };
  void receive_from_console();
  OrderCommand process_response();
  bool updated{false};
  bool interrupted{false};

private:
  std::string _query;
  std::string _response;
  User *_user;
  std::mutex *_usermtx;
  std::condition_variable *_usercond;
};
class SparrowWindow : public SubWindow {
public:
  SparrowWindow(int, int, int, int);
  ~SparrowWindow();
  void add(const char *);
  void add(std::string);
};
struct IntPanel {
  std::shared_ptr<UserWindow> user;
  std::shared_ptr<SubWindow> sparrow;
  std::shared_ptr<SubWindow> status;
  std::shared_ptr<BookMonitor> bookmonitor;
};
namespace Interface {
IntPanel setup(int status_y, int sparrow_y, int user_y,
               std::shared_ptr<User> user);
void setup(IntPanel &, std::shared_ptr<OrderBook> &,
           std::shared_ptr<OrderSubmission> &);
void init(IntPanel &);
void display(IntPanel &);
void cdisplay(IntPanel);
void wait_for_F1();
void close();
}; // namespace Interface
