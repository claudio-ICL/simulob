#include"gui.h"
#include<cassert>
#include "orders.h"
#include"orderbook.h"
#include "user.h"
SubWindow::SubWindow(int h, int w, int y, int x): 
  _height(h), _width(w), _y(y), _x(x)
{
  _window = newwin(h,w,y,x);
  assert (_window!=nullptr);
}
SubWindow::~SubWindow(){
}
SubWindow::SubWindow(const SubWindow& source){
  _height = source.height();
  _width = source.width();
  _y = source.y();
  _x = source.x();
  _window = newwin(_height, _width, _y, _x);
}
SubWindow& SubWindow::operator= (const SubWindow& source){
  _height = source.height();
  _width = source.width();
  _y = source.y();
  _x = source.x();
  _window = newwin(_height, _width, _y, _x);
  return *this;
}
SubWindow& SubWindow::operator= (SubWindow&& source){
  _height = source.height();
  _width = source.width();
  _y = source.y();
  _x = source.x();
  _window = newwin(_height, _width, _y, _x);
  return *this;
}
void SubWindow::refresh(){
  /*int err = */ wrefresh(_window);
  //assert (err!=ERR);
}
void SubWindow::erase(){
  /*int err = */werase(_window);
  //assert (err!=ERR);
}
bool SubWindow::scribendum() const{
  return !_scribenda.empty();
}
void SubWindow::print_one(){
  if (scribendum()){
    std::string printout = pop_scribenda();
    /*int nchar = */wprintw(_window, "%s", printout.c_str());
    //assert (nchar>=0);
  }
}
void SubWindow::print_n(int n){
  int i{0};
  while ((scribendum())&&(i<n)){
    std::string printout = pop_scribenda();
    /*int nchar = */wprintw(_window, "%s", printout.c_str());
    //assert (nchar>=0);
    ++i;
  }
}
void SubWindow::push_scribenda(std::string printout){
  std::lock_guard<std::mutex> lock(_mtx);
  _scribenda.push_back(printout);
}
std::string SubWindow::pop_scribenda(){
  std::lock_guard<std::mutex> lock(_mtx);
  if (!_scribenda.empty()){
    std::string res = _scribenda.front()  ; 
    _scribenda.pop_front();
    return res;
  }
  return std::string("");
}
void SubWindow::write(const char* format, ...){
  char txt[200];
  va_list args;
  va_start(args, format);
  vsnprintf(txt,200, format, args);
  std::string text(txt);
  push_scribenda(txt);
  va_end(args);
}
UserWindow::UserWindow(int h, int w, int y, int x): SubWindow(h,w,y,x){
  set_query("Enter new order: ");
  scrollok(_window, true);
  idlok(_window, true);
}
UserWindow::UserWindow(std::shared_ptr<User> user, int h, int w, int y, int x): UserWindow(h,w,y,x){
  set_user(user);
}
UserWindow::~UserWindow(){
  //printf("UserWindow Destructor\n");
}
void UserWindow::set_query(std::string sentence){
  _query = sentence;
}
void UserWindow::set_user(std::mutex* usermtx, std::condition_variable* usercond){
  std::lock_guard<std::mutex> lock(*usermtx);
  set_usermtx(usermtx);
  set_usercond(usercond);
  updated=true;
  usercond->notify_all();
}
void UserWindow::set_user(std::shared_ptr<User> user){
  _user = user.get();
  set_user(user->mtx(), user->cond());
}
void UserWindow::query(){
  write("%s", _query.c_str());
  _response.clear();
}
void UserWindow::first_query(){
  write("%s", _query.c_str());
  _response.clear();
  updated=false;
  this->print_one();
  this->refresh();
}
void UserWindow::getchar(){
  int c = wgetch(_window);
  if ((updated==false)&&(c!=ERR)){
    if (c==KEY_F(1)){
      write("Interrupted\n");
      this->print_one(); this->refresh();
      _user->stop();
      _user->stop_ordersubmission();
      _user->notify_all();
      interrupted=true;
    }
    else{
      _response.push_back((c=='\n')? '\0' : c);
      if ((_response.size()>50)||(c=='\n')||(c=='\0')){
        std::lock_guard<std::mutex> lock(*_usermtx);
        _usercond->notify_one();
      }
    }
  }
}
OrderCommand UserWindow::process_response(){
  //write("UserWindow::process_response()\n");
  OrderCommand cmd;
  char side;
  std::istringstream linestream(_response);
  try{
    linestream >> side >> cmd.price >> cmd.size;
    switch (side){
      case 'a' : cmd.d = OrderDirection::ask; break;
      case 'A' : cmd.d = OrderDirection::ask; break;
      case 'b' : cmd.d = OrderDirection::bid; break;
      case 'B' : cmd.d = OrderDirection::bid; break;
      case '1' : cmd.d = OrderDirection::bid; break;
      default  : cmd.d = OrderDirection::ask; break;
    }
  }catch(const std::exception&){}
  cmd.price = std::max(0, cmd.price);
  cmd.size = std::max(0, cmd.size);
  write("Your %ldth order: d=%d, price=%d, size=%d\n", 1+_user->submissions(), cmd.d, cmd.price, cmd.size);
  updated=true;
  return cmd; 
}
SparrowWindow::SparrowWindow(int h,int w,int y,int x):
  SubWindow(h,w,y,x){
  scrollok(_window, true);
  idlok(_window, true);
}
SparrowWindow::~SparrowWindow(){
  //printf("SparrowWindow Destructor\n");
}
void SparrowWindow::add(const char* msg){
  write(msg);
}
void SparrowWindow::add(std::string s){
  add(s.c_str());
}
IntPanel Interface::setup(int status_y, int sparrow_y, int user_y, std::shared_ptr<User> user){
  initscr();
  status_y=std::max(0,status_y);
  user_y = std::max(user_y,LINES-6);
  sparrow_y=std::max(sparrow_y,LINES/10);
  assert (sparrow_y < user_y);
  assert (status_y < sparrow_y);
  IntPanel interface;
  interface.sparrow = std::make_shared<SparrowWindow>(user_y-sparrow_y-2, COLS-2, sparrow_y, 1);
  interface.user = std::make_shared<UserWindow>(user, LINES - user_y-1, COLS-2, user_y, 1);
  user->set_userwindow(interface.user);
  interface.status = std::make_shared<SubWindow>(sparrow_y - status_y - 1, COLS-2, status_y, 1);
  return interface;
}
void Interface::setup(IntPanel& interface, std::shared_ptr<OrderBook>& ob, std::shared_ptr<OrderSubmission>& osub){
  assert (interface.sparrow->handle()!=nullptr);
  assert (interface.user->handle()!=nullptr);
  assert (interface.status->handle()!=nullptr);
  interface.bookmonitor = std::make_shared<BookMonitor>(ob);
  interface.bookmonitor->set_subwindow(interface.status);
  ob->set_subwindow(interface.sparrow);
  osub->set_subwindow(interface.sparrow);
}
void Interface::init(IntPanel& interface){
  assert (interface.sparrow->handle()!=nullptr);
  keypad(stdscr, TRUE);
  keypad(interface.user->handle(), TRUE);
  cbreak();
  nodelay(interface.user->handle(),TRUE);
  mvprintw(interface.user->y() - 1, 0, "-------------------------------------");
  refresh();
}
void Interface::display(IntPanel& interface){
  curs_set(0);
  interface.bookmonitor->launch();
  interface.status->print_one();
  interface.status->refresh();
  interface.user->first_query();
  interface.sparrow->refresh();
  while (!interface.user->interrupted){
    interface.sparrow->print_n(5);
    interface.sparrow->refresh();
    {
      std::lock_guard<std::mutex> lock(interface.bookmonitor->mtx);
      if (interface.bookmonitor->updated){
        interface.status->erase();
        interface.status->print_one();
        interface.status->refresh();
        interface.bookmonitor->updated=false;
      }
    }
    {
      if (interface.user->updated){
        interface.user->print_one();
        interface.user->query();
        interface.user->print_one();
        interface.user->refresh();
        interface.user->updated=false;
      }
      else{
        interface.user->getchar();
      }
    }
  }
  interface.bookmonitor->stop();
  curs_set(1);
}
void Interface::cdisplay(IntPanel interface){
  Interface::display(interface);
}
void Interface::wait_for_F1(){
  nodelay(stdscr,FALSE);
  curs_set(1);
  while (getch()!=KEY_F(1)){}
  endwin();
}

void Interface::close(){
  try{endwin();} catch(...){}
}
