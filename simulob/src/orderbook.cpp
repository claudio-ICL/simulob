#include"orderbook.h"
#include<iostream>
#include"user.h"
#include"hawkes.h"
#include<cassert>
#define name_of_side std::string side=(_direction==OrderDirection::ask)?"ask":"bid"
using namespace std::chrono_literals;

OrderDirection opposite(OrderDirection d){
 return (d==OrderDirection::ask)? OrderDirection::bid : OrderDirection::ask;
}

MatchRes OrderBook::match_orders(std::shared_ptr<Order>& order_1, std::shared_ptr<Order>& order_2){
  MatchRes res;
  res.transaction=false;
  if ((order_1)&&(order_2)){
    std::lock_guard<std::mutex> lock_1(*order_1->mtx());
    std::lock_guard<std::mutex> lock_2(*order_2->mtx());
    if ((order_1->status()==OrderStatus::queued) && (order_2->status() ==OrderStatus::queued)){
      if ((order_1->direction()!=order_2->direction())&&(order_1->id()!=order_2->id())){
        std::shared_ptr<Order>& ask = (order_1->direction()==OrderDirection::ask)? order_1 : order_2;
        std::shared_ptr<Order>& bid = (order_1->direction()==OrderDirection::bid)? order_1 : order_2;
        //if((ask->id()==bid->id())||(ask->id()<0)||(bid->id()<0)) return res;
        if (ask->price()<=bid->price()){
          res.transaction=true;
          res.transaction_size = std::min(ask->size(), bid->size());
          res.transaction_price = (*ask < *bid)? ask->price() : bid->price();
          ask->size(ask->size()-res.transaction_size);
          bid->size(bid->size()-res.transaction_size);
          if (ask->size()<=0) ask->mark_as(OrderStatus::executed);
          if (bid->size()<=0) bid->mark_as(OrderStatus::executed);
        }
      }
    }
  }
  return res;
}
bool OrderBook::is_match(std::shared_ptr<Order>& order_1, std::shared_ptr<Order>& order_2){
  if ((order_1)&&(order_2)){
    std::lock_guard<std::mutex> lock_1(*order_1->mtx());
    std::lock_guard<std::mutex> lock_2(*order_2->mtx());
    if ((order_1->status()==OrderStatus::queued) && (order_2->status() ==OrderStatus::queued)){
      if ((order_1->direction()!=order_2->direction())&&(order_1->id()!=order_2->id())){
        std::shared_ptr<Order>& ask = (order_1->direction()==OrderDirection::ask)? order_1 : order_2;
        std::shared_ptr<Order>& bid = (order_1->direction()==OrderDirection::bid)? order_1 : order_2;
        if(ask->id()==bid->id()) return false;
        return ask->price()<=bid->price();
      }
    }
  }
  return false;
}
OrderQueue::OrderQueue(OrderDirection d, int opening_price, std::mutex* bookmtx, std::condition_variable* bookcond):
  _direction(d), _bestprice(BestPrice(d, opening_price)), _bookmtx(bookmtx), _bookcond(bookcond) 
{
}
OrderQueue::~OrderQueue(){
  /*
  std::string side = (_direction==OrderDirection::ask)? "ask" : "bid";
  //printf("OrderQueue Destructor - side: %s\n", side.c_str());
  */
  _bookcond->notify_all();
  _cond.notify_all();
}
OrderQueue::OrderQueue(const OrderQueue& source): 
  std::deque<std::shared_ptr<Order>>{source},
  Controller(),
  _direction(source.direction()), 
  _bestprice(BestPrice(_direction, source.bestprice_value())),
  _bookmtx(source.bookmtx()),
  _bookcond(source.bookcond())
{
}
OrderQueue::OrderQueue(OrderQueue&& source): 
  std::deque<std::shared_ptr<Order>>{source},
  Controller(),
  _direction(source.direction()), 
  _bestprice(BestPrice(_direction, source.bestprice_value())),
  _bookmtx(source.bookmtx()),
  _bookcond(source.bookcond())
{
}
OrderQueue& OrderQueue::operator=(const OrderQueue& source){
  std::lock_guard<std::mutex> lock(*_bookmtx);
  //std::cout << "OrderQueue Copy Assignment this: " << this << std::endl;
  _direction = source.direction(); 
  _bestprice = BestPrice(_direction, source.bestprice_value());
  _bookmtx = source.bookmtx();
  _bookcond = source.bookcond();
  _bestprice.new_(source.bestprice_order());
  return *this;
}
OrderQueue& OrderQueue::operator=(OrderQueue&& source){
  std::lock_guard<std::mutex> lock(*_bookmtx);
  //std::cout << "OrderQueue Move Assignment this: " << this << std::endl;
  _direction = source.direction(); 
  _bestprice = BestPrice(_direction, source.bestprice_value());
  _bookmtx = source.bookmtx();
  _bookcond = source.bookcond();
  _bestprice.new_(source.bestprice_order());
  return *this;
}
  
void OrderQueue::add(std::shared_ptr<Order>&& order){
  //write("OrderQueue:: add() direction=%d, order #%d \n", _direction, order->id());
  if (order){
    if (order->direction()==_direction){
      if (order->status()==OrderStatus::queued){
        std::lock_guard<std::mutex> lock(_mtx);
        _volume+=order->size();
        auto pos =
          std::lower_bound(
            this->begin(), this->end(),  
            order, 
            [](std::shared_ptr<Order> order_1, std::shared_ptr<Order> order_2)
            {return *order_1 < *order_2;});
        _bestprice.update(order);
        this->insert(pos, order);
      }
    }
  }
}
int OrderQueue::execute(std::shared_ptr<Order> order){
  if (!order) return 0;
  int trade{0};
  if (!is_empty()){
    std::lock_guard<std::mutex> lock(_mtx);
    if (order->direction()!=_direction){
      int order_id = order->id();
      std::string order_side = order->side();
      for(
        auto it = this->begin();
        (order->status()!=OrderStatus::executed) && (it!=this->end()) &&
        (_direction * order->price() <= _direction * (*it)->price()) //_direction is either 1 (bid queue) or -1 (ask queue)
        ;
      ){
        int id = (*it)->id();
        std::string side = (*it)->side();
        MatchRes mr = OrderBook::match_orders(*it, order);
        if (mr.transaction){
          write(
              "Limit orders #%d(%s) and #%d(%s) match:  transaction price=%d, transaction size=%d\n", 
              id, side.c_str(), order_id, order_side.c_str(),
              mr.transaction_price, mr.transaction_size
          );
          trade+=mr.transaction_price*mr.transaction_size;
          _volume -= mr.transaction_size;
        }
        if ((*it)->status()!=OrderStatus::queued)
          erase_order(it);
        else
          ++it;
      }
      if (this->empty()){
        name_of_side;
        write("\n%s queue is now empty \n\n", side.c_str());
        this->invalidate_bestprice();
        this->cache_bestprice(order->price());
      }
    }
  }
  else{
      this->invalidate_bestprice();
      int p = (_direction == OrderDirection::ask)? std::max(_bestprice.cached_price(), order->price()) : std::min(_bestprice.cached_price(), order->price());
      this->cache_bestprice(p);
  }
  //write("OrderQueue::execute() direction=%d Now exiting\n", _direction);
  return trade;
}
void OrderQueue::simulate(){
  {
    std::lock_guard<std::mutex> booklock(*_bookmtx);
    _bookcond->notify_all();
  }
  while (this->go()){
     if (!is_empty())
     {
       bool alterations = update();
       if (alterations){
        std::lock_guard<std::mutex> booklock(*_bookmtx);
        renew_bestprice();
        _bookcond->notify_all();//we notify OrderBook so that it can refresh volumes
       }
     }
  }
}
void OrderQueue::stop(){
  _cond.notify_all();
  for (auto it = this->begin(); it!=this->end(); ++it){
    (*it)->stop();
  }
  std::lock_guard<std::mutex> lock(_gomtx);
  _go=false;
}
void OrderQueue::erase_order(iterator& it){
  if ((this->begin()<= it) && (it<this->end())){
    if (*it){
      //int id = (*it)->id();
      _volume -= (*it)->size();
      it = this->erase(it);
      if (it!=this->end()){
        _bestprice.update(*it);
      }
      //write("OrderQueue::erase_order order #%d has been erased\n", id);
    }
    else
      it = this->erase(it);
  }
}
bool OrderQueue::update(){
   //write("OrderQueue::update() direction=%d\n", _direction);
   bool cancellations{false};
   bool deletions{false};
   std::lock_guard<std::mutex> lock(_mtx);
   for (iterator it = this->begin(); it!=this->end();){
       if (((*it)->status()!=OrderStatus::queued)||((*it)->size()<=0)){
         erase_order(it);
         deletions=true;
       }
       else{
         bool cancelling = (*it)->update();
         /*
         if (it!=this->begin()){
           int price = (*it)->price();
           int prev_price = (*(it-1))->price(); 
           if (price != prev_price){
             _pricelevel[price] = it;
           }
          */
         ++it;
         cancellations = cancellations || cancelling;
       }
   }
   return cancellations||deletions;
}
void OrderQueue::sort(){
  if (!is_empty()){
    std::lock_guard<std::mutex> lock(_mtx);
    std::sort(
        this->begin(), 
        this->end(), 
        [](std::shared_ptr<Order> a, std::shared_ptr<Order> b) {return *a < *b;}
    );
  }
}
void OrderQueue::renew_bestprice(){
  std::lock_guard<std::mutex> lock(_mtx);
  //write("OrderQueue::renew_bestprice() _direction: %d\n", _direction);
  if (this->empty()){
    _bestprice.invalidate();
  }
  else{
    for (auto it = this->begin(); it!= this->end(); ++it){
      std::shared_ptr<Order>& order = *it;
      if (order){
        if ((order->status()==OrderStatus::queued)&&(order->size()>0)){
          _bestprice.new_(order);
          break;
        }
      }
    }
  }
}
void OrderQueue::erase_notqueued(){
  std::lock_guard<std::mutex> lock(_mtx);
  //write("OrderQueue::erase_notqueued() _direction: %d\n", _direction);
  int count{0};
  if (!(this->empty())){
    for (auto it = this->begin(); it!= this->end();){
      if ( (*it)->status()!=OrderStatus::queued){
        erase_order(it);
        count++;
      }
      else{
        ++it;
      }
    }
    //write("%d orders have been erased from queue %d\n", count, _direction);
  }
}
bool OrderQueue::is_sorted(){
  //write("OrderQueue::is_sorted() _direction: %d\n", _direction);
  //std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::lock_guard<std::mutex> lock(_mtx);
  if (!(this->empty())){
    auto it = this->begin();
    while (it!=this->end()){
      Order& current = **it;
      ++it;
      if (it!=this->end()){
        Order& next = **it;
        if (!(current<next)){
          write("OrderQueue %d is not sorted. \n", _direction );
          return false;
        }
      }
    }
  }
  return true;
}
bool OrderQueue::is_empty(){
  bool isempty=false;
  std::lock_guard<std::mutex> lock(_mtx);
  if (this->empty()){
    isempty=true;
    invalidate_bestprice();
    /*
    std::string side = (_direction==OrderDirection::bid)? "Bid" : "Ask";
    write("%s queue is empty\n", side.c_str());
    */
  }
  return isempty;
}

OrderBook::OrderBook(int ticksize, int opening_bidprice, int opening_askprice): 
  _ticksize(ticksize) 
{
  opening_bidprice = std::max(10*ticksize, opening_bidprice);
  opening_askprice = std::max(opening_bidprice, opening_askprice);
  _queue = std::map<OrderDirection, OrderQueue>  {
        {OrderDirection::bid, OrderQueue(OrderDirection::bid, opening_bidprice, &_mtx, &_cond)},
        {OrderDirection::ask, OrderQueue(OrderDirection::ask, opening_askprice, &_mtx, &_cond)},
  };
}
OrderBook::OrderBook(int ticksize, int opening_bidprice, int opening_askprice, std::shared_ptr<SubWindow> subwin): 
   OrderBook(ticksize, opening_bidprice, opening_askprice)
{
     set_subwindow(subwin);
}
OrderBook::~OrderBook(){
  //printf("OrderBook Destructor\n");
}
void OrderBook::set_subwindow(std::shared_ptr<SubWindow> subwin){
  _subwindow = subwin;
  for (auto d : all_directions)
    _queue.at(d).set_subwindow(subwin);
}
void OrderBook::add(std::shared_ptr<Order>&& order){
    if (order){
        std::lock_guard<std::mutex> lock(_mtx);
        write("Order #%d is being added on %s side; price=%d, size=%d\n", order->id(), order->side().c_str(), order->price(), order->active_size());
        _queue.at(opposite(order->direction())).execute(order);
        _queue.at(order->direction()).add(std::move(order));
        _cond.notify_all();
    }
}
void OrderBook::add(std::shared_ptr<StandingOrder>&& order, double decayrate){
  if (order){
    order->set_decay(this,decayrate);
    add(std::move(order));
  }
}
void OrderBook::add(OrderCommand&& cmd){
  add(std::make_shared<Order>(cmd.price, cmd.size, cmd.d, _subwindow));
}
void OrderBook::launch_simulation(){
  for (OrderDirection d : all_directions){
    _threads.emplace_back(std::thread(&OrderQueue::simulate, &(_queue.at(d))));
  }
}
void OrderBook::stop(){
  for (OrderDirection d : all_directions){
    _queue.at(d).stop();
  }
  _cond.notify_all();
}
BestPrice::BestPrice(OrderDirection d, int initial_price): _direction(d), _cached(initial_price){
}
BestPrice::BestPrice(const BestPrice& source): _direction(source.direction()), _price(source.price()), _cached(source.cached_price())
{
    _order=source.order();
}
BestPrice::BestPrice(BestPrice&& source): _direction(source.direction()), _price(source.price()), _cached(source.cached_price())
{
    _order=source.order();
}
BestPrice::~BestPrice(){
  ////printf("BestPrice Destructor; _direction = %d\n", _direction);
} 
BestPrice& BestPrice::operator=(const BestPrice& source){
  _direction = source.direction();
  _price = source.price();
  _cached = source.cached_price();
  _order = source.order();
  return *this;
}
BestPrice& BestPrice::operator=(BestPrice&& source){
  _direction = source.direction();
  _price = source.price();
  _cached = source.cached_price();
  _order = source.order();
  return *this;
}
int BestPrice::value() const {
  return (_order)? _price : _cached;
}
void BestPrice::invalidate(){
  _order.reset();
  _price=(_direction==OrderDirection::bid)? 0 : 0;
}
bool BestPrice::is_better_than(std::shared_ptr<Order> other){
  if (!(other)) return true;
  return (_order)? (*_order<*other) : false;
}
void BestPrice::update(std::shared_ptr<Order> order){
  if (order){
    if ((order->status()==OrderStatus::queued)&&(order->size()>0)){
      if (order->direction() == _direction){
        if (! this->is_better_than(order) ){
          std::lock_guard<std::mutex> lock(*(order->mtx()));
          _price = order->price();
          _cached = _price;
          _order = order;
          //write("BestPrice::update() direction=%d, price=%d\n", _direction, _price);
        }
      }
    }
  }
}
void BestPrice::new_(std::shared_ptr<Order> order){
  _order.reset();
  update(order);
}
int OrderBook::askprice()  {
  return  _queue.at(OrderDirection::ask).bestprice()->value();
}
int OrderBook::bidprice()  {
  return  _queue.at(OrderDirection::bid).bestprice()->value();
}
long OrderBook::askvolume() {
  return  _queue.at(OrderDirection::ask).volume();
}
long OrderBook::bidvolume() {
  return  _queue.at(OrderDirection::bid).volume();
}
std::random_device rd;
std::default_random_engine OrderSubmission::generator(rd());
std::discrete_distribution<int> OrderSubmission::distribution({0.05,0.35,0.40,0.05,0.05,0.05,0.05});
OrderSubmission::OrderSubmission(std::shared_ptr<OrderBook> ob, double decayrate): hawkes(HawkesProcess(2)), _decayrate(decayrate){
  //write("OrderSubmission Constructor\n");
  _book = ob;
  init_user();
}
OrderSubmission::~OrderSubmission(){
  //printf("OrderSubmission Destructor\n");
}
void OrderSubmission::init_user(){
  _user = std::make_shared<User>(_book);
  _user->set_ordersubmission(this);
}
void OrderSubmission::set_subwindow(std::shared_ptr<SubWindow> sw){
  hawkes.set_subwindow(sw);
  _subwindow = sw;
}
void OrderSubmission::set_userwindow(std::shared_ptr<UserWindow> uw){
  _user->set_userwindow(uw);
}
OrderCommand OrderSubmission::generate_command(Event&& event){
  OrderCommand cmd;
  cmd.d = (event.type==0)? OrderDirection::bid : OrderDirection::ask;
  int level = -2+distribution(generator);
  int bestprice = (cmd.d == OrderDirection::bid)? _book->bidprice() : _book->askprice();
  cmd.price =  std::max(0, bestprice - (cmd.d)*(_book->ticksize())*level);
  cmd.size = std::max(0, 50*(1+distribution(generator)));
  //write("OrderSubmission::generate_command side=%d, level=%d, price=%d, size=%d\n", cmd.d, level, cmd.price, cmd.size);
  return cmd;
}
void OrderSubmission::submit(int price, int size, OrderDirection d){
  _book->add(std::make_shared<Order>(price,size,d, _subwindow));
}
void OrderSubmission::submit(OrderCommand cmd){
  submit(cmd.price, cmd.size, cmd.d);
}
void OrderSubmission::simulate(){
  //write("OrderSubmission::simulate()\n");
  while(this->go()){
    OrderCommand cmd = generate_command(hawkes.wait_for_event());
    _book->add(std::make_shared<StandingOrder>(cmd.price, cmd.size, cmd.d, _subwindow ), _decayrate);
  }
}
void OrderSubmission::launch_simulation(){
  _threads.emplace_back(std::thread(&HawkesProcess::simulate, &hawkes));
  _threads.emplace_back(std::thread(&OrderSubmission::simulate, this));
  _threads.emplace_back(std::thread(&User::simulate, _user.get()));
}
void OrderSubmission::stop(){
  hawkes.stop();
  _book->stop();
  _user->stop();
  {
    std::lock_guard<std::mutex> lock(_gomtx);
    _go=false;
  }
}
