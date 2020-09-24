#pragma once
#include<vector>
#include<future>
#include<chrono>
#include<thread>
#include<stdio.h>
#include<ncurses.h>
#include <stdarg.h>
#define sleep10 std::this_thread::sleep_for(std::chrono::milliseconds(10))
class PrintOut{
  public:
    PrintOut(const char* str, ...);
    PrintOut(const char* , va_list& );
    ~PrintOut();
    PrintOut(const PrintOut& src);
    PrintOut(const PrintOut&& src);
    PrintOut& operator=(const  PrintOut& src);
    PrintOut& operator=(const  PrintOut&& src);
    const char* format;
    mutable va_list args;
};
class SubWindow;
class Controller{
  public:
    Controller();
    ~Controller();
    bool go();
    virtual void stop();
    void write(const char*, ...);
    virtual void set_subwindow(std::shared_ptr<SubWindow>);
    virtual void set_subwindow(SubWindow*);
  protected:
    bool _go{true};
    std::mutex _gomtx;
    std::shared_ptr<SubWindow> _subwindow;
};
class tController: public Controller{
  public:
    ~tController();
    void join();
    void detach();
  protected:
    std::vector<std::thread> _threads;
};
class OrderBook;
class BookMonitor: public tController{
  public:
    BookMonitor(std::shared_ptr<OrderBook>);
    ~BookMonitor();
    void set_book(std::shared_ptr<OrderBook>);
    void show();
    void launch();
    std::mutex mtx;
    bool updated{false};
  private:
    std::shared_ptr<OrderBook> _book;
    std::mutex* _bookmtx;
    std::condition_variable* _bookcond;
};
