#pragma once
#include<future>
#include<thread>
#include<mutex>
#include"controller.h"
#include"orders.h"

class OrderBook;
class UserWindow;
class OrderSubmission;
class User: public Controller{
  public:
    User(std::shared_ptr<OrderBook>);
    ~User();
    void set_ordersubmission(OrderSubmission*);
    void send_order(int, int, OrderDirection);
    void send_order(OrderCommand);
    void simulate();
    void stop_ordersubmission(); 
    void set_userwindow(std::shared_ptr<UserWindow> uw);
    std::mutex* mtx(){return &_mtx;}
    std::condition_variable* cond(){return &_cond;}
    long submissions() const {return _submissions;}
    void notify_all() {_cond.notify_all();}
  private:
    OrderBook* _book;
    UserWindow* _userwindow;
    OrderSubmission* _ordersubmission;
    std::mutex _mtx;
    std::condition_variable _cond;
    long _submissions{0};

};
