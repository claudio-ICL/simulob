#pragma once
#include<stdio.h>
#include<string>
#include<chrono>
#include<thread>
#include<random>
#include<mutex>
#include<condition_variable>
#include<deque>
#include<map>
#include<vector>
#include<cmath>
#include<cstdlib>
#include<algorithm>
#include"controller.h"
enum OrderDirection {bid=1, ask=-1}; //LOBSTER convention
enum OrderStatus {queued, executed, cancelled};
static std::vector<OrderDirection> all_directions {OrderDirection::bid, OrderDirection::ask};
struct OrderCommand{
  int price{0}; int size{0}; OrderDirection d{OrderDirection::ask};
};
class BestPrice;
class OrderBook;
class SubWindow;
class Order: public tController{
  public:
    Order(int, int, OrderDirection);
    Order(int, int, OrderDirection, SubWindow*);
    Order(int, int, OrderDirection, std::shared_ptr<SubWindow>);
    ~Order();
    bool operator<(const Order&) const; //Execution priority 
    OrderDirection direction() const {return _direction;}
    std::string side() const;
    std::chrono::time_point<std::chrono::system_clock> timestamp() const {return _timestamp;}
    int id() const {return _id;}
    OrderStatus status() const {return _status;}
    int price() const {return _price;}
    int size() const {return _size;}
    int active_size() const {return (_status==OrderStatus::queued)? _size : 0;}
    void size(int new_size){_size=new_size;}
    void mark_as(OrderStatus);
    virtual bool update(){return false;}
    std::mutex* mtx() {return &_mtx;}
  protected:
    static long count;
    long const _id;
    int _price;
    int _size;
    OrderDirection const _direction;
    std::chrono::time_point<std::chrono::system_clock> const _timestamp;
    OrderStatus _status;
    mutable std::mutex _mtx;
};
class StandingOrder: public Order{
  public:
    StandingOrder(int,int,  OrderDirection);
    StandingOrder(int,int,  OrderDirection, SubWindow*);
    StandingOrder(int,int,  OrderDirection, std::shared_ptr<SubWindow>);
    ~StandingOrder();
    void set_decay(OrderBook*, double);
    void set_decay(BestPrice*, int, double);
    bool update();
    void cancel();
    int distance_from_refprice() const;
    static std::random_device  r;
    static std::default_random_engine  generator;
    static std::uniform_real_distribution<double>  uniform_distr;
  private:
    BestPrice* _refprice;
    int _ticksize{100};
    float _decayrate{0.0};
};
