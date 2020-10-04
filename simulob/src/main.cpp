#include<stdio.h>
#include<ncurses.h>
#include<cassert>
#include<memory>
#include<thread>
#include<chrono>
#include<map>
#include"gui.h"
#include"orderbook.h"
#include"user.h"
#include<algorithm>
int main (){
  std::shared_ptr<OrderBook> ob = std::make_shared<OrderBook>(100, 100000, 100500);
  double decayrate=0.000000001;
  std::shared_ptr<OrderSubmission> osub = std::make_shared<OrderSubmission>(ob, decayrate);
  osub->hawkes.baserate(std::vector<double>{0.003,0.003});
  osub->hawkes.impactcoef(std::vector<std::vector<double>>{{0.005,0.003},{0.003,0.005}});
  osub->hawkes.decaycoef(std::vector<std::vector<double>>{{3.5,3.5},{3.5,3.5}});
  osub->hawkes.set_timeorigin(0.0);
  {
    int status_y=1;
    int user_y = 7;
    int sparrow_y= 4;
    IntPanel interface = Interface::setup(status_y, sparrow_y, user_y, osub->user());
    Interface::setup(interface, ob, osub);
    Interface::init(interface);
    std::thread t(Interface::display, std::ref(interface));
    ob->launch_simulation();
    osub->launch_simulation();
    t.join();
  }
  osub->join();
  ob->join();
  Interface::wait_for_F1();
  Interface::close();
  return 0;
}
