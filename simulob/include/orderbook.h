#pragma once
#include "controller.h"
#include "gui.h"
#include "hawkes.h"
#include "orders.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <stdio.h>
#include <thread>
#include <vector>
class BestPrice : public Controller {
public:
  BestPrice(OrderDirection, int);
  ~BestPrice();
  BestPrice(const BestPrice &);
  BestPrice(BestPrice &&);
  BestPrice &operator=(const BestPrice &);
  BestPrice &operator=(BestPrice &&);
  bool is_better_than(std::shared_ptr<Order>);
  void update(std::shared_ptr<Order>);
  void new_(std::shared_ptr<Order>);
  void invalidate();
  OrderDirection direction() const { return _direction; }
  int price() const { return _price; }
  int cached_price() const { return _cached; }
  void cache_price(int p) { _cached = p; }
  int value() const;
  std::shared_ptr<Order> order() const { return _order; }

private:
  OrderDirection _direction;
  int _price;
  int _cached;
  /* Not-owned*/
  std::shared_ptr<Order> _order;
};
class OrderQueue : public std::deque<std::shared_ptr<Order>>,
                   public Controller {
public:
  OrderQueue(OrderDirection, int, std::mutex *, std::condition_variable *);
  ~OrderQueue();
  OrderQueue(const OrderQueue &);
  OrderQueue(OrderQueue &&);
  OrderQueue &operator=(const OrderQueue &);
  OrderQueue &operator=(OrderQueue &&);
  void add(std::shared_ptr<Order> &&);
  int execute(std::shared_ptr<Order>);
  void sort();
  bool update();
  void erase_order(iterator &);
  void simulate();
  void stop();
  void renew_bestprice();
  void erase_notqueued();
  bool is_sorted();
  bool is_empty();
  OrderDirection direction() const { return _direction; }
  BestPrice *bestprice() { return &_bestprice; }
  std::shared_ptr<Order> bestprice_order() const { return _bestprice.order(); }
  int bestprice_value() const { return _bestprice.value(); }
  void invalidate_bestprice() { _bestprice.invalidate(); }
  void cache_bestprice(int p) { _bestprice.cache_price(p); }
  long volume() const { return _volume; }
  std::mutex *mtx() { return &_mtx; }
  std::condition_variable *condition() { return &_cond; }
  std::mutex *bookmtx() const { return _bookmtx; }
  std::condition_variable *bookcond() const { return _bookcond; }

private:
  OrderDirection _direction;
  BestPrice _bestprice;
  long _volume{0};
  std::mutex _mtx;
  std::condition_variable _cond;
  std::mutex *_bookmtx;
  std::condition_variable *_bookcond;
};
class User;
struct MatchRes {
  bool transaction{false};
  int transaction_size{0};
  int transaction_price{0};
};
class OrderBook : public tController {
public:
  OrderBook(int ticksize = 100, int opening_bidprice = 100000,
            int opening_askprice = 100500);
  OrderBook(int, int, int, std::shared_ptr<SubWindow>);
  ~OrderBook();
  void add(std::shared_ptr<Order> &&);
  void add(std::shared_ptr<StandingOrder> &&order, double decayrate);
  void add(OrderCommand &&);
  static MatchRes match_orders(std::shared_ptr<Order> &,
                               std::shared_ptr<Order> &);
  static bool is_match(std::shared_ptr<Order> &, std::shared_ptr<Order> &);
  void launch_simulation();
  void stop();
  int midprice() { return (askprice() + bidprice()) / 2; }
  int askprice();
  int bidprice();
  long askvolume();
  long bidvolume();
  int ticksize() const { return _ticksize; }
  std::map<OrderDirection, OrderQueue> &queue() { return _queue; }
  std::map<OrderDirection, OrderQueue> _queue;
  std::mutex &mtx() { return _mtx; }
  std::mutex *mtx_ptr() { return &_mtx; }
  std::condition_variable *cond_ptr() { return &_cond; }
  void set_subwindow(std::shared_ptr<SubWindow>);

private:
  int const _ticksize;
  std::mutex _mtx;
  std::condition_variable _cond;
};
class OrderSubmission : public tController {
public:
  OrderSubmission(std::shared_ptr<OrderBook>, double = 0.0);
  ~OrderSubmission();
  void init_user();
  void set_userwindow(std::shared_ptr<UserWindow>);
  void set_subwindow(std::shared_ptr<SubWindow>);
  HawkesProcess hawkes;
  OrderCommand generate_command(Event &&);
  void submit(int, int, OrderDirection);
  void submit(OrderCommand);
  void simulate();
  void launch_simulation();
  void stop();
  std::shared_ptr<User> user() const { return _user; };
  static std::default_random_engine generator;
  static std::discrete_distribution<int> distribution;

private:
  std::shared_ptr<OrderBook> _book;
  std::shared_ptr<User> _user;
  double _decayrate{0.0};
};
