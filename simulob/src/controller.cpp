#include "controller.h"
#include "gui.h"
#include "orderbook.h"
PrintOut::PrintOut(const char *str, ...) {
  format = str;
  va_list list;
  va_start(list, str);
  va_copy(args, list);
}
PrintOut::PrintOut(const char *str, va_list &list) {
  format = str;
  va_copy(args, list);
}
PrintOut::~PrintOut() { va_end(args); }
PrintOut::PrintOut(const PrintOut &src) {
  format = src.format;
  va_copy(args, src.args);
}
PrintOut::PrintOut(const PrintOut &&src) : PrintOut(src) {}
PrintOut &PrintOut::operator=(const PrintOut &src) {
  format = src.format;
  va_copy(args, src.args);
  return *this;
}
PrintOut &PrintOut::operator=(const PrintOut &&src) {
  format = src.format;
  va_copy(args, src.args);
  return *this;
}
Controller::Controller() { _go = true; }
Controller::~Controller() { stop(); }
void Controller::set_subwindow(std::shared_ptr<SubWindow> subwindow) {
  _subwindow = subwindow;
}
void Controller::set_subwindow(SubWindow *subwindow) {
  _subwindow.reset(subwindow);
}
bool Controller::go() {
  std::lock_guard<std::mutex> lock(_gomtx);
  return _go;
}
void Controller::stop() {
  std::lock_guard<std::mutex> lock(_gomtx);
  _go = false;
}
void Controller::write(const char *format, ...) {
  char txt[200];
  va_list args;
  va_start(args, format);
  vsnprintf(txt, 200, format, args);
  if (_subwindow) {
    std::string text(txt);
    _subwindow->push_scribenda(txt);
  } else
    printf("%s", txt);
  va_end(args);
}
tController::~tController() {
  stop();
  join();
}
void tController::join() {
  for (auto it = _threads.begin(); it != _threads.end(); ++it) {
    if (it->joinable())
      try {
        it->join();
      } catch (std::system_error &e) {
      }
  }
}
void tController::detach() {
  for (auto it = _threads.begin(); it != _threads.end(); ++it) {
    try {
      it->detach();
    } catch (std::system_error &e) {
    }
  }
}
BookMonitor::BookMonitor(std::shared_ptr<OrderBook> ob) { set_book(ob); }
BookMonitor::~BookMonitor() {}
void BookMonitor::set_book(std::shared_ptr<OrderBook> book) {
  _book = book;
  _bookmtx = book->mtx_ptr();
  _bookcond = book->cond_ptr();
}
void BookMonitor::show() {
  write("Ask best price: %8d            Ask tot volumes: %10ld\nBid best "
        "price: %8d            Bid tot volumes: %10ld",
        _book->askprice(), _book->askvolume(), _book->bidprice(),
        _book->bidvolume());
  while (_go) {
    std::unique_lock<std::mutex> lock(*_bookmtx);
    (*_bookcond).wait(lock);
    write("Ask best price: %8d            Ask tot volumes: %10ld\nBid best "
          "price: %8d            Bid tot volumes: %10ld",
          _book->askprice(), _book->askvolume(), _book->bidprice(),
          _book->bidvolume());
    {
      std::lock_guard<std::mutex> lock(mtx);
      updated = true;
    }
  }
}
void BookMonitor::launch() {
  _threads.emplace_back(std::thread(&BookMonitor::show, this));
}
