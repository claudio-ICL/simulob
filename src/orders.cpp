#include<iostream>
#include"orders.h"
#include"orderbook.h"
#include "user.h"
#include "gui.h"
#define sleep10 std::this_thread::sleep_for(std::chrono::milliseconds(10))
long Order::count{0};
Order::Order(int price, int size, OrderDirection direction): 
  _id(count++), _price(price), _size(size), _direction(direction),  _timestamp(std::chrono::system_clock::now()), _status(OrderStatus::queued){
}
Order::Order(int price, int size, OrderDirection direction, SubWindow* subwin): 
  Order(price, size, direction){
    set_subwindow(subwin);
}
Order::Order(int price, int size, OrderDirection direction, std::shared_ptr<SubWindow> subwin): 
  Order(price, size, direction)
{
    set_subwindow(subwin);
}
Order::~Order(){
  //write("Order #%d is being destructed\n", _id);
  _mtx.unlock();
}
void Order::mark_as(OrderStatus new_status){
  _status = new_status;
}
bool Order::operator<(const Order& other) const{
  if (_status!=other.status()){
    return _status==OrderStatus::queued;
  }
  if ((_direction==other.direction())&(_price!=other.price())){
    int sign = (_direction==OrderDirection::bid)? -1 : 1;
    int p = other.price();
    return sign*_price<sign*p;
  }
  else{
    return _timestamp<other.timestamp();
  }
}
std::string Order::side() const{
  return (_direction==OrderDirection::ask)? "ask" : "bid";
}
StandingOrder::StandingOrder(int price, int size, OrderDirection direction): 
  Order{price, size, direction}{}
StandingOrder::StandingOrder(int price, int size, OrderDirection direction, SubWindow* subwindow): 
  Order{price, size, direction, subwindow}{}
StandingOrder::StandingOrder(int price, int size, OrderDirection direction, std::shared_ptr<SubWindow> subwindow): 
  Order{price, size, direction, subwindow}{}
StandingOrder::~StandingOrder(){}
int StandingOrder::distance_from_refprice() const {
  int sgn = (_direction==OrderDirection::bid)? 1 : -1;
  int delta_price = std::max(0, sgn*(_refprice->value() - _price));
  return delta_price/_ticksize;
}
void StandingOrder::cancel(){
  std::chrono::time_point<std::chrono::system_clock> time_start = std::chrono::system_clock::now();
  long countdown = static_cast<long>(1000*uniform_distr(generator));
  while 
  (  
    (
     _status==OrderStatus::queued
    )
    &&
    (
     (std::chrono::duration_cast
      <std::chrono::milliseconds>
      (std::chrono::high_resolution_clock::now() - time_start).count()
     )<countdown
    )
    &&
    (
     this->go()
    )
  )
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::lock_guard<std::mutex> lock(_mtx);
  if (_status==OrderStatus::queued){
    write("Order #%d is cancelled from %s queue; price=%d, level=%d \n", _id, side().c_str(),_price, 1+ distance_from_refprice());
    mark_as(OrderStatus::cancelled);
  }
}
bool StandingOrder::update(){
  if (_status==OrderStatus::queued){
      /*coefficient of exponential decay lambda is larger if the order is near the best price*/
      double lambda = _decayrate/(1+10*distance_from_refprice());
      auto t =std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _timestamp).count();
      double threshold = 1.0 - exp(-lambda*t);
      double U = uniform_distr(generator);
      if (U<=threshold){
        //write("Order #%d _decayrate=%f;  threshold=%f\n",_id, _decayrate, threshold);
        _threads.emplace_back(std::thread(&StandingOrder::cancel, this));
        return true;
      }
  }
  return false;
}
std::random_device StandingOrder::r;
std::default_random_engine StandingOrder::generator(r());
std::uniform_real_distribution<double> StandingOrder::uniform_distr(0,1);
void StandingOrder::set_decay(OrderBook* book, double updatechance){
  //write("StandingOrder::set_decay\n");
  OrderQueue& queue = (book->_queue).at(_direction);
  set_decay(queue.bestprice(), book->ticksize(), updatechance);
}
void StandingOrder::set_decay(BestPrice* refprice, int ticksize, double decayrate){
  _refprice=refprice;
  _ticksize=ticksize;
  _decayrate=decayrate;
}
